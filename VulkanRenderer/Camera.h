#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

enum Camera_Movement {
	FORWARD,
	BACKWARD,
	LEFT,
	RIGHT
};

class Camera
{
public:
	Camera(glm::vec3 pos = glm::vec3(0.f, 0.f, 0.f),
		glm::vec3 up = glm::vec3(0.f, 1.f, 0.f),
		float yaw = -90.f,
		float pitch = 0.f);

	glm::mat4 getViewMatrix();
	void ProcessKeyboard(Camera_Movement direction, float dt);
	void ProcessMouseMovement(float xoffset, float yoffset, bool constrainPitch = true);
	
public:
	glm::vec3 position;
	glm::vec3 front;
	glm::vec3 up;
	glm::vec3 right;
	glm::vec3 worldUp;

	float Yaw;
	float Pitch;

	float MovementSpeed;
	float MouseSensitivity;

private:
	void updateCameraVectors();
};

