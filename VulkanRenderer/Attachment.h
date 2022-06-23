#pragma once
#include "vulkan/vulkan.hpp"
struct FrameBufferAttachment
{
	VkImage image;
	VkDeviceMemory memory;
	VkImageView view;
	VkFormat format;
};