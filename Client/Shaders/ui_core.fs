#version 330 core

// Phase 17: Core-profile 2D UI fragment shader (INERT - not wired yet).
//
// The 2D UI draws have NO per-vertex colour: RenderBitmap, RenderColor and the
// rotation variants set the fixed-function current colour once with glColor4f
// and submit only positions and texcoords. So the colour arrives as a uniform
// here rather than as an attribute, which also keeps the vertex format to the
// two streams those call sites already build.
//
// Blending stays fixed-function (glBlendFunc is program-independent), exactly as
// in the effect path. Only the texture combine and the alpha-test discard move
// into the shader.
out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D texture1;
// Fixed-function current colour, read back per draw.
uniform vec4 uColor;
// 0 for the untextured branches (RenderColor, and the DisableTexture() cases);
// the colour is then used directly.
uniform int uUseTexture;
// Optional alpha-test discard threshold (<0 disables). Mirrors glAlphaFunc.
uniform float uAlphaRef;

void main()
{
    vec4 result = uColor;
    if (uUseTexture != 0)
        result *= texture(texture1, TexCoord);

    if (uAlphaRef >= 0.0 && result.a < uAlphaRef)
        discard;

    FragColor = result;
}
