#version 330 core

// Phase 15: Core-profile character/BMD vertex shader. Semantically identical to
// the 330-compatibility character.vs (gl_Position = MVP * gl_Vertex; TexCoord =
// gl_MultiTexCoord0; to_light = gl_Color) but with explicit vertex attributes and
// matrix uniforms, so it runs in a Core profile with no fixed-function built-ins.
// Fed by the BMD emit collector (CPU-skinned world-space positions). uModelView is
// the per-character modelview (camera * character transform), set per object; uProj
// is g_ProjectionMatrix. NOT wired into CShaderScene yet.
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTex;
layout(location = 2) in vec4 aColor;

uniform mat4 uProj;
uniform mat4 uModelView;

// Phase 15.4: the legacy draw enables GL_COLOR_ARRAY only for lit/chrome meshes;
// otherwise every vertex takes the fixed-function current colour. A Core profile
// has neither, so the draw path selects between the aColor stream and a constant.
uniform int  uUseVertexColor;
uniform vec4 uConstColor;

out vec2 TexCoord;
out vec4 to_light;

void main()
{
    gl_Position = uProj * uModelView * vec4(aPos, 1.0);
    TexCoord = aTex;
    to_light = (uUseVertexColor != 0) ? aColor : uConstColor;
}
