#version 430 core

// First isolated Bless-derived model experiment. The explicit model inputs,
// skinned position/normal stage and vertex-to-fragment contract are adapted
// from Bless Shader/model_sync.vert. Bindings remain SRC-MU-52-owned so this
// program can use the established BMD VAO and bone transports without SSBOs.
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTex;
layout(location = 3) in uint aBone;

uniform mat4 uProj;
uniform mat4 uView;

#ifdef MU_USE_BONE_SSBO
layout(std430) readonly buffer BoneStorage
{
    vec4 u_Bones[];
};
uniform int u_BoneBase;
#elif defined(MU_USE_BONE_UBO)
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

out vec2 vTexCoord;
out vec2 vReflectionCoord;
out vec4 vModelColor;

vec3 TransformPosition(vec3 position, uint boneIndex)
{
	#ifdef MU_USE_BONE_SSBO
	boneIndex += uint(u_BoneBase);
	#endif
    vec4 source = vec4(position, 1.0);
    return vec3(
        dot(u_Bones[boneIndex + 0u], source),
        dot(u_Bones[boneIndex + 1u], source),
        dot(u_Bones[boneIndex + 2u], source));
}

vec3 TransformNormal(vec3 normal, uint boneIndex)
{
	#ifdef MU_USE_BONE_SSBO
	boneIndex += uint(u_BoneBase);
	#endif
    return normalize(vec3(
        dot(u_Bones[boneIndex + 0u].xyz, normal),
        dot(u_Bones[boneIndex + 1u].xyz, normal),
        dot(u_Bones[boneIndex + 2u].xyz, normal)));
}

void main()
{
    vec3 worldPosition = TransformPosition(aPos, aBone);
    vec3 worldNormal = TransformNormal(aNormal, aBone);

    if (u_translate != 0)
        worldPosition = worldPosition * u_bodyTransform.w + u_bodyTransform.xyz;

    vTexCoord = aTex;
    vReflectionCoord = vec2(worldNormal.z * 0.5 + 0.2,
        worldNormal.y * 0.5 + 0.5);

    vec4 modelColor = u_bodyLight;
    if (u_enableLight != 0)
    {
        float intensity = dot(worldNormal, u_lightPosition.xyz) * 0.8 + 0.4;
        if (intensity < 0.2)
            intensity = 0.2;
        modelColor.rgb *= intensity;
    }
    modelColor.rgb = clamp(modelColor.rgb, 0.0, 1.0) *
        (u_translate != 0 ? 1.0 : 0.85);
    vModelColor = modelColor;

    gl_Position = uProj * uView * vec4(worldPosition, 1.0);
}
