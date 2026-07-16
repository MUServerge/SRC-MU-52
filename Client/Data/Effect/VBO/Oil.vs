#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTex;
layout(location = 3) in uint aBone;

uniform mat4 uProj;
uniform mat4 uView;
uniform vec4 u_Bones[600];

uniform vec4 u_bodyLight;
uniform vec4 u_lightPosition;
uniform vec4 u_meshUV;
uniform vec4 u_setting1;
uniform vec4 u_setting2;
uniform int u_enableLight;

out vec2 vTex;
out vec4 vColor;
out vec3 vNormal;

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

    vec2 uv = vec2(0.0);
    uv = (normal.xy * aTex) + u_meshUV.xy;

    vTex = uv;
    vNormal = normal;

    vec4 color = u_bodyLight;
    if (u_enableLight != 0) {
        float intensity = dot(normal, u_lightPosition.xyz) * 0.8 + 0.4;
        if (intensity < 0.2) intensity = 0.2;
        color.rgb *= intensity;
    }
    color.rgb = clamp(color.rgb, 0.0, 1.0); // fixed-function clamps vertex colour before the texture modulate
    vColor = color;

    gl_Position = uProj * uView * vec4(worldPos, 1.0);
}
