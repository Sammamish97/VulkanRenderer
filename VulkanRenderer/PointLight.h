#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
struct PointLight
{
	PointLight(glm::vec3 color = glm::vec3(0.1f, 0.1f, 0.1f), glm::vec3 pos = glm::vec3(0.f, 1.f, 0.f));

public:
	glm::vec3 mColor;
	float pad;
	glm::vec3 mPos;
	float pad2;
};