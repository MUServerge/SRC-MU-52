#version 330 core

// Phase 15: Core-profile character/BMD fragment shader. Identical logic to the
// 330-compatibility character.fs; only the profile changed (it already used no
// fixed-function built-ins). NOT wired into CShaderScene yet.
out vec4 FragColor;

in vec2 TexCoord;
in vec4 to_light;

uniform sampler2D texture1;

void main()
{
    // Parity with the compatibility character.fs this program replaces: it
    // samples texture1 unconditionally (a bound fragment shader ignores the
    // fixed-function GL_TEXTURE_2D enable, so DisableTexture() has no effect on
    // it), so the Core path must too, or the ON/OFF outputs would differ on the
    // RENDER_BRIGHT / RENDER_COLOR materials.
    vec4 tex = texture(texture1, TexCoord);

    float alpha = tex.a * to_light.a;
    if (alpha < 0.1)
        discard;

    FragColor = vec4(to_light.rgb * tex.rgb, alpha);
}
