#version 450

// Standard full-screen-triangle trick: 3 hardcoded vertices, no vertex/index
// buffer needed. The third vertex lands outside the -1..1 NDC range on
// purpose -- the visible screen is exactly covered by one corner of this
// oversized triangle, and rasterization/interpolation clips the rest.
layout(push_constant) uniform PushConstants {
    mat4 invViewProj;
} pc;

layout(location = 0) out vec3 outDir;

void main() {
    vec2 positions[3] = vec2[](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0)
    );
    vec2 pos = positions[gl_VertexIndex];

    // invViewProj was built from a view matrix with its translation zeroed
    // out, so this reconstructs a pure direction -- the skybox rotates with
    // the camera but never translates.
    vec4 worldPos = pc.invViewProj * vec4(pos, 1.0, 1.0);
    outDir = worldPos.xyz / worldPos.w;

    gl_Position = vec4(pos, 1.0, 1.0);
}
