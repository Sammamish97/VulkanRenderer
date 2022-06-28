#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
struct Mesh;
struct Object
{
	Object(Mesh* mesh, glm::vec3 position, glm::vec3 scale = glm::vec3(1.f));
	glm::mat4 BuildModelMat() const;
	void Draw(/*Maybe command buffer OR device*/);

	Mesh* mMesh;
	glm::vec3 mPosition;
	glm::vec3 mScale;
};

