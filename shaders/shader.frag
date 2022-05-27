#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 tempLookVec;

layout(location = 0) out vec4 outColor;

struct DirLight {
	vec3 color;
    float pad;
	vec3 dir;
    float pad2;
};

layout (binding = 1) uniform DirLightsUBO {
    DirLight dirlights[3];
} dirLight;

void main() {
   
    vec3 norm_n = normalize(fragNormal);
    
    vec3 result = vec3(0, 0, 0);
    
    for(int i = 0; i < 1; ++i)
    {
        vec3 norm_l = normalize(dirLight.dirlights[i].dir);
        float diff = max(dot(norm_l, norm_n), 0.1);
        vec3 diffuse = diff * dirLight.dirlights[i].color;

        float specularStrength = 0.5;
        vec3 reflectDir = reflect(-norm_l, norm_n);
        float spec = pow(max(dot(tempLookVec, reflectDir), 0.0), 32);
        vec3 specular = specularStrength * spec * dirLight.dirlights[i].color; 

        result += (diffuse + specular) * fragColor;
    }

    outColor = vec4(result, 1.0f);
}