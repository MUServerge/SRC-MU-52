#version 330 core

// Phase 17: Core-profile 2D UI vertex shader (INERT - not wired yet).
//
// The UI is drawn in screen space, not world space: BeginBitmap() replaces the
// projection with gluOrtho2D(0, WindowWidth, 0, WindowHeight) and loads an
// identity modelview, so this path must NOT use g_ProjectionMatrix (which holds
// the perspective camera). uProj is fed from a CPU ortho mirror built alongside
// that gluOrtho2D, the same way g_ProjectionMatrix mirrors gluPerspective.
//
// Positions are 2-component (RenderBitmap and friends submit float p[4][2]);
// there is no depth involved because BeginBitmap disables the depth test.
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTex;

uniform mat4 uProj;

out vec2 TexCoord;

void main()
{
    TexCoord = aTex;
    gl_Position = uProj * vec4(aPos, 0.0, 1.0);
}
