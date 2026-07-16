#version 330 compatibility

out vec2 TexCoord;
out vec4 to_light;

void main()
{
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
    TexCoord = gl_MultiTexCoord0.xy;
    to_light = gl_Color;
}
