#version 450

layout (binding = 2) uniform sampler2D samplerposition;
layout (binding = 3) uniform sampler2D samplerAlbedo;
layout (binding = 4) uniform sampler2D samplerNormal;

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outFragcolor;

struct DirLight {
	vec3 color;
    float pad;
	vec3 dir;
    float pad2;
};

layout (binding = 1) uniform DirLightsUBO {
    DirLight dirlights[3];
    vec3 lookvec;
} dirLight;

void main() 
{
	// Get G-Buffer values
	vec3 fragPos = texture(samplerposition, inUV).rgb;
	vec3 normal = texture(samplerNormal, inUV).rgb;
	vec4 albedo = texture(samplerAlbedo, inUV);

	vec3 norm_n = normalize(normal);
    
    vec3 result = vec3(0, 0, 0);
    
    for(int i = 0; i < 3; ++i)
    {
        vec3 norm_l = normalize(dirLight.dirlights[i].dir);
        float diff = max(dot(norm_l, norm_n), 0.3);
        vec3 diffuse = diff * dirLight.dirlights[i].color;

        result += (diffuse) * vec3(albedo);
    }

    outFragcolor = vec4(result, 1.0f);
}