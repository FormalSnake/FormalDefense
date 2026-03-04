// fd_gfx_sokol.c — Sokol implementation of fd_gfx.h
#include "fd_gfx.h"
#include "fd_app.h"
#include "sokol_gfx.h"
#include "sokol_app.h"
#include "sokol_glue.h"
#include "sokol_gl.h"
#include "sokol_debugtext.h"
#include "sokol_shape.h"

#include "shaders/ps1.glsl.h"
#include "shaders/fullscreen.glsl.h"
#include "shaders/unlit.glsl.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

// --- Internal state ---

static struct {
    // PS1 shader pipeline
    sg_shader ps1_shd;
    sg_pipeline ps1_pip;

    // Unlit shader pipeline (skybox)
    sg_shader unlit_shd;
    sg_pipeline unlit_pip;

    // Fullscreen blit
    sg_shader fullscreen_shd;
    sg_pipeline fullscreen_pip;
    sg_sampler point_smp;

    // Current 3D view/proj matrices
    FdMat4 view;
    FdMat4 proj;
    FdMat4 vp;
    bool in3D;

    // PS1 shader params
    vs_params_t vs_params;
    fs_params_t fs_params;

    // Sphere mesh
    sg_buffer sphere_vbuf;
    sg_buffer sphere_ibuf;
    int sphere_num_elements;
    bool sphere_ready;

    // Current pass action
    sg_pass_action pass_action;

    // FPS tracking
    float fps_timer;
    int fps_frame_count;
    int fps_display;
} gfx;

// --- Render target ---

struct FdRenderTarget {
    sg_image color;
    sg_image depth;
    sg_view color_att_view;
    sg_view depth_att_view;
    sg_view tex_view;
    int w, h;
};

// --- Mesh ---

struct FdMesh {
    sg_buffer vbuf;   // interleaved: position(3f) + color(4ub) + normal(3f)
    int vert_count;
    bool has_normals;
};

// --- Lifecycle ---

void FdGfxInit(void) {
    // PS1 shader
    gfx.ps1_shd = sg_make_shader(ps1_shader_desc(sg_query_backend()));
    {
        sg_pipeline_desc pd = {0};
        pd.shader = gfx.ps1_shd;
        pd.layout.attrs[ATTR_ps1_position].format = SG_VERTEXFORMAT_FLOAT3;
        pd.layout.attrs[ATTR_ps1_color0].format   = SG_VERTEXFORMAT_UBYTE4N;
        pd.layout.attrs[ATTR_ps1_normal].format    = SG_VERTEXFORMAT_FLOAT3;
        pd.depth.write_enabled = true;
        pd.depth.compare = SG_COMPAREFUNC_LESS_EQUAL;
        pd.cull_mode = SG_CULLMODE_BACK;
        pd.face_winding = SG_FACEWINDING_CCW;
        pd.label = "ps1-pipeline";
        gfx.ps1_pip = sg_make_pipeline(&pd);
    }

    // Unlit shader (for skybox)
    gfx.unlit_shd = sg_make_shader(unlit_shader_desc(sg_query_backend()));
    {
        sg_pipeline_desc pd = {0};
        pd.shader = gfx.unlit_shd;
        pd.layout.attrs[ATTR_unlit_position].format = SG_VERTEXFORMAT_FLOAT3;
        pd.layout.attrs[ATTR_unlit_color0].format   = SG_VERTEXFORMAT_UBYTE4N;
        // Skybox: no depth write, no backface culling
        pd.depth.write_enabled = false;
        pd.depth.compare = SG_COMPAREFUNC_LESS_EQUAL;
        pd.cull_mode = SG_CULLMODE_NONE;
        pd.label = "unlit-pipeline";
        gfx.unlit_pip = sg_make_pipeline(&pd);
    }

    // Fullscreen blit shader
    gfx.fullscreen_shd = sg_make_shader(fullscreen_shader_desc(sg_query_backend()));
    {
        sg_pipeline_desc pd = {0};
        pd.shader = gfx.fullscreen_shd;
        pd.depth.write_enabled = false;
        pd.depth.compare = SG_COMPAREFUNC_ALWAYS;
        pd.cull_mode = SG_CULLMODE_NONE;
        pd.label = "fullscreen-pipeline";
        gfx.fullscreen_pip = sg_make_pipeline(&pd);
    }

    // Point-filtered sampler for render target blit
    {
        sg_sampler_desc sd = {0};
        sd.min_filter = SG_FILTER_NEAREST;
        sd.mag_filter = SG_FILTER_NEAREST;
        sd.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
        sd.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
        gfx.point_smp = sg_make_sampler(&sd);
    }

    // Init sokol_gl for immediate-mode 2D/3D drawing
    sgl_desc_t sgl_desc = {0};
    sgl_desc.max_vertices = 65536;
    sgl_desc.max_commands = 4096;
    sgl_setup(&sgl_desc);

    // Init sokol_debugtext for text rendering
    sdtx_desc_t sdtx_desc = {0};
    sdtx_desc.fonts[0] = sdtx_font_c64();
    sdtx_desc.context_pool_size = 1;
    sdtx_setup(&sdtx_desc);

    // Default PS1 shader params
    gfx.fs_params.light_dir = (HMM_Vec3){{0.4f, -0.7f, 0.3f}};
    gfx.fs_params.light_color = (HMM_Vec3){{0.7f, 0.7f, 0.65f}};
    gfx.fs_params.ambient_color = (HMM_Vec3){{0.3f, 0.3f, 0.35f}};
    gfx.fs_params.color_bands = 24.0f;
    gfx.vs_params.jitter_strength = 1.0f;
}

