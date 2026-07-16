#version 330 compatibility

out vec2 TexCoord;
out vec3 vColor;

uniform int uColorMode = 0;
uniform int uColorValue = 0;
uniform sampler1D uColorPalette;

void main()
{
    TexCoord = gl_MultiTexCoord0.xy;

    vec3 baseColor = gl_Color.rgb;

    if (uColorMode > 0)
    {
        float idx = clamp(float(uColorValue) / 255.0, 0.0, 1.0);
        vec3 pal  = texture1D(uColorPalette, idx).rgb;
        vColor    = baseColor * pal;
    }
    else
    {
        vColor = baseColor;
    }

    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}
