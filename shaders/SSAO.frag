#version 450

layout (binding = 2) uniform sampler2D samplerposition;
layout (binding = 3) uniform sampler2D samplerNormal;
layout (binding = 4) uniform sampler2D samplerAlbedo;

layout (location = 0) in vec2 inUV;

layout (location = 0) out float outFragcolor;

void main() 
{
	vec3 fragPos = texture(samplerposition, inUV).rgb;//Each pixel of sample position contain world position
	vec3 normal = texture(samplerNormal, inUV).rgb;
	vec4 albedo = texture(samplerAlbedo, inUV);

    outFragcolor = fragPos.x;
}