void FdGfxShutdown(void) {
    sdtx_shutdown();
    sgl_shutdown();

    if (gfx.sphere_ready) {
        sg_destroy_buffer(gfx.sphere_vbuf);
        sg_destroy_buffer(gfx.sphere_ibuf);
    }

    sg_destroy_sampler(gfx.point_smp);
    sg_destroy_pipeline(gfx.fullscreen_pip);
    sg_destroy_shader(gfx.fullscreen_shd);
    sg_destroy_pipeline(gfx.unlit_pip);
    sg_destroy_shader(gfx.unlit_shd);
    sg_destroy_pipeline(gfx.ps1_pip);
    sg_destroy_shader(gfx.ps1_shd);
}

// --- Render targets ---

FdRenderTarget *FdRenderTargetCreate(int w, int h) {
    FdRenderTarget *rt = calloc(1, sizeof(FdRenderTarget));
    rt->w = w;
    rt->h = h;

    sg_image_desc cd = {0};
    cd.usage.color_attachment = true;
    cd.usage.immutable = false;
    cd.width = w;
    cd.height = h;
    cd.pixel_format = SG_PIXELFORMAT_BGRA8;
    cd.label = "rt-color";
    rt->color = sg_make_image(&cd);

    sg_image_desc dd = {0};
    dd.usage.depth_stencil_attachment = true;
    dd.usage.immutable = false;
    dd.width = w;
    dd.height = h;
    dd.pixel_format = SG_PIXELFORMAT_DEPTH_STENCIL;
    dd.label = "rt-depth";
    rt->depth = sg_make_image(&dd);

    rt->color_att_view = sg_make_view(&(sg_view_desc){
        .color_attachment.image = rt->color,
        .label = "rt-color-att-view",
    });
    rt->depth_att_view = sg_make_view(&(sg_view_desc){
        .depth_stencil_attachment.image = rt->depth,
        .label = "rt-depth-att-view",
    });
    rt->tex_view = sg_make_view(&(sg_view_desc){
        .texture.image = rt->color,
        .label = "rt-tex-view",
    });

    return rt;
}

