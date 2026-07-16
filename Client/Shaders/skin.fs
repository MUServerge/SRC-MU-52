#version 330 core

// =============================================================================
// GL33 migration POC - skinned-model fragment shader.
// Parity with the legacy fixed-function look: the engine bakes per-vertex
// lighting into the vertex color (vLight), so we just MODULATE it by the texture
// (GL_MODULATE). Re-applying lighting here double-lights and blows models out to
// white (same bug that was fixed in shader.fs). Alpha test matches the legacy
// EnableAlphaTest cutoff.
// =============================================================================

in vec2 vTexCoord;
in vec4 vLight;

uniform sampler2D uTexture;

out vec4 FragColor;

void main()
{
    vec4 tex = texture(uTexture, vTexCoord);

    float alpha = tex.a * vLight.a;
    if (alpha < 0.1)
        discard;

    FragColor = vec4(vLight.rgb * tex.rgb, alpha);
}
