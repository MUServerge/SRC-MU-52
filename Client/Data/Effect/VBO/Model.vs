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

// Chrome / metal / oil texture-coordinate generation.
//
// These materials do not have per-vertex UVs: the legacy path recomputes the
// texcoord every frame on the CPU from the TRANSFORMED NORMAL (ZzzBMD.cpp
// g_chrome, ~lines 1762-1819) and feeds it through the client-array draw. That
// forced every chrome/metal overlay onto the CPU-skinned path while the base
// mesh underneath it was GPU-skinned here - two skinnings of one surface, whose
// float noise z-fights and produces the camera-distance-dependent glow flicker.
//
// The formulas are pure functions of the transformed normal (which this shader
// already computes) plus a few per-draw scalars, so they belong here. Geometry
// and skinning are untouched: positions stay bit-identical to the base pass,
// which is what actually removes the z-fight.
//
// u_chromeMode 0 keeps aTex, so this whole block is inert until the C++ side
// selects a mode (uninitialized uniforms read as 0).
#define CHROME_NONE    0
#define CHROME_METAL   1   // default branch: N.z*0.5+0.2 , N.y*0.5+0.5
#define CHROME_1       2   // RENDER_CHROME
#define CHROME_2       3   // RENDER_CHROME2
#define CHROME_3       4   // RENDER_CHROME3
#define CHROME_4       5   // RENDER_CHROME4
#define CHROME_5       6   // RENDER_CHROME5
#define CHROME_6       7   // RENDER_CHROME6
#define CHROME_7       8   // RENDER_CHROME7
#define CHROME_NORMALXY 9  // RENDER_CHROME8 and RENDER_OIL

// How the generated coordinate is applied, mirroring the legacy build loop:
//   0 = use it directly                    (RENDER_CHROME)
//   1 = add u_blendMeshTexCoord            (RENDER_CHROME4, RENDER_CHROME8)
//   2 = multiply by aTex, then add offset  (RENDER_OIL)
#define CHROME_APPLY_DIRECT 0
#define CHROME_APPLY_OFFSET 1
#define CHROME_APPLY_MODULATE 2

uniform int  u_chromeMode;
uniform int  u_chromeApply;
// x = Wave, y = Wave2, z = WorldTime * 0.00006 (precomputed on the CPU: passing
// WorldTime itself would lose precision in a float uniform).
uniform vec4 u_chromeScalars;
uniform vec4 u_chromeL;
uniform vec4 u_chromeLightVector;
uniform vec4 u_blendMeshTexCoord;

vec2 ChromeTexCoord(vec3 N)
{
    float wave  = u_chromeScalars.x;
    float wave2 = u_chromeScalars.y;
    vec3  L     = u_chromeL.xyz;

    if (u_chromeMode == CHROME_1)
        return vec2(N.z * 0.5 + wave, N.y * 0.5 + wave * 2.0);
    if (u_chromeMode == CHROME_2)
        return vec2((N.z + N.x) * 0.8 + wave2 * 2.0, (N.y + N.x) * 1.0 + wave2 * 3.0);
    if (u_chromeMode == CHROME_3)
    {
        float d = dot(N, u_chromeLightVector.xyz);
        return vec2(d, 1.0 - d);
    }
    if (u_chromeMode == CHROME_4)
    {
        float d = dot(N, L);
        return vec2(d + (N.y * 0.5 + L.y * 3.0), (1.0 - d) - (N.z * 0.5 + wave * 3.0));
    }
    if (u_chromeMode == CHROME_5)
    {
        float d = dot(N, L);
        return vec2(d + (N.y * 3.0 + L.y * 5.0), (1.0 - d) - (N.z * 2.5 + wave * 1.0));
    }
    if (u_chromeMode == CHROME_6)
    {
        float s = (N.z + N.x) * 0.8 + wave2 * 2.0;
        return vec2(s, s);
    }
    if (u_chromeMode == CHROME_7)
    {
        float s = (N.z + N.x) * 0.8 + u_chromeScalars.z;
        return vec2(s, s);
    }
    if (u_chromeMode == CHROME_NORMALXY)
        return vec2(N.x, N.y);

    // CHROME_METAL - the legacy 'else' branch
    return vec2(N.z * 0.5 + 0.2, N.y * 0.5 + 0.5);
}

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
    // Raw, exactly like the CPU path (ZzzBMD.cpp NormalTransform is a bare
    // VectorRotate with no renormalization). See main(): the two consumers of
    // this vector need different things, so the normalize happens per consumer
    // instead of here.
    return vec3(dot(r0, normal), dot(r1, normal), dot(r2, normal));
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

    if (u_chromeMode == CHROME_NONE)
    {
        vTex = aTex;
    }
    else
    {
        // Chrome takes the UNIT normal. These formulas map the normal straight
        // into texture space (N.z*0.5+0.2 and friends), so a normal whose length
        // wobbles per vertex and per animation frame makes the highlight swim
        // and flicker. Length is meaningless to a reflection direction - only
        // orientation is - so normalizing here is correct, not a tweak.
        vec2 chrome = ChromeTexCoord(normalize(normal));
        if (u_chromeApply == CHROME_APPLY_MODULATE)
            vTex = chrome * aTex + u_blendMeshTexCoord.xy;
        else if (u_chromeApply == CHROME_APPLY_OFFSET)
            vTex = chrome + u_blendMeshTexCoord.xy;
        else
            vTex = chrome;
    }

    // Match the legacy fixed-function lighting exactly (ZzzBMD.cpp RenderMesh /
    // BMD::Transform): body colour modulated per-vertex by clamp(dot(N,L)*0.8+0.4, min 0.2)
    // when lit, otherwise flat body colour. No brightness floor (the old max(...,0.45)
    // washed out dark areas / over-brightened them).
    // Lighting takes the RAW normal, matching the CPU term exactly. Normalizing
    // it here lengthened short BMD normals, raised dot(N,L) and washed out
    // characters/NPCs - that difference is what the removed `* 0.85` trim used
    // to average back down.
    vec4 color = u_bodyLight;
    if (u_enableLight != 0) {
        float intensity = dot(normal, u_lightPosition.xyz) * 0.8 + 0.4;
        if (intensity < 0.2) intensity = 0.2;
        color.rgb *= intensity;
    }
    // Fixed-function clamps the vertex colour before the texture modulate, so
    // this clamp is real parity, not a tweak.
    //
    // A `* (u_translate != 0 ? 1.0 : 0.85)` trim used to sit here. It had NO
    // counterpart anywhere in the CPU path - ZzzBMD.cpp computes exactly
    // dot(N,L)*0.8 + 0.4 with a 0.2 floor and nothing else - so it was an
    // eyeballed correction for a difference nobody had identified, and it made
    // u_translate do two unrelated jobs at once (body transform AND brightness).
    // Removed: u_translate is now purely geometric, and any remaining mismatch
    // has to be found and fixed rather than averaged away. Compare against the
    // legacy reference with the 'vbo.disable' marker.
    color.rgb = clamp(color.rgb, 0.0, 1.0);
    vColor = color;

    gl_Position = uProj * uView * vec4(worldPos, 1.0);
}