void FdRenderTargetDestroy(FdRenderTarget *rt) {
    if (!rt) return;
    sg_destroy_view(rt->tex_view);
    sg_destroy_view(rt->depth_att_view);
    sg_destroy_view(rt->color_att_view);
    sg_destroy_image(rt->depth);
    sg_destroy_image(rt->color);
    free(rt);
}

void FdRenderTargetBegin(FdRenderTarget *rt, Color clear) {
    sg_pass pass = {0};
    pass.attachments.colors[0] = rt->color_att_view;
    pass.attachments.depth_stencil = rt->depth_att_view;
    pass.action.colors[0].load_action = SG_LOADACTION_CLEAR;
    pass.action.colors[0].clear_value = (sg_color){
        clear.r / 255.0f, clear.g / 255.0f, clear.b / 255.0f, clear.a / 255.0f
    };
    pass.action.depth.load_action = SG_LOADACTION_CLEAR;
    pass.action.depth.clear_value = 1.0f;
    sg_begin_pass(&pass);
}

void FdRenderTargetEnd(void) {
    sgl_draw();
    sg_end_pass();
}

void FdRenderTargetBlit(FdRenderTarget *rt, int dstW, int dstH) {
    (void)dstW; (void)dstH;
    sg_apply_pipeline(gfx.fullscreen_pip);
    sg_bindings bind = {0};
    bind.views[VIEW_tex] = rt->tex_view;
    bind.samplers[SMP_smp] = gfx.point_smp;
    sg_apply_bindings(&bind);
    sg_draw(0, 3, 1);  // fullscreen triangle
}

// --- Frame ---

void FdBeginFrame(Color clear) {
    // FPS tracking
    gfx.fps_frame_count++;
    gfx.fps_timer += sapp_frame_duration();
    if (gfx.fps_timer >= 1.0f) {
        gfx.fps_display = gfx.fps_frame_count;
        gfx.fps_frame_count = 0;
        gfx.fps_timer -= 1.0f;
    }

    sg_pass pass = {0};
    pass.swapchain = sglue_swapchain();
    pass.action.colors[0].load_action = SG_LOADACTION_CLEAR;
    pass.action.colors[0].clear_value = (sg_color){
        clear.r / 255.0f, clear.g / 255.0f, clear.b / 255.0f, clear.a / 255.0f
    };
    pass.action.depth.load_action = SG_LOADACTION_CLEAR;
    pass.action.depth.clear_value = 1.0f;
    sg_begin_pass(&pass);

    sdtx_canvas((float)sapp_width() * 0.5f, (float)sapp_height() * 0.5f);
}

void FdEndFrame(void) {
    sdtx_draw();
    sgl_draw();
    sg_end_pass();
    sg_commit();
}

// --- 3D mode ---

void FdBegin3D(FdMat4 view, FdMat4 proj) {
    gfx.view = view;
    gfx.proj = proj;
    gfx.vp = HMM_MulM4(proj, view);
    gfx.in3D = true;

    // Set up sgl for 3D immediate mode
    sgl_defaults();
    sgl_matrix_mode_projection();
    sgl_load_matrix((const float *)&proj);
    sgl_matrix_mode_modelview();
    sgl_load_matrix((const float *)&view);
}

void FdEnd3D(void) {
    gfx.in3D = false;
}

// --- Mesh ---

// Interleaved vertex format: float3 pos + ubyte4n color + float3 normal = 28 bytes
#define VERT_STRIDE 28

