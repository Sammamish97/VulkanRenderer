#pragma once
#include <vector>
#include "vulkan/vulkan.h"
namespace initializers
{
	inline VkWriteDescriptorSet writeDescriptorSet(
		VkDescriptorSet dstSet,
		VkDescriptorType type,
		uint32_t binding,
		VkDescriptorImageInfo* imageInfo,
		uint32_t descriptorCount = 1)
	{
		VkWriteDescriptorSet writeDescriptorSet{};
		writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet.dstSet = dstSet;
		writeDescriptorSet.descriptorType = type;
		writeDescriptorSet.dstBinding = binding;
		writeDescriptorSet.pImageInfo = imageInfo;
		writeDescriptorSet.descriptorCount = descriptorCount;
		return writeDescriptorSet;
	}

	inline VkWriteDescriptorSet writeDescriptorSet(
		VkDescriptorSet dstSet,
		VkDescriptorType type,
		uint32_t binding,
		VkDescriptorBufferInfo* bufferInfo,
		uint32_t descriptorCount = 1)
	{
		VkWriteDescriptorSet writeDescriptorSet{};
		writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet.dstSet = dstSet;
		writeDescriptorSet.descriptorType = type;
		writeDescriptorSet.dstBinding = binding;
		writeDescriptorSet.pBufferInfo = bufferInfo;
		writeDescriptorSet.descriptorCount = descriptorCount;
		return writeDescriptorSet;
	}

	inline VkCommandBufferBeginInfo commandBufferBeginInfo()
	{
		VkCommandBufferBeginInfo cmdBufferBeginInfo{};
		cmdBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		return cmdBufferBeginInfo;
	}
}
