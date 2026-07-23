#version 330 core

// Phase 16: Core-profile effect/sprite vertex shader (INERT — not wired into
// CShaderScene yet). Covers the fixed-function effect draws in ZzzEffect*,
// Sprite.cpp and SideHair.cpp, which emit glTexCoord/glColor/glVertex under a
// blend mode and a texture-env mode, with no lighting.
//
// Explicit attributes replace gl_Vertex / gl_MultiTexCoord0 / gl_Color so the
// program is Core-clean. Both a 3D world path (uModelView = camera view, verts in
// world space) and a 2D screen path (uModelView = identity, verts already in the
// ortho space set by the caller) are served by the same uProj * uModelView.
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTex;
layout(location = 2) in vec4 aColor;

uniform mat4 uProj;
uniform mat4 uModelView;

out vec2 TexCoord;
out vec4 vColor;

void main()
{
    gl_Position = uProj * uModelView * vec4(aPos, 1.0);
    TexCoord = aTex;
    vColor = aColor;
}