FdMesh *FdMeshCreate(const float *positions, const unsigned char *colors, const float *normals, int vertCount) {
    FdMesh *m = calloc(1, sizeof(FdMesh));
    m->vert_count = vertCount;
    m->has_normals = (normals != NULL);

    // Interleave vertex data
    int bufSize = vertCount * VERT_STRIDE;
    unsigned char *buf = malloc(bufSize);

    for (int i = 0; i < vertCount; i++) {
        float *dst = (float *)(buf + i * VERT_STRIDE);
        // Position
        dst[0] = positions[i * 3 + 0];
        dst[1] = positions[i * 3 + 1];
        dst[2] = positions[i * 3 + 2];
        // Color (4 bytes after 12 bytes of position)
        unsigned char *cdst = (unsigned char *)(dst + 3);
        if (colors) {
            cdst[0] = colors[i * 4 + 0];
            cdst[1] = colors[i * 4 + 1];
            cdst[2] = colors[i * 4 + 2];
            cdst[3] = colors[i * 4 + 3];
        } else {
            cdst[0] = cdst[1] = cdst[2] = cdst[3] = 255;
        }
        // Normal (12 bytes after color)
        float *ndst = (float *)(cdst + 4);
        if (normals) {
            ndst[0] = normals[i * 3 + 0];
            ndst[1] = normals[i * 3 + 1];
            ndst[2] = normals[i * 3 + 2];
        } else {
            ndst[0] = 0.0f;
            ndst[1] = 1.0f;
            ndst[2] = 0.0f;
        }
    }

    sg_buffer_desc bd = {0};
    bd.data = (sg_range){ buf, bufSize };
    bd.label = "fd-mesh";
    m->vbuf = sg_make_buffer(&bd);

    free(buf);
    return m;
}

void FdMeshDestroy(FdMesh *mesh) {
    if (!mesh) return;
    sg_destroy_buffer(mesh->vbuf);
    free(mesh);
}

// --- Sphere mesh ---

void FdSphereMeshInit(void) {
    if (gfx.sphere_ready) return;

    sshape_vertex_t vertices[1024];
    uint16_t indices[4096];

    sshape_buffer_t buf = {0};
    buf.vertices.buffer = SSHAPE_RANGE(vertices);
    buf.indices.buffer = SSHAPE_RANGE(indices);

    buf = sshape_build_sphere(&buf, &(sshape_sphere_t){
        .radius = 1.0f,
        .slices = 8,
        .stacks = 8,
        .random_colors = false,
    });

    sg_buffer_desc vbd = sshape_vertex_buffer_desc(&buf);
    vbd.label = "sphere-vbuf";
    gfx.sphere_vbuf = sg_make_buffer(&vbd);

    sg_buffer_desc ibd = sshape_index_buffer_desc(&buf);
    ibd.label = "sphere-ibuf";
    gfx.sphere_ibuf = sg_make_buffer(&ibd);

    gfx.sphere_num_elements = sshape_element_range(&buf).num_elements;
    gfx.sphere_ready = true;
}

void FdSphereMeshShutdown(void) {
    if (!gfx.sphere_ready) return;
    sg_destroy_buffer(gfx.sphere_vbuf);
    sg_destroy_buffer(gfx.sphere_ibuf);
    gfx.sphere_ready = false;
}

// --- 3D draw calls ---

void FdDrawMesh(const FdMesh *mesh, FdMat4 model, bool usePS1Shader) {
    if (!mesh) return;

    if (usePS1Shader) {
        sg_apply_pipeline(gfx.ps1_pip);
        gfx.vs_params.mvp = HMM_MulM4(gfx.vp, model);
        gfx.vs_params.model = model;
        sg_apply_uniforms(UB_vs_params, &SG_RANGE(gfx.vs_params));
        sg_apply_uniforms(UB_fs_params, &SG_RANGE(gfx.fs_params));
    } else {
        sg_apply_pipeline(gfx.unlit_pip);
        vs_unlit_params_t up = { .mvp = HMM_MulM4(gfx.vp, model) };
        sg_apply_uniforms(UB_vs_unlit_params, &SG_RANGE(up));
    }

    sg_bindings bind = {0};
    bind.vertex_buffers[0] = mesh->vbuf;
    sg_apply_bindings(&bind);
    sg_draw(0, mesh->vert_count, 1);
}

