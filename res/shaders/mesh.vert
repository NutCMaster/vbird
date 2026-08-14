#version 450

layout(binding = 0) uniform CameraUBO {
    mat4 viewProj;
} camera;

layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 color;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

layout(location = 0) out vec3 fragColor;

void main() {
    gl_Position = camera.viewProj * pc.model * vec4(inPosition, 1.0);

    // Fixed-direction flat shading, not a lighting system: every vertex on a
    // face shares the same axis-aligned normal, so this lands on one flat
    // brightness per face (top brightest, bottom darkest) -- just enough for
    // a solid-colored Part to still read as a 3D box instead of same-color
    // faces blending into a flat silhouette. No light objects, nothing
    // configurable; real lighting is a later milestone.
    const vec3 lightDir = normalize(vec3(0.35, 1.0, 0.45));
    float shade = clamp(0.55 + 0.45 * dot(normalize(inNormal), lightDir), 0.35, 1.0);

    fragColor = pc.color.rgb * shade;
}
