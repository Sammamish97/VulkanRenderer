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
	//glm::vec3 normal;

	static VkVertexInputBindingDescription getBindingDescription();
	static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions();
};

struct Mesh
{
public:
	std::vector<Vertex> vertices;
	std::vector<uint16_t> indices;
	bool loadAndCreateMesh(const char* filename, VulkanDevice* vulkan_device);
	bool loadFromObj(const char* filename);
	void createVertexBuffer(VulkanDevice* vulkanDevice);
	void createIndexBuffer(VulkanDevice* vulkanDevice);
	~Mesh();
	//TODO: 소멸자 혹은 destroy를 통해 Buffer들을 할당 해제 해야함!
public:
	VkBuffer vertexBuffer;
	VkDeviceMemory vertexBufferMemory;
	VkBuffer indexBuffer;
	VkDeviceMemory indexBufferMemory;

private:
	VkDevice logicalDevice;
};