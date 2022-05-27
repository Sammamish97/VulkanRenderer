#pragma once
#include <vector>
#include <glm/vec3.hpp>
#include <vulkan/vulkan_core.h>
#include <array>

struct VulkanDevice;

struct Vertex
{
	glm::vec3 position;
	glm::vec3 color;
	glm::vec3 normal;

	static VkVertexInputBindingDescription getBindingDescription();
	static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions();
};

struct Mesh
{
public:
	std::vector<Vertex> vertices;
	bool loadAndCreateMesh(const char* filename, VulkanDevice* vulkan_device, glm::vec3 assignedColor);
	bool loadFromObj(const char* filename, glm::vec3 assignedColor);
	void createVertexBuffer(VulkanDevice* vulkanDevice);
	~Mesh();
	//TODO: �Ҹ��� Ȥ�� destroy�� ���� Buffer���� �Ҵ� ���� �ؾ���!
public:
	VkBuffer vertexBuffer;
	VkDeviceMemory vertexBufferMemory;

private:
	VkDevice logicalDevice;
};