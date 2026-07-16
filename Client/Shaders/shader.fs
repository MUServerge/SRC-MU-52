#version 330 compatibility
out vec4 FragColor;

in vec2 TexCoord;
in vec4 to_light;

uniform sampler2D texture1;

void main() {
    vec4 tex = texture(texture1, TexCoord);

    float alpha = tex.a * to_light.a;
    if (alpha < 0.1)
        discard;

    FragColor = vec4(to_light.rgb * tex.rgb, alpha);
}
