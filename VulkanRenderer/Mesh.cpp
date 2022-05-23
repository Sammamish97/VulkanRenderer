#define TINYOBJLOADER_IMPLEMENTATION
#include "Mesh.h"
#include <tiny_obj_loader.h>
#include <iostream>

#include "VulkanDevice.h"

VkVertexInputBindingDescription Vertex::getBindingDescription()
{
	VkVertexInputBindingDescription bindingDescription{};
	bindingDescription.binding = 0;
	bindingDescription.stride = sizeof(Vertex);
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	return bindingDescription;
}

std::array<VkVertexInputAttributeDescription, 2> Vertex::getAttributeDescriptions()
{
	std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};
	attributeDescriptions[0].binding = 0;
	attributeDescriptions[0].location = 0;
	attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[0].offset = offsetof(Vertex, position);

	attributeDescriptions[1].binding = 0;
	attributeDescriptions[1].location = 1;
	attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[1].offset = offsetof(Vertex, color);

	/*attributeDescriptions[2].binding = 0;
	attributeDescriptions[2].location = 2;
	attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[2].offset = offsetof(Vertex, normal);*/

	return attributeDescriptions;
}

/*******************************************************************/

bool Mesh::loadFromObj(const char* filename)
{
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;

	std::string warning;
	std::string error;

	tinyobj::LoadObj(&attrib, &shapes, &materials, &warning, &error, filename, nullptr);

	if(warning.empty() == false)
	{
		std::cout << "WARN: " << warning << std::endl;
	}

	if (warning.empty() == false)
	{
		std::cerr << error << std::endl;
		return false;
	}

	for(size_t shape_idx = 0; shape_idx < shapes.size(); ++shape_idx)
	{
		size_t indexOffset = 0;
		for(size_t face_idx = 0; face_idx < shapes[shape_idx].mesh.num_face_vertices.size(); ++face_idx)
		{
			int fv = 3;

			for(size_t v = 0; v < fv; ++v)
			{
				tinyobj::index_t idx = shapes[shape_idx].mesh.indices[indexOffset + v];
				indices.push_back(idx.vertex_index);
			}
			indexOffset += fv;
		}

		size_t vertex_num = attrib.vertices.size() / 3.f;
		for(size_t vertex_idx = 0; vertex_idx < vertex_num; ++vertex_idx)
		{
			Vertex newVertex;

			glm::vec3 position;
			position.x = attrib.vertices[vertex_idx * 3 + 0];
			position.y = attrib.vertices[vertex_idx * 3 + 1];
			position.z = attrib.vertices[vertex_idx * 3 + 2];

			glm::vec3 color{ 0.3f };

			newVertex.position = position;
			newVertex.color = color;

			vertices.push_back(newVertex);
		}
	}
	return true;
}

void Mesh::createVertexBuffer(VulkanDevice* vulkanDevice)
{
	VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

	vulkanDevice->createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		bufferSize, &vertexBuffer, &vertexBufferMemory, vertices.data());
}

void Mesh::createIndexBuffer(VulkanDevice* vulkanDevice)
{
	VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

	vulkanDevice->createBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		bufferSize, &indexBuffer, &indexBufferMemory, indices.data());
}

bool Mesh::loadAndCreateMesh(const char* filename, VulkanDevice* vulkan_device)
{
	loadFromObj(filename);
	createVertexBuffer(vulkan_device);
	createIndexBuffer(vulkan_device);
	logicalDevice = vulkan_device->logicalDevice;
	return true;
}

Mesh::~Mesh()
{
	vkDestroyBuffer(logicalDevice, vertexBuffer, nullptr);
	vkFreeMemory(logicalDevice, vertexBufferMemory, nullptr);
	vkDestroyBuffer(logicalDevice, indexBuffer, nullptr);
	vkFreeMemory(logicalDevice, indexBufferMemory, nullptr);
}
