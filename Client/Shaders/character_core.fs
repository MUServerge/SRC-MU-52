#version 330 core

// Phase 15: Core-profile character/BMD fragment shader. Identical logic to the
// 330-compatibility character.fs; only the profile changed (it already used no
// fixed-function built-ins). NOT wired into CShaderScene yet.
out vec4 FragColor;

in vec2 TexCoord;
in vec4 to_light;

uniform sampler2D texture1;
// Phase 15.3: the legacy RENDER_BRIGHT material draws untextured (it calls
// DisableTexture()); a Core profile has no fixed-function texture enable, so the
// draw path selects it here instead. Defaults to 1 (textured) from C++.
uniform int uUseTexture;

void main()
{
    vec4 tex = (uUseTexture != 0) ? texture(texture1, TexCoord) : vec4(1.0);

    float alpha = tex.a * to_light.a;
    if (alpha < 0.1)
        discard;

    FragColor = vec4(to_light.rgb * tex.rgb, alpha);
}
