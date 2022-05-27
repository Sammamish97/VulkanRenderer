#include "Object.h"
#include "Mesh.h"
#include <glm/gtc/matrix_transform.hpp>
Object::Object(Mesh* mesh, glm::vec3 position, glm::vec3 scale)
	:mMesh(mesh), mPosition(position), mScale(scale)
{
}

void Object::Draw()
{
}

glm::mat4 Object::BuildModelMat() const
{
	glm::mat4 translate = glm::translate(glm::identity<glm::mat4>(), mPosition);
	glm::mat4 scale = glm::scale(translate, mScale);
	//glm::mat4 rotation = glm::rotate();
	

	return scale;
}
