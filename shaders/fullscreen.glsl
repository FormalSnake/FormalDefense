// Fullscreen blit shader for offscreen render target to screen
@ctype mat4 HMM_Mat4

@vs vs_fullscreen
out vec2 uv;

void main() {
    uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
    uv.y = 1.0 - uv.y;
}
@end

@fs fs_fullscreen
layout(binding=0) uniform texture2D tex;
layout(binding=0) uniform sampler smp;

in vec2 uv;
out vec4 out_color;

void main() {
    out_color = texture(sampler2D(tex, smp), uv);
}
@end

@program fullscreen vs_fullscreen fs_fullscreen
