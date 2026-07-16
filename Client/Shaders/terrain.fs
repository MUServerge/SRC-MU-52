#version 330 compatibility

out vec4 FragColor;

in vec2 TexCoord;
in vec3 Normal;
in vec3 FragPos;
in vec4 VertColor;

uniform sampler2D texture1;
uniform float brightness = 1.0;
uniform float contrast = 1.0;

void main() {
    vec4 texColor = texture(texture1, TexCoord);
    
    // Mix with vertex color
    vec3 result = texColor.rgb * VertColor.rgb;
    
    // Apply brightness and contrast
    result = (result - 0.5) * contrast + 0.5 + (brightness - 1.0);
    
    FragColor = vec4(result, texColor.a * VertColor.a);
}
