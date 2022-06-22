#pragma once
#include <string>
#include <vulkan/vulkan.h>

#include <ktx.h>
#include <ktxvulkan.h>
class VulkanDevice;
class Texture
{
public:
	VulkanDevice* device;
	VkImage               image;
	VkImageLayout         imageLayout;
	VkDeviceMemory        deviceMemory;
	VkImageView           view;
	uint32_t              width, height;
	uint32_t              mipLevels;
	uint32_t              layerCount;
	VkDescriptorImageInfo descriptor;
	VkSampler             sampler;

	void      updateDescriptor();
	void      destroy();
	ktxResult loadKTXFile(std::string filename, ktxTexture** target);
};

class TextureCubeMap : public Texture
{
public:
	void loadFromFile(
		std::string        filename,
		VkFormat           format,
		VulkanDevice* device,
		VkQueue            copyQueue,
		VkImageUsageFlags  imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
		VkImageLayout      imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
};