#pragma once
#include <glm/glm.hpp>
struct PointLight
{
	PointLight(glm::vec3 pos, glm::vec3 color);

	glm::vec3 mPos;
	glm::vec3 mColor;
};

