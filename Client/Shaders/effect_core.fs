#version 330 core

// Phase 16: Core-profile effect/sprite fragment shader (INERT — not wired yet).
// Reproduces the fixed-function texture-environment combine that the effect draws
// rely on, since a Core profile has no glTexEnv. Alpha blending stays a
// program-independent fixed-function state (glBlendFunc), set by the caller as
// today; only the texel*primary combine and the alpha-test discard move here.
out vec4 FragColor;

in vec2 TexCoord;
in vec4 vColor;

uniform sampler2D texture1;
// Texture-env combine mode: 0 = GL_MODULATE (default), 1 = GL_ADD,
// 2 = GL_REPLACE. Matches the glTexEnvi(GL_TEXTURE_ENV_MODE, ...) sites.
uniform int uTexEnvMode;
// 0 when the draw is untextured (Sprite.cpp's no-texture branch, some effects);
// the primary colour is used directly.
uniform int uUseTexture;
// Optional alpha-test discard threshold (<0 disables). Default -1.
uniform float uAlphaRef;

void main()
{
    vec4 result;
    if (uUseTexture == 0)
    {
        result = vColor;
    }
    else
    {
        vec4 tex = texture(texture1, TexCoord);
        if (uTexEnvMode == 1)          // GL_ADD: Cv = Cp + Ct, Av = Ap * At
            result = vec4(vColor.rgb + tex.rgb, vColor.a * tex.a);
        else if (uTexEnvMode == 2)     // GL_REPLACE: Cv = Ct, Av = At
            result = tex;
        else                           // GL_MODULATE: Cv = Cp * Ct
            result = vColor * tex;
    }

    if (uAlphaRef >= 0.0 && result.a < uAlphaRef)
        discard;

    FragColor = result;
}