void FdDrawCube(Vector3 pos, Vector3 size, Color color) {
    float hx = size.x * 0.5f, hy = size.y * 0.5f, hz = size.z * 0.5f;

    sgl_begin_triangles();
    sgl_c4b(color.r, color.g, color.b, color.a);

    // Front face
    sgl_v3f(pos.x-hx, pos.y-hy, pos.z+hz); sgl_v3f(pos.x+hx, pos.y-hy, pos.z+hz); sgl_v3f(pos.x+hx, pos.y+hy, pos.z+hz);
    sgl_v3f(pos.x-hx, pos.y-hy, pos.z+hz); sgl_v3f(pos.x+hx, pos.y+hy, pos.z+hz); sgl_v3f(pos.x-hx, pos.y+hy, pos.z+hz);
    // Back face
    sgl_v3f(pos.x+hx, pos.y-hy, pos.z-hz); sgl_v3f(pos.x-hx, pos.y-hy, pos.z-hz); sgl_v3f(pos.x-hx, pos.y+hy, pos.z-hz);
    sgl_v3f(pos.x+hx, pos.y-hy, pos.z-hz); sgl_v3f(pos.x-hx, pos.y+hy, pos.z-hz); sgl_v3f(pos.x+hx, pos.y+hy, pos.z-hz);
    // Top face
    sgl_v3f(pos.x-hx, pos.y+hy, pos.z-hz); sgl_v3f(pos.x-hx, pos.y+hy, pos.z+hz); sgl_v3f(pos.x+hx, pos.y+hy, pos.z+hz);
    sgl_v3f(pos.x-hx, pos.y+hy, pos.z-hz); sgl_v3f(pos.x+hx, pos.y+hy, pos.z+hz); sgl_v3f(pos.x+hx, pos.y+hy, pos.z-hz);
    // Bottom face
    sgl_v3f(pos.x-hx, pos.y-hy, pos.z+hz); sgl_v3f(pos.x-hx, pos.y-hy, pos.z-hz); sgl_v3f(pos.x+hx, pos.y-hy, pos.z-hz);
    sgl_v3f(pos.x-hx, pos.y-hy, pos.z+hz); sgl_v3f(pos.x+hx, pos.y-hy, pos.z-hz); sgl_v3f(pos.x+hx, pos.y-hy, pos.z+hz);
    // Right face
    sgl_v3f(pos.x+hx, pos.y-hy, pos.z+hz); sgl_v3f(pos.x+hx, pos.y-hy, pos.z-hz); sgl_v3f(pos.x+hx, pos.y+hy, pos.z-hz);
    sgl_v3f(pos.x+hx, pos.y-hy, pos.z+hz); sgl_v3f(pos.x+hx, pos.y+hy, pos.z-hz); sgl_v3f(pos.x+hx, pos.y+hy, pos.z+hz);
    // Left face
    sgl_v3f(pos.x-hx, pos.y-hy, pos.z-hz); sgl_v3f(pos.x-hx, pos.y-hy, pos.z+hz); sgl_v3f(pos.x-hx, pos.y+hy, pos.z+hz);
    sgl_v3f(pos.x-hx, pos.y-hy, pos.z-hz); sgl_v3f(pos.x-hx, pos.y+hy, pos.z+hz); sgl_v3f(pos.x-hx, pos.y+hy, pos.z-hz);

    sgl_end();
}

