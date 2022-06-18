#pragma once
#include <vulkan/vulkan.h>
#include <vector>

struct VulkanDevice;

struct FramebufferAttachment
{
	VkImage image;
	VkDeviceMemory memory;
	VkImageView view;
	VkFormat format;
	VkImageSubresourceRange subresourceRange;
	VkAttachmentDescription description;

	bool HasDepth();
	bool HasStencil();
	bool IsDepthStencil();
};

struct AttachmentCreateInfo
{
	uint32_t width, height;
	uint32_t layerCount;
	VkFormat format;
	VkImageUsageFlags usage;
	VkSampleCountFlagBits imageSampleCount = VK_SAMPLE_COUNT_1_BIT;
};

struct FrameBuffer
{
private:
	VulkanDevice* vulkanDevice;

public:
	uint32_t width, height;
	VkFramebuffer framebuffer;
	VkRenderPass renderPass;
	VkSampler sampler;
	std::vector<FramebufferAttachment> attachments;

public:
	FrameBuffer(VulkanDevice* vulkanDevice);
	~FrameBuffer();

	uint32_t AddAttachment(AttachmentCreateInfo createInfo);
	VkResult CreateSampler(VkFilter magFilter, VkFilter minFilter, VkSamplerAddressMode addressMode);
	VkResult CreateRenderPass();

};

