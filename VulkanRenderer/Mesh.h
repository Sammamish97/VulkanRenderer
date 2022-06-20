#pragma once
#include <vector>
#include <glm/glm.hpp>
#include <vulkan/vulkan_core.h>
#include <array>

struct VulkanDevice;

struct Vertex
{
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 UV;

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
	//TODO: 소멸자 혹은 destroy를 통해 Buffer들을 할당 해제 해야함!
public:
	VkBuffer vertexBuffer;
	VkDeviceMemory vertexBufferMemory;

	int vertexNum = 0;
	int faceNum = 0;
private:
	VkDevice logicalDevice;
};