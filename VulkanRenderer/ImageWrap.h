#pragma once
#include <vulkan/vulkan.h>
struct ImageWrap
{
    uint32_t width, height;
    uint32_t mipLevels{};
	VkImage image{};
    VkFormat format{};
	VkDeviceMemory memory{};
	VkSampler sampler{};
	VkImageView imageView{};
	VkImageLayout imageLayout{};

    void destroy(VkDevice device)
    {
        vkDestroyImage(device, image, nullptr);
        vkFreeMemory(device, memory, nullptr);
        vkDestroyImageView(device, imageView, nullptr);
        vkDestroySampler(device, sampler, nullptr);
    }

    VkDescriptorImageInfo Descriptor() const
    {
        return VkDescriptorImageInfo({ sampler, imageView, imageLayout });
    }
};

