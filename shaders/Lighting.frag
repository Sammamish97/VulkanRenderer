#version 450

layout (binding = 2) uniform sampler2D samplerposition;
layout (binding = 3) uniform sampler2D samplerNormal;
layout (binding = 4) uniform sampler2D samplerAlbedo;
layout (binding = 8) uniform sampler2D shadowDepth;

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outFragcolor;

struct PointLight {
	vec3 color;
    float pad;
	vec3 pos;
    float pad2;
};

layout (binding = 7) uniform LightMatUBO 
{
	mat4 lightMVP;
} LightMat;

layout (binding = 1) uniform PointLightsUBO {
    PointLight pointlights[3];
    vec3 lookvec;
} pointLight;

float ShadowCalc(vec4 shadowCoord)
{
    float shadow = 1.0;
    vec2 uv = vec2(shadowCoord.x, shadowCoord.y);
    float currentDepth = shadowCoord.z;
	if ( currentDepth > -1.0 && currentDepth < 1.0 ) 
	{
		float closestDepth = texture( shadowDepth, uv).r;
		if ( shadowCoord.w > 0.0 && closestDepth < currentDepth ) 
		{
			shadow = 0.0;
		}
	}
	return shadow;
}

void main() 
{
	// Get G-Buffer values
	vec3 fragPos = texture(samplerposition, inUV).rgb;//Each pixel of sample position contain world position
	vec3 normal = texture(samplerNormal, inUV).rgb;
	vec4 albedo = texture(samplerAlbedo, inUV);
	vec3 norm_n = normalize(normal);

    // Shadow values
    vec4 lightSpaceFragPos = LightMat.lightMVP * vec4(fragPos, 1.0);
    lightSpaceFragPos /= lightSpaceFragPos.w;

    float shadow = ShadowCalc(lightSpaceFragPos);

    //Light calculation
    vec3 result = vec3(0, 0, 0);
    
    //Currently, Shadowing working on only first light
    vec3 norm_l = normalize(pointLight.pointlights[0].pos - fragPos);
    float diff = max(dot(norm_l, norm_n), 0.2f);
    vec3 diffuse = diff * pointLight.pointlights[0].color;
    result += (diffuse) * (shadow);

    for(int i = 1; i < 3; ++i)
    {
        vec3 norm_l = normalize(pointLight.pointlights[i].pos - fragPos);
        float diff = max(dot(norm_l, norm_n), 0.2f);
        vec3 diffuse = diff * pointLight.pointlights[i].color;

        result += (diffuse);
    }

    outFragcolor = vec4(result, 1.0f);
}