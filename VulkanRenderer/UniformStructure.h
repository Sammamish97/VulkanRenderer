#pragma once
#include <glm/glm.hpp>
#include "PointLight.h"
#include "DirLight.h"
struct UniformBufferMat
{
	glm::mat4 view;
	glm::mat4 proj;
};

struct LightMatUBO
{
	glm::mat4 lightMVP;
};

struct UniformBufferLights
{
	PointLight point_light[3];
	glm::vec3 lookVec;
};

