#pragma once
#include <glm/glm.hpp>
#include "DirLight.h"

struct UniformBufferMat
{
	glm::mat4 view;
	glm::mat4 proj;
};

struct UniformBufferLights
{
	DirLight dir_light[3];
	glm::vec3 lookVec;
};