#version 450

layout(binding = 0) uniform sampler2D skyboxTex;

layout(location = 0) in vec3 inDir;
layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;

void main() {
    vec3 dir = normalize(inDir);

    // Standard equirectangular mapping: longitude around Y, latitude from
    // the poles.
    float u = atan(dir.z, dir.x) / (2.0 * PI) + 0.5;
    float v = acos(clamp(dir.y, -1.0, 1.0)) / PI;

    outColor = texture(skyboxTex, vec2(u, v));
}
