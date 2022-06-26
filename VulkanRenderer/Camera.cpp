#include "Camera.h"

#include <iostream>

Camera::Camera(glm::vec3 pos, glm::vec3 up, float yaw, float pitch)
	:front(glm::vec3(0.f, 0.f, -1.f)), MovementSpeed(2.5f), MouseSensitivity(0.25f)
{
	position = pos;
	worldUp = up;
	Yaw = yaw;
	Pitch = pitch;
	updateCameraVectors();
}

glm::mat4 Camera::getViewMatrix()
{
	return glm::lookAt(position, position + front, up);
}

void Camera::ProcessKeyboard(Camera_Movement direction, float dt)
{
	float velocity = MovementSpeed * dt;
	if (direction == FORWARD)
		position += front * velocity;
	if (direction == BACKWARD)
		position -= front * velocity;
	if (direction == LEFT)
		position -= right * velocity;
	if (direction == RIGHT)
		position += right * velocity;
	if (direction == UP)
		position += up * velocity;
	if (direction == DOWN)
		position -= up * velocity;
}

void Camera::ProcessMouseMovement(float xoffset, float yoffset, bool constrainPitch)
{
	xoffset *= MouseSensitivity;
	yoffset *= MouseSensitivity;

	Yaw += xoffset;
	Pitch += yoffset;

	// make sure that when pitch is out of bounds, screen doesn't get flipped
	if (constrainPitch)
	{
		if (Pitch > 89.0f)
			Pitch = 89.0f;
		if (Pitch < -89.0f)
			Pitch = -89.0f;
	}

	// update Front, Right and Up Vectors using the updated Euler angles
	updateCameraVectors();
}

void Camera::updateCameraVectors()
{
	glm::vec3 newFront;
	newFront.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
	newFront.y = sin(glm::radians(Pitch));
	newFront.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
	front = glm::normalize(newFront);
	// also re-calculate the Right and Up vector
	right = glm::normalize(glm::cross(front, worldUp));  // normalize the vectors, because their length gets closer to 0 the more you look up or down which results in slower movement.

	glm::vec3 temp_up = glm::normalize(glm::cross(front, right));
	if(temp_up.y < 0.f)
	{
		up = -temp_up;
	}
	else
	{
		up = temp_up;
	}
}
