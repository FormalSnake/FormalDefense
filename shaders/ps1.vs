#version 330

// Raylib batch attributes
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec4 vertexColor;

uniform mat4 mvp;
uniform mat4 matModel;
uniform vec2 resolution;
uniform float jitterStrength;

out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragWorldPos;

void main()
{
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;

    // World position for fragment lighting
    fragWorldPos = (matModel * vec4(vertexPosition, 1.0)).xyz;

    // Standard clip-space transform
    vec4 clipPos = mvp * vec4(vertexPosition, 1.0);

    // PS1 vertex snapping: snap to screen-pixel grid
    vec2 screenPos = clipPos.xy / clipPos.w;              // NDC [-1, 1]
    vec2 pixelPos = screenPos * resolution * 0.5;         // to pixel coords
    float gridSize = jitterStrength;
    pixelPos = floor(pixelPos / gridSize) * gridSize;     // snap
    screenPos = pixelPos / (resolution * 0.5);            // back to NDC
    clipPos.xy = screenPos * clipPos.w;

    gl_Position = clipPos;
}
