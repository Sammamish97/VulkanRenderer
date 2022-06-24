#version 450
layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;

layout (push_constant) uniform constants
{
	mat4 model;
} PushConstants;

layout (binding = 7) uniform LightMatUBO 
{
	mat4 lightMVP;
} LightMat;

void main()
{
    gl_Position = LightMat.lightMVP * PushConstants.model * vec4(inPos, 1.0);
}