void FdDrawCubeWires(Vector3 pos, Vector3 size, Color color) {
    float hx = size.x * 0.5f, hy = size.y * 0.5f, hz = size.z * 0.5f;

    sgl_begin_lines();
    sgl_c4b(color.r, color.g, color.b, color.a);

    // Bottom edges
    sgl_v3f(pos.x-hx, pos.y-hy, pos.z-hz); sgl_v3f(pos.x+hx, pos.y-hy, pos.z-hz);
    sgl_v3f(pos.x+hx, pos.y-hy, pos.z-hz); sgl_v3f(pos.x+hx, pos.y-hy, pos.z+hz);
    sgl_v3f(pos.x+hx, pos.y-hy, pos.z+hz); sgl_v3f(pos.x-hx, pos.y-hy, pos.z+hz);
    sgl_v3f(pos.x-hx, pos.y-hy, pos.z+hz); sgl_v3f(pos.x-hx, pos.y-hy, pos.z-hz);
    // Top edges
    sgl_v3f(pos.x-hx, pos.y+hy, pos.z-hz); sgl_v3f(pos.x+hx, pos.y+hy, pos.z-hz);
    sgl_v3f(pos.x+hx, pos.y+hy, pos.z-hz); sgl_v3f(pos.x+hx, pos.y+hy, pos.z+hz);
    sgl_v3f(pos.x+hx, pos.y+hy, pos.z+hz); sgl_v3f(pos.x-hx, pos.y+hy, pos.z+hz);
    sgl_v3f(pos.x-hx, pos.y+hy, pos.z+hz); sgl_v3f(pos.x-hx, pos.y+hy, pos.z-hz);
    // Vertical edges
    sgl_v3f(pos.x-hx, pos.y-hy, pos.z-hz); sgl_v3f(pos.x-hx, pos.y+hy, pos.z-hz);
    sgl_v3f(pos.x+hx, pos.y-hy, pos.z-hz); sgl_v3f(pos.x+hx, pos.y+hy, pos.z-hz);
    sgl_v3f(pos.x+hx, pos.y-hy, pos.z+hz); sgl_v3f(pos.x+hx, pos.y+hy, pos.z+hz);
    sgl_v3f(pos.x-hx, pos.y-hy, pos.z+hz); sgl_v3f(pos.x-hx, pos.y+hy, pos.z+hz);

    sgl_end();
}

void FdDrawSphere(Vector3 pos, float radius, Color color) {
    // Draw sphere using sgl immediate mode (low-poly fits PS1 aesthetic)
    int slices = 8, stacks = 8;

    sgl_begin_triangles();
    sgl_c4b(color.r, color.g, color.b, color.a);

    for (int i = 0; i < stacks; i++) {
        float phi0 = PI * (float)i / stacks;
        float phi1 = PI * (float)(i + 1) / stacks;

        for (int j = 0; j < slices; j++) {
            float theta0 = 2.0f * PI * (float)j / slices;
            float theta1 = 2.0f * PI * (float)(j + 1) / slices;

            float x00 = pos.x + radius * sinf(phi0) * cosf(theta0);
            float y00 = pos.y + radius * cosf(phi0);
            float z00 = pos.z + radius * sinf(phi0) * sinf(theta0);
            float x10 = pos.x + radius * sinf(phi1) * cosf(theta0);
            float y10 = pos.y + radius * cosf(phi1);
            float z10 = pos.z + radius * sinf(phi1) * sinf(theta0);
            float x01 = pos.x + radius * sinf(phi0) * cosf(theta1);
            float y01 = pos.y + radius * cosf(phi0);
            float z01 = pos.z + radius * sinf(phi0) * sinf(theta1);
            float x11 = pos.x + radius * sinf(phi1) * cosf(theta1);
            float y11 = pos.y + radius * cosf(phi1);
            float z11 = pos.z + radius * sinf(phi1) * sinf(theta1);

            sgl_v3f(x00, y00, z00);
            sgl_v3f(x10, y10, z10);
            sgl_v3f(x11, y11, z11);

            sgl_v3f(x00, y00, z00);
            sgl_v3f(x11, y11, z11);
            sgl_v3f(x01, y01, z01);
        }
    }

    sgl_end();
}

void FdDrawLine3D(Vector3 a, Vector3 b, Color color) {
    sgl_begin_lines();
    sgl_c4b(color.r, color.g, color.b, color.a);
    sgl_v3f(a.x, a.y, a.z);
    sgl_v3f(b.x, b.y, b.z);
    sgl_end();
}

