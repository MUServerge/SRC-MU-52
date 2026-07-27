#version 330 core

// Phase 14: Core-profile terrain fragment shader. Identical logic to the
// 330-compatibility terrain.fs; only the profile changed (it already used no
// fixed-function built-ins). NOT wired into CShaderScene yet.
out vec4 FragColor;

in vec2 TexCoord;
in vec3 Normal;
in vec3 FragPos;
in vec4 VertColor;

uniform sampler2D texture1;
// No inline initializers: uniform default values are not portable in strict
// 330 core (they were standardized in GLSL 4.20). The draw path sets brightness
// and contrast from C++ (default 1.0), matching the compatibility terrain.fs.
uniform float brightness;
uniform float contrast;

void main() {
    vec4 texColor = texture(texture1, TexCoord);

    // Mix with vertex color
    vec3 result = texColor.rgb * VertColor.rgb;

    // Apply brightness and contrast
    result = (result - 0.5) * contrast + 0.5 + (brightness - 1.0);

    FragColor = vec4(result, texColor.a * VertColor.a);
}
