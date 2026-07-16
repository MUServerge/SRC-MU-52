#version 330 core

// =============================================================================
// GL33 migration POC - single-bone rigid GPU skinning.
// See Source/Main5.2/GL33_MIGRATION_PLAN.md (Phase 2 / POC).
//
// Reproduces the legacy CPU path (BMD::Transform, ZzzBMD.cpp) exactly so the
// result is byte-for-byte the same geometry, just moved off the CPU:
//     world = BoneMatrix[Node] * vec4(Position, 1)     // VectorTransform, 3x4 affine
//     if (useTranslate) world = world * bodyScale + bodyOrigin
// then the usual view / projection.
//
// Static per-vertex data (uploaded ONCE): aPos, aBone, aUV.
// Per-frame data: bone matrices (UBO) + per-vertex light (aColor, still CPU in
// the POC by design) + view/projection/body uniforms.
// =============================================================================

layout(location = 0) in vec3  aPos;     // static bind-pose local position
layout(location = 1) in float aBone;    // bone index (Vertex_t::Node)
layout(location = 2) in vec2  aUV;      // static texcoord
layout(location = 3) in vec4  aColor;   // per-frame CPU light (RGBA)

const int SKIN_MAX_BONES = 200;         // == MAX_BONES (ZzzBMD.h)

// std140 UBO. 3 vec4 "rows" per bone == the exact memory layout of the engine's
// BoneTransform[MAX_BONES][3][4] (row-major 3x4 affine), so C++ uploads it with a
// single contiguous copy. 200*3 = 600 vec4 = 9600 bytes < the 16 KB GL 3.3 min,
// and a UBO (not a plain uniform array) avoids the 256-vec4 uniform-limit link
// failure on weak x86 drivers (see plan risks).
layout(std140) uniform BoneBlock
{
    vec4 uBones[SKIN_MAX_BONES * 3];
};

uniform mat4  uView;
uniform mat4  uProjection;
uniform float uBodyScale;
uniform vec3  uBodyOrigin;
uniform int   uUseTranslate;

out vec2 vTexCoord;
out vec4 vLight;

void main()
{
    int b = int(aBone + 0.5) * 3;
    vec4 p = vec4(aPos, 1.0);

    vec3 world;
    world.x = dot(uBones[b + 0], p);
    world.y = dot(uBones[b + 1], p);
    world.z = dot(uBones[b + 2], p);

    if (uUseTranslate == 1)
        world = world * uBodyScale + uBodyOrigin;

    gl_Position = uProjection * uView * vec4(world, 1.0);
    vTexCoord = aUV;
    vLight = aColor;
}
