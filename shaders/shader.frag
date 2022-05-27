#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;

layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform DirLight {
    vec4 color;
    vec4 dir;
} dirLight;

void main() {
    vec3 norm_l = vec3(normalize(dirLight.dir));
    vec3 norm_n = normalize(fragNormal);
    float diff = max(dot(norm_l, norm_n), 0.1);
    vec3 diffuse = diff * vec3(dirLight.color);
    vec3 result = fragColor * diffuse;
    outColor = vec4(result, 1.0f);
}