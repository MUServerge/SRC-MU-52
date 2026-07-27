#version 330 core

// Phase 14: Core-profile terrain vertex shader. Semantically identical to the
// 330-compatibility terrain.vs (FragPos = ModelView*Vertex, Normal =
// NormalMatrix*Normal, gl_Position = MVP*Vertex) but with explicit vertex
// attributes and matrix uniforms, so it runs in a Core profile with no
// fixed-function built-ins. Fed by the terrain VBO/VAO (Phase 14.x): uProj is
// g_ProjectionMatrix, uModelView the camera modelview, and uNormalMatrix =
// transpose(inverse(mat3(uModelView))). NOT wired into CShaderScene yet.
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTex;
layout(location = 3) in vec4 aColor;

uniform mat4 uProj;
uniform mat4 uModelView;
uniform mat3 uNormalMatrix;

out vec2 TexCoord;
out vec3 Normal;
out vec3 FragPos;
out vec4 VertColor;

void main() {
    vec4 mvPos = uModelView * vec4(aPos, 1.0);
    FragPos = mvPos.xyz;
    Normal = uNormalMatrix * aNormal;
    TexCoord = aTex;
    VertColor = aColor;
    gl_Position = uProj * mvPos;
}
