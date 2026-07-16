#version 330 compatibility

out vec2 TexCoord;
out vec3 Normal;
out vec3 FragPos;
out vec4 VertColor;

void main() {
    FragPos = vec3(gl_ModelViewMatrix * gl_Vertex);
    Normal = gl_NormalMatrix * gl_Normal;  
    TexCoord = gl_MultiTexCoord0.xy;
    VertColor = gl_Color;
    
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}
