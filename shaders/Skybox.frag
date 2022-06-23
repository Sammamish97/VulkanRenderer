#version 450
layout (location = 0) in vec3 TexCoords;
layout (location = 0) out vec4 FragColor;

layout (binding = 6) uniform samplerCube skybox;

void main()
{    
    FragColor = texture(skybox, TexCoords);
}