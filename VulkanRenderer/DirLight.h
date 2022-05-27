#pragma once
#include <glm/glm.hpp>
struct DirLight
{
	DirLight(glm::vec3 color = glm::vec3(0.1f, 0.1f, 0.1f), glm::vec3 dir = glm::vec3(0.f, 1.f, 0.f));
public:
	glm::vec3 mColor;
	float pad;
	glm::vec3 mDir;
	float pad2;
};