// --- Immediate-mode triangles ---

void FdBeginTriangles(void) {
    sgl_begin_triangles();
}

void FdTriVertex3f(float x, float y, float z) {
    sgl_v3f(x, y, z);
}

void FdTriColor4ub(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    sgl_c4b(r, g, b, a);
}

void FdEndTriangles(void) {
    sgl_end();
}

// --- GPU state ---

void FdDisableBackfaceCulling(void) {
    // Handled via pipeline state; for sgl:
    // sgl doesn't have per-draw cull mode, but we can manage via separate pipelines
    // For now, this is a no-op since skybox uses unlit_pip which has CULLMODE_NONE
}

void FdEnableBackfaceCulling(void) {
    // No-op (see above)
}

void FdDisableDepthWrite(void) {
    // Managed via pipeline
}

void FdEnableDepthWrite(void) {
    // Managed via pipeline
}

// --- PS1 shader params ---

void FdPS1ShaderSetParams(float jitter, float bands, Vector3 lightDir, Vector3 lightColor, Vector3 ambient) {
    gfx.vs_params.jitter_strength = jitter;
    gfx.fs_params.color_bands = bands;
    gfx.fs_params.light_dir = _toHMM3(lightDir);
    gfx.fs_params.light_color = _toHMM3(lightColor);
    gfx.fs_params.ambient_color = _toHMM3(ambient);
}

void FdPS1ShaderSetResolution(float w, float h) {
    gfx.vs_params.resolution = (HMM_Vec2){w, h};
}

// --- 2D drawing ---

void FdDrawRect(int x, int y, int w, int h, Color color) {
    sgl_defaults();
    sgl_matrix_mode_projection();
    sgl_ortho(0.0f, (float)sapp_width(), (float)sapp_height(), 0.0f, -1.0f, 1.0f);
    sgl_matrix_mode_modelview();
    sgl_load_identity();

    sgl_begin_triangles();
    sgl_c4b(color.r, color.g, color.b, color.a);
    float fx = (float)x, fy = (float)y, fw = (float)w, fh = (float)h;
    sgl_v2f(fx, fy);       sgl_v2f(fx+fw, fy);     sgl_v2f(fx+fw, fy+fh);
    sgl_v2f(fx, fy);       sgl_v2f(fx+fw, fy+fh);  sgl_v2f(fx, fy+fh);
    sgl_end();
}

void FdDrawRectLines(int x, int y, int w, int h, Color color) {
    FdDrawRectLinesEx(x, y, w, h, 1, color);
}

void FdDrawRectLinesEx(int x, int y, int w, int h, int thick, Color color) {
    FdDrawRect(x, y, w, thick, color);             // top
    FdDrawRect(x, y + h - thick, w, thick, color);  // bottom
    FdDrawRect(x, y, thick, h, color);              // left
    FdDrawRect(x + w - thick, y, thick, h, color);   // right
}

void FdDrawText(const char *text, int x, int y, int size, Color color) {
    // Scale factor: debugtext base is 8x8 pixels
    float scale = (float)size / 8.0f;
    sdtx_color4b(color.r, color.g, color.b, color.a);
    // Convert pixel position to debugtext grid coords
    // Canvas is set to half screen resolution, characters are 8 pixels
    float canvasW = (float)sapp_width() * 0.5f;
    float canvasH = (float)sapp_height() * 0.5f;
    float charW = canvasW / (canvasW / 8.0f);   // = 8
    float charH = canvasH / (canvasH / 8.0f);   // = 8
    (void)charW; (void)charH;
    sdtx_pos((float)x / (8.0f * scale), (float)y / (8.0f * scale));
    sdtx_font(0);
    sdtx_puts(text);
}

int FdMeasureText(const char *text, int size) {
    // Each character is 8 pixels wide at base size
    int len = (int)strlen(text);
    float scale = (float)size / 8.0f;
    return (int)(len * 8.0f * scale);
}

