#version 450

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec2 outUV;
layout (location = 2) out vec3 outWorldPos;


layout (push_constant) uniform constants
{
	mat4 model;
} PushConstants;

layout (binding = 0) uniform UBO 
{
	mat4 view;
	mat4 projection;
} Mat;


void main() 
{
	gl_Position = Mat.projection * Mat.view * PushConstants.model * vec4(inPos, 1.0);

	// Vertex position in world space
	outWorldPos = vec3(PushConstants.model * vec4(inPos, 1.0));
	
	// Normal in world space
	mat3 mNormal = transpose(inverse(mat3(PushConstants.model)));
	outNormal = mNormal * normalize(inNormal);	
	
	// Currently just vertex color
	outUV = inUV;
}
