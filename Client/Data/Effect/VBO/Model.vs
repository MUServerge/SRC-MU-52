#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTex;
layout(location = 3) in uint aBone;

uniform mat4 uProj;
uniform mat4 uView;

// One authoritative source supports both bone transports. CShaderGL first
// compiles MU_USE_BONE_UBO when the context, entry points and 9,600-byte block
// contract are valid; otherwise it compiles the existing uniform-array fallback.
#ifdef MU_USE_BONE_UBO
layout(std140) uniform BoneBlock
{
    vec4 u_Bones[600];
};
#else
uniform vec4 u_Bones[600];
#endif

uniform vec4 u_bodyLight;
uniform vec4 u_lightPosition;
uniform int u_enableLight;
uniform vec4 u_bodyTransform;
uniform int u_translate;

out vec2 vTex;
out vec4 vColor;

vec3 ApplyBonePosition(vec3 pos, uint boneIndex)
{
    vec4 p = vec4(pos, 1.0);
    vec4 r0 = u_Bones[boneIndex + 0u];
    vec4 r1 = u_Bones[boneIndex + 1u];
    vec4 r2 = u_Bones[boneIndex + 2u];
    return vec3(dot(r0, p), dot(r1, p), dot(r2, p));
}

vec3 ApplyBoneNormal(vec3 normal, uint boneIndex)
{
    vec3 r0 = u_Bones[boneIndex + 0u].xyz;
    vec3 r1 = u_Bones[boneIndex + 1u].xyz;
    vec3 r2 = u_Bones[boneIndex + 2u].xyz;
    return normalize(vec3(dot(r0, normal), dot(r1, normal), dot(r2, normal)));
}

void main()
{
    uint boneIndex = aBone;
    vec3 worldPos = ApplyBonePosition(aPos, boneIndex);
    vec3 normal = ApplyBoneNormal(aNormal, boneIndex);

    // Match BMD::MaterializeCpuTransforms for translated character/equipment
    // meshes: bone-space result, uniform BodyScale, then BodyOrigin. Normals are
    // rotated by the bone only, exactly like the legacy CPU path.
    if (u_translate != 0) {
        worldPos = worldPos * u_bodyTransform.w + u_bodyTransform.xyz;
    }

    vTex = aTex;

    // Match the legacy fixed-function lighting exactly (ZzzBMD.cpp RenderMesh /
    // BMD::Transform): body colour modulated per-vertex by clamp(dot(N,L)*0.8+0.4, min 0.2)
    // when lit, otherwise flat body colour. No brightness floor (the old max(...,0.45)
    // washed out dark areas / over-brightened them).
    vec4 color = u_bodyLight;
    if (u_enableLight != 0) {
        float intensity = dot(normal, u_lightPosition.xyz) * 0.8 + 0.4;
        if (intensity < 0.2) intensity = 0.2;
        color.rgb *= intensity;
    }
    // Fixed-function clamps vertex colour before the texture modulate. Preserve
    // the established 0.85 trim for the existing world-object VBO path, but use
    // the untrimmed legacy colour for opt-in translated character/equipment draws.
    color.rgb = clamp(color.rgb, 0.0, 1.0) * (u_translate != 0 ? 1.0 : 0.85);
    vColor = color;

    gl_Position = uProj * uView * vec4(worldPos, 1.0);
}
