#version 450
layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;

layout (location = 0) out vec3 TexCoords;

layout (binding = 0) uniform UBO 
{
	mat4 view;
	mat4 projection;
} Mat;


void main()
{
    mat4 posRemoveView = mat4(mat3(Mat.view));
    TexCoords = inPos;
    vec4 pos = Mat.projection * posRemoveView * vec4(inPos, 1.0);
    gl_Position = pos.xyww;
}  