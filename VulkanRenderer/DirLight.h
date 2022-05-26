#pragma once
#include <glm/glm.hpp>
struct DirLight
{
	DirLight(glm::vec4 color, glm::vec4 dir);
public:
	glm::vec4 mColor;
	glm::vec4 mDir;
};

