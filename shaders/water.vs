#version 330

in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec4 vertexColor;

uniform mat4 mvp;
uniform mat4 matModel;
uniform vec2 resolution;
uniform float jitterStrength;
uniform float time;

out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragWorldPos;
out float fragWaveHeight;

void main()
{
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;

    // World position
    vec3 worldPos = (matModel * vec4(vertexPosition, 1.0)).xyz;

    // Three overlapping sine waves for water surface
    float wave1 = sin(worldPos.x * 0.8 + time * 1.2) * 0.055;
    float wave2 = sin(worldPos.z * 1.1 + time * 0.9 + 2.0) * 0.04;
    float wave3 = sin((worldPos.x + worldPos.z) * 0.6 + time * 1.5 + 4.5) * 0.03;
    float waveY = wave1 + wave2 + wave3;

    worldPos.y += waveY;
    fragWaveHeight = waveY;
    fragWorldPos = worldPos;

    // Reconstruct local position with wave offset for MVP
    vec3 displaced = vertexPosition;
    displaced.y += waveY;

    vec4 clipPos = mvp * vec4(displaced, 1.0);

    // PS1 vertex snapping
    vec2 screenPos = clipPos.xy / clipPos.w;
    vec2 pixelPos = screenPos * resolution * 0.5;
    float gridSize = jitterStrength;
    pixelPos = floor(pixelPos / gridSize) * gridSize;
    screenPos = pixelPos / (resolution * 0.5);
    clipPos.xy = screenPos * clipPos.w;

    gl_Position = clipPos;
}
