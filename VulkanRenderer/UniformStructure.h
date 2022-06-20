#pragma once
#include <glm/glm.hpp>
#include "PointLight.h"
struct UniformBufferMat
{
	glm::mat4 view;
	glm::mat4 proj;
};

struct UniformBufferLights
{
	PointLight point_light[3];
	glm::vec3 lookVec;
};