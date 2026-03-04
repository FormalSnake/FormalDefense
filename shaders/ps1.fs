#version 330

in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragWorldPos;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

uniform vec3 lightDir;
uniform vec3 lightColor;
uniform vec3 ambientColor;
uniform float colorBands;

out vec4 finalColor;

void main()
{
    // Flat normal from screen-space derivatives of world position
    vec3 dx = dFdx(fragWorldPos);
    vec3 dy = dFdy(fragWorldPos);
    vec3 normal = normalize(cross(dx, dy));

    // Simple directional lighting
    float NdotL = max(dot(normal, -normalize(lightDir)), 0.0);
    vec3 lighting = ambientColor + lightColor * NdotL;

    // Base color from vertex color * diffuse
    vec4 texColor = texture(texture0, fragTexCoord);
    vec4 baseColor = fragColor * colDiffuse * texColor;

    // Apply lighting
    vec3 litColor = baseColor.rgb * lighting;

    // Color banding / posterization
    litColor = floor(litColor * colorBands) / colorBands;

    finalColor = vec4(litColor, baseColor.a);
}
