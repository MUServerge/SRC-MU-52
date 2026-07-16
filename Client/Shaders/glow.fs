#version 330 compatibility
out vec4 FragColor;

in vec2 TexCoord;
in vec4 to_light;

uniform sampler2D texture1;
uniform float time;
uniform float glowIntensity;
uniform vec3  glowColor;

void main() {
    vec4 tex = texture(texture1, TexCoord);

    float alpha = tex.a * to_light.a;
    if (alpha < 0.1)
        discard;

    vec3 base = to_light.rgb * tex.rgb;

    float pulse = sin(time * 3.0) * 0.5 + 0.5;
    base += glowColor * glowIntensity * pulse;

    FragColor = vec4(base, alpha);
}
