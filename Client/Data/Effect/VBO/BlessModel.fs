#version 330 core

// Adapted from the explicit model fragment-stage structure in
// Bless Shader/model_sync.frag. Existing MU texture and optional glow-layer
// contracts are retained so opting in does not require a material-path change.
in vec2 vTexCoord;
in vec2 vReflectionCoord;
in vec4 vModelColor;

out vec4 FragColor;

uniform sampler2D uTexture;
uniform sampler2D uChromeTex;
uniform sampler2D uShinyTex;
// CShaderGL uploads these through the established vec4 material setter. Keep
// the fourth component reserved while consuming RGB exactly like the current
// Model material path intends.
uniform vec4 uChromeColor;
uniform vec4 uShinyColor;

void main()
{
    vec4 result = texture(uTexture, vTexCoord) * vModelColor;

    if (uChromeColor.rgb != vec3(0.0))
        result.rgb += texture(uChromeTex, vReflectionCoord).rgb * uChromeColor.rgb;

    if (uShinyColor.rgb != vec3(0.0))
        result.rgb += texture(uShinyTex, vReflectionCoord).rgb * uShinyColor.rgb;

    FragColor = result;
}
