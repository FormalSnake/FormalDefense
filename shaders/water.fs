#version 330

in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragWorldPos;
in float fragWaveHeight;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

uniform vec3 lightDir;
uniform vec3 lightColor;
uniform vec3 ambientColor;
uniform float colorBands;
uniform float time;

out vec4 finalColor;

// Simple hash-based 2D noise
float hash(vec2 p)
{
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float noise2d(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);

    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));

    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

void main()
{
    // Flat normal from screen-space derivatives
    vec3 dx = dFdx(fragWorldPos);
    vec3 dy = dFdy(fragWorldPos);
    vec3 normal = normalize(cross(dx, dy));

    // Directional lighting
    float NdotL = max(dot(normal, -normalize(lightDir)), 0.0);
    vec3 lighting = ambientColor + lightColor * NdotL;

    // Deep blue base, lighter at wave crests
    vec3 deepBlue = vec3(0.08, 0.18, 0.42);
    vec3 lightBlue = vec3(0.15, 0.35, 0.55);
    float crestFactor = smoothstep(0.02, 0.1, fragWaveHeight);
    vec3 waterColor = mix(deepBlue, lightBlue, crestFactor);

    // Scrolling foam noise patches
    vec2 foamUV = fragWorldPos.xz * 0.4 + vec2(time * 0.08, time * 0.05);
    float foamNoise = noise2d(foamUV * 3.0);
    float foam = smoothstep(0.62, 0.75, foamNoise);

    // Foam also at wave crests
    foam = max(foam, smoothstep(0.07, 0.12, fragWaveHeight) * 0.6);

    vec3 foamColor = vec3(0.7, 0.8, 0.85);
    waterColor = mix(waterColor, foamColor, foam * 0.5);

    // Apply vertex color tint and lighting
    vec3 baseColor = waterColor * fragColor.rgb * colDiffuse.rgb;
    vec3 litColor = baseColor * lighting;

    // PS1 posterization
    litColor = floor(litColor * colorBands) / colorBands;

    finalColor = vec4(litColor, 1.0);
}