void FdDrawFPS(int x, int y) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d FPS", gfx.fps_display);
    FdDrawText(buf, x, y, 20, GREEN);
}

void FdDrawLine2D(int x1, int y1, int x2, int y2, Color color) {
    sgl_defaults();
    sgl_matrix_mode_projection();
    sgl_ortho(0.0f, (float)sapp_width(), (float)sapp_height(), 0.0f, -1.0f, 1.0f);
    sgl_matrix_mode_modelview();
    sgl_load_identity();

    sgl_begin_lines();
    sgl_c4b(color.r, color.g, color.b, color.a);
    sgl_v2f((float)x1, (float)y1);
    sgl_v2f((float)x2, (float)y2);
    sgl_end();
}

void FdDrawCircle2D(int cx, int cy, int radius, Color color) {
    sgl_defaults();
    sgl_matrix_mode_projection();
    sgl_ortho(0.0f, (float)sapp_width(), (float)sapp_height(), 0.0f, -1.0f, 1.0f);
    sgl_matrix_mode_modelview();
    sgl_load_identity();

    int segs = 16;
    sgl_begin_triangles();
    sgl_c4b(color.r, color.g, color.b, color.a);
    for (int i = 0; i < segs; i++) {
        float a0 = 2.0f * PI * (float)i / segs;
        float a1 = 2.0f * PI * (float)(i + 1) / segs;
        sgl_v2f((float)cx, (float)cy);
        sgl_v2f((float)cx + radius * cosf(a0), (float)cy + radius * sinf(a0));
        sgl_v2f((float)cx + radius * cosf(a1), (float)cy + radius * sinf(a1));
    }
    sgl_end();
}

// --- Projection helpers ---

Ray FdScreenToWorldRay(Vector2 mousePos, FdMat4 view, FdMat4 proj, int screenW, int screenH) {
    // Convert screen coords to NDC
    float ndcX = (2.0f * mousePos.x / (float)screenW) - 1.0f;
    float ndcY = 1.0f - (2.0f * mousePos.y / (float)screenH);

    // Unproject near and far points
    FdMat4 vpInv = HMM_InvGeneralM4(HMM_MulM4(proj, view));

    HMM_Vec4 nearPt = HMM_MulM4V4(vpInv, (HMM_Vec4){{ndcX, ndcY, -1.0f, 1.0f}});
    HMM_Vec4 farPt  = HMM_MulM4V4(vpInv, (HMM_Vec4){{ndcX, ndcY,  1.0f, 1.0f}});

    nearPt = HMM_MulV4F(nearPt, 1.0f / nearPt.W);
    farPt  = HMM_MulV4F(farPt,  1.0f / farPt.W);

    Vector3 origin = { nearPt.X, nearPt.Y, nearPt.Z };
    Vector3 dir = Vector3Normalize(Vector3Subtract(
        (Vector3){ farPt.X, farPt.Y, farPt.Z }, origin));

    return (Ray){ origin, dir };
}

Vector2 FdWorldToScreen(Vector3 worldPos, FdMat4 vp, int screenW, int screenH) {
    HMM_Vec4 clip = HMM_MulM4V4(vp, (HMM_Vec4){{worldPos.x, worldPos.y, worldPos.z, 1.0f}});
    if (clip.W <= 0.001f) return (Vector2){-1000, -1000};

    float ndcX = clip.X / clip.W;
    float ndcY = clip.Y / clip.W;

    return (Vector2){
        (ndcX * 0.5f + 0.5f) * screenW,
        (0.5f - ndcY * 0.5f) * screenH,
    };
}

// --- Text formatting ---

const char *FdTextFormat(const char *fmt, ...) {
    static char buffers[4][256];
    static int idx = 0;
    char *buf = buffers[idx];
    idx = (idx + 1) % 4;

    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, 256, fmt, args);
    va_end(args);
    return buf;
}
