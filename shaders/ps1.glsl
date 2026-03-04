// PS1-style vertex snapping + flat lighting + color banding shader
@ctype mat4 HMM_Mat4
@ctype vec3 HMM_Vec3
@ctype vec2 HMM_Vec2

@vs vs_ps1
layout(binding=0) uniform vs_params {
    mat4 mvp;
    mat4 model;
    vec2 resolution;
    float jitter_strength;
};

in vec4 position;
in vec4 color0;
in vec3 normal;

out vec4 frag_color;
out vec3 frag_world_pos;

void main() {
    frag_color = color0;
    frag_world_pos = (model * vec4(position.xyz, 1.0)).xyz;

    vec4 clip_pos = mvp * vec4(position.xyz, 1.0);

    // PS1 vertex snapping: snap to screen-pixel grid
    vec2 screen_pos = clip_pos.xy / clip_pos.w;
    vec2 pixel_pos = screen_pos * resolution * 0.5;
    float grid_size = jitter_strength;
    pixel_pos = floor(pixel_pos / grid_size) * grid_size;
    screen_pos = pixel_pos / (resolution * 0.5);
    clip_pos.xy = screen_pos * clip_pos.w;

    gl_Position = clip_pos;
}
@end

@fs fs_ps1
layout(binding=1) uniform fs_params {
    vec3 light_dir;
    vec3 light_color;
    vec3 ambient_color;
    float color_bands;
};

in vec4 frag_color;
in vec3 frag_world_pos;

out vec4 out_color;

void main() {
    vec3 dx = dFdx(frag_world_pos);
    vec3 dy = dFdy(frag_world_pos);
    vec3 n = normalize(cross(dx, dy));

    float NdotL = max(dot(n, -normalize(light_dir)), 0.0);
    vec3 lighting = ambient_color + light_color * NdotL;

    vec3 lit_color = frag_color.rgb * lighting;
    lit_color = floor(lit_color * color_bands) / color_bands;

    out_color = vec4(lit_color, frag_color.a);
}
@end

@program ps1 vs_ps1 fs_ps1
