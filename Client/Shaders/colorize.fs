#version 330 compatibility
out vec4 FragColor;

in vec2 TexCoord;
in vec3 vColor;

uniform sampler2D uTexture;

void main()
{
    vec4 tex = texture(uTexture, TexCoord);

    float alpha = tex.a;
    if (alpha < 0.1)
        discard;

    FragColor = vec4(vColor * tex.rgb, alpha);
}
