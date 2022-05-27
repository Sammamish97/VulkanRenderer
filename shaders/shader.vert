#version 450

layout(push_constant) uniform constants
{
    mat4 model;
} PushConstants;

layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    vec3 lookVec;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 tempLookVec;

void main() {
    gl_Position = ubo.proj * ubo.view * PushConstants.model * vec4(inPosition, 1.0);
    fragColor = inColor;
    fragNormal = inNormal;
    tempLookVec = normalize(ubo.lookVec);
}