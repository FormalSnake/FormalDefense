// Unlit vertex-color shader (skybox)
@ctype mat4 HMM_Mat4

@vs vs_unlit
layout(binding=0) uniform vs_unlit_params {
    mat4 mvp;
};

in vec4 position;
in vec4 color0;

out vec4 frag_color;

void main() {
    frag_color = color0;
    gl_Position = mvp * vec4(position.xyz, 1.0);
}
@end

@fs fs_unlit
in vec4 frag_color;
out vec4 out_color;

void main() {
    out_color = frag_color;
}
@end

@program unlit vs_unlit fs_unlit
