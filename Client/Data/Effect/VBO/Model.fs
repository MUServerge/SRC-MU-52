#version 330 core

// Model fragment shader.
//
// SINGLE-PASS ITEM GLOW (see docs/RENDER_ARCHITECTURE_MAP.md section 4b).
//
// A glowing item used to be drawn three times over the same geometry: the base
// texture, then BITMAP_CHROME and BITMAP_SHINY as additively blended overlays
// whose texture coordinates are generated from the transformed normal. That was
// the only way to layer textures when hardware had a single combine stage. It
// is also the direct cause of the two worst problems in this renderer: it
// multiplies the mesh-draw count that dominates the frame, and the overlay draw
// is a second copy of the surface that can disagree with the base when the two
// take different skinning paths, which is what makes the glow break.
//
// A shader has no such limit. The layers are sampled here, in one draw.
//
// The layer inputs default to zero, which reproduces the plain textured result
// exactly, so this file is inert until the C++ side describes a material.

in vec2 vTex;        // base UVs
in vec2 vChromeTex;  // reflection coords generated in the vertex shader
in vec4 vColor;

out vec4 FragColor;

uniform sampler2D uTexture;      // the item's own texture
uniform sampler2D uChromeTex;    // BITMAP_CHROME  - the reflection layer
uniform sampler2D uShinyTex;     // BITMAP_SHINY   - the metal highlight layer

// Per-layer strength. 0 disables the layer entirely, which is the default and
// keeps this identical to the single-texture version. These carry what the
// separate passes used to express through BodyLight and the blend mode.
uniform vec3 uChromeColor;
uniform vec3 uShinyColor;

void main()
{
    vec4 result = texture(uTexture, vTex) * vColor;

    // The overlays were additive passes, so they add here. Their contribution is
    // scaled by the same per-layer colour the extra RenderBody/RenderMesh calls
    // used to pass in, which is how item level and item type modulated the glow.
    if (uChromeColor != vec3(0.0))
        result.rgb += texture(uChromeTex, vChromeTex).rgb * uChromeColor;

    if (uShinyColor != vec3(0.0))
        result.rgb += texture(uShinyTex, vChromeTex).rgb * uShinyColor;

    FragColor = result;
}
