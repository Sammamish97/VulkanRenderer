#include "VulkanDevice.h"
#include "VulkanTools.h"

#include <iostream>
#include <unordered_set>

VulkanDevice::VulkanDevice(VkPhysicalDevice physicalDevice)
{
	assert(physicalDevice);
	this->physicalDevice = physicalDevice;

	vkGetPhysicalDeviceProperties(physicalDevice, &properties);
	vkGetPhysicalDeviceFeatures(physicalDevice, &features);
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

	uint32_t queueFamilyCount;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
	assert(queueFamilyCount > 0);
	queueFamilyProperties.resize(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilyProperties.data());

	uint32_t extCount = 0;
	vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, nullptr);
	if (extCount > 0)
	{
		std::vector<VkExtensionProperties> extensions(extCount);
		if (vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, &extensions.front()) == VK_SUCCESS)
		{
			for (auto ext : extensions)
			{
				supportedExtensions.push_back(ext.extensionName);
			}
		}
	}
}

VulkanDevice::~VulkanDevice()
{
	if(commandPool)
	{
		vkDestroyCommandPool(logicalDevice, commandPool, nullptr);
	}
	if(logicalDevice)
	{
		vkDestroyDevice(logicalDevice, nullptr);
	}
}
/**
	* Get the index of a memory type that has all the requested property bits set
	*
	* @param typeBits Bit mask with bits set for each memory type supported by the resource to request for (from VkMemoryRequirements)
	* @param properties Bit mask of properties for the memory type to request
	* @param (Optional) memTypeFound Pointer to a bool that is set to true if a matching memory type has been found
	*
	* @return Index of the requested memory type
	*
	* @throw Throws an exception if memTypeFound is null and no memory type could be found that supports the requested properties
	*/
uint32_t VulkanDevice::getMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties, VkBool32* memTypeFound) const
{
	for(uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
	{
		if((typeBits & 1) == 1)
		{
			if((memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
			{
				if(memTypeFound)
				{
					*memTypeFound = true;
				}
				return i;
			}
		}
		typeBits >>= 1;
	}

	if(memTypeFound)
	{
		*memTypeFound = false;
		return 0;
	}
	else
	{
		throw std::runtime_error("Could not find a matching memory type");
	}
}

uint32_t VulkanDevice::getQueueFamilyIndex(VkQueueFlagBits queueFlags) const
{
	// Dedicated queue for compute
	// Try to find a queue family index that supports compute but not graphics
	if(queueFlags & VK_QUEUE_COMPUTE_BIT)
	{
		for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++)
		{
			if ((queueFamilyProperties[i].queueFlags & queueFlags) && ((queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0))
			{
				return i;
			}
		}
	}

	// Dedicated queue for transfer
	// Try to find a queue family index that supports transfer but not graphics and compute
	if (queueFlags & VK_QUEUE_TRANSFER_BIT)
	{
		for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++)
		{
			if ((queueFamilyProperties[i].queueFlags & queueFlags) && ((queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) && ((queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0))
			{
				return i;
			}
		}
	}

	// For other queue types or if no separate compute queue is present, return the first one to support the requested flags
	for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++)
	{
		if (queueFamilyProperties[i].queueFlags & queueFlags)
		{
			return i;
		}
	}

	throw std::runtime_error("Could not find a matching queue family index");
}

VkResult VulkanDevice::createLogicalDevice(VkPhysicalDeviceFeatures enabledFeatures,
	std::vector<const char*> enabledExtensions, void* pNextChain, bool useSwapChain, VkQueueFlags requestedQueueTypes)
{
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos{};

	const float defaultQueuePriority = 0.0f;

	if(requestedQueueTypes & VK_QUEUE_GRAPHICS_BIT)
	{
		queueFamilyIndices.graphics = getQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT);
		VkDeviceQueueCreateInfo queueInfo{};
		queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueInfo.queueFamilyIndex = queueFamilyIndices.graphics;
		queueInfo.queueCount = 1;
		queueInfo.pQueuePriorities = &defaultQueuePriority;
		queueCreateInfos.push_back(queueInfo);
	}
	else
	{
		queueFamilyIndices.graphics = 0;
	}

	// Dedicated compute queue
	if (requestedQueueTypes & VK_QUEUE_COMPUTE_BIT)
	{
		queueFamilyIndices.compute = getQueueFamilyIndex(VK_QUEUE_COMPUTE_BIT);
		if (queueFamilyIndices.compute != queueFamilyIndices.graphics)
		{
			// If compute family index differs, we need an additional queue create info for the compute queue
			VkDeviceQueueCreateInfo queueInfo{};
			queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueInfo.queueFamilyIndex = queueFamilyIndices.compute;
			queueInfo.queueCount = 1;
			queueInfo.pQueuePriorities = &defaultQueuePriority;
			queueCreateInfos.push_back(queueInfo);
		}
	}
	else
	{
		// Else we use the same queue
		queueFamilyIndices.compute = queueFamilyIndices.graphics;
	}

	if (requestedQueueTypes & VK_QUEUE_TRANSFER_BIT)
	{
		queueFamilyIndices.transfer = getQueueFamilyIndex(VK_QUEUE_TRANSFER_BIT);
		if ((queueFamilyIndices.transfer != queueFamilyIndices.graphics) && (queueFamilyIndices.transfer != queueFamilyIndices.compute))
		{
			// If compute family index differs, we need an additional queue create info for the compute queue
			VkDeviceQueueCreateInfo queueInfo{};
			queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueInfo.queueFamilyIndex = queueFamilyIndices.transfer;
			queueInfo.queueCount = 1;
			queueInfo.pQueuePriorities = &defaultQueuePriority;
			queueCreateInfos.push_back(queueInfo);
		}
	}
	else
	{
		// Else we use the same queue
		queueFamilyIndices.transfer = queueFamilyIndices.graphics;
	}

	std::vector<const char*> deviceExtensions(enabledExtensions);
	if(useSwapChain)
	{
		deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
	}

	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
	deviceCreateInfo.pEnabledFeatures = &enabledFeatures;

	VkPhysicalDeviceFeatures2 physicalDeviceFeatures2{};
	if(pNextChain)
	{
		physicalDeviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		physicalDeviceFeatures2.features = enabledFeatures;
		physicalDeviceFeatures2.pNext = pNextChain;
		deviceCreateInfo.pEnabledFeatures = nullptr;
		deviceCreateInfo.pNext = &physicalDeviceFeatures2;
	}

	if(extensionSupported(VK_EXT_DEBUG_MARKER_EXTENSION_NAME))
	{
		deviceExtensions.push_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
		enableDebugMarkers = true;
	}

	if(deviceExtensions.size() > 0)
	{
		for(const char* enabledExtension : deviceExtensions)
		{
			if(extensionSupported(enabledExtension) == false)
			{
				std::cerr << "Enabled device extension \"" << enabledExtension << "\" is not present at device level\n";
			}
		}
		deviceCreateInfo.enabledExtensionCount = (uint32_t)deviceExtensions.size();
		deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
	}

	this->enabledFeatures = enabledFeatures;

	VkResult result = vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &logicalDevice);
	if(result != VK_SUCCESS)
	{
		return result;
	}

	commandPool = createCommandPool(queueFamilyIndices.graphics);
	return result;
}

VkResult VulkanDevice::createBuffer(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags,
	VkDeviceSize size, VkBuffer* buffer, VkDeviceMemory* memory, void* data)
{
	VkBufferCreateInfo bufferCreateInfo{};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.usage = usageFlags;
	bufferCreateInfo.size = size;
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VK_CHECK_RESULT(vkCreateBuffer(logicalDevice, &bufferCreateInfo, nullptr, buffer));

	VkMemoryRequirements memReqs;
	VkMemoryAllocateInfo memAllocateInfo{};
	memAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	vkGetBufferMemoryRequirements(logicalDevice, *buffer, &memReqs);//TODO: memory required가 어떤 memory를 말하는것인지 알아야함.
	memAllocateInfo.allocationSize = memReqs.size;
	memAllocateInfo.memoryTypeIndex = getMemoryType(memReqs.memoryTypeBits, memoryPropertyFlags);
	VkMemoryAllocateFlagsInfoKHR allocFlagsInfo{};
	if(usageFlags & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
	{
		allocFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
		allocFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
		memAllocateInfo.pNext = &allocFlagsInfo;
	}
	VK_CHECK_RESULT(vkAllocateMemory(logicalDevice, &memAllocateInfo, nullptr, memory));

	if(data != nullptr)
	{
		void* mapped;
		VK_CHECK_RESULT(vkMapMemory(logicalDevice, *memory, 0, size, 0, &mapped));
		
		memcpy(mapped, data, size);
		if((memoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
		{
			VkMappedMemoryRange mappedRange{};
			mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
			mappedRange.memory = *memory;
			mappedRange.offset = 0;
			mappedRange.size = size;
			vkFlushMappedMemoryRanges(logicalDevice, 1, &mappedRange);
		}
		vkUnmapMemory(logicalDevice, *memory);
	}
	VK_CHECK_RESULT(vkBindBufferMemory(logicalDevice, *buffer, *memory, 0));
	return VK_SUCCESS;
}

VkResult VulkanDevice::createBuffer(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags,
	Buffer* buffer, VkDeviceSize size, void* data)
{
	buffer->device = logicalDevice;

	VkBufferCreateInfo bufferCreateInfo{};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.usage = usageFlags;
	bufferCreateInfo.size = size;
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VK_CHECK_RESULT(vkCreateBuffer(logicalDevice, &bufferCreateInfo, nullptr, &buffer->buffer));

	VkMemoryRequirements memReqs;
	VkMemoryAllocateInfo memAllocateInfo{};
	memAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	vkGetBufferMemoryRequirements(logicalDevice, buffer->buffer, &memReqs);//TODO: memory required가 어떤 memory를 말하는것인지 알아야함.
	memAllocateInfo.allocationSize = memReqs.size;
	memAllocateInfo.memoryTypeIndex = getMemoryType(memReqs.memoryTypeBits, memoryPropertyFlags);
	VkMemoryAllocateFlagsInfoKHR allocFlagsInfo{};
	if (usageFlags & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
	{
		allocFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
		allocFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
		memAllocateInfo.pNext = &allocFlagsInfo;
	}
	VK_CHECK_RESULT(vkAllocateMemory(logicalDevice, &memAllocateInfo, nullptr, &buffer->memory));
	

	buffer->alignment = memReqs.alignment;
	buffer->size = size;
	buffer->usageFlags = usageFlags;
	buffer->memoryPropertyFlags = memoryPropertyFlags;

	if(data != nullptr)
	{
		VK_CHECK_RESULT(buffer->map());
		memcpy(buffer->mapped, data, size);
		if((memoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
		{
			buffer->flush();
		}
		buffer->Unmap();
	}
	buffer->setupDescriptor();

	return buffer->bind();
}

/**
* Copy buffer data from src to dst using VkCmdCopyBuffer
*
* @param src Pointer to the source buffer to copy from
* @param dst Pointer to the destination buffer to copy to
* @param queue Pointer
* @param copyRegion (Optional) Pointer to a copy region, if NULL, the whole buffer is copied
*
* @note Source and destination pointers must have the appropriate transfer usage flags set (TRANSFER_SRC / TRANSFER_DST)
*/
void VulkanDevice::copyBuffer(Buffer* src, Buffer* dst, VkQueue queue, VkBufferCopy* copyRegion)
{
	assert(dst->size <= src->size);
	assert(src->buffer);
	VkCommandBuffer copyCmd = createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
	VkBufferCopy bufferCopy{};
	if(copyRegion == nullptr)
	{
		bufferCopy.size = src->size;
	}
	else
	{
		bufferCopy = *copyRegion;
	}

	vkCmdCopyBuffer(copyCmd, src->buffer, dst->buffer, 1, &bufferCopy);
	flushCommandBuffer(copyCmd, queue);
}

VkCommandPool VulkanDevice::createCommandPool(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags createFlags)
{
	VkCommandPoolCreateInfo cmdPoolInfo{};
	cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmdPoolInfo.queueFamilyIndex = queueFamilyIndex;
	cmdPoolInfo.flags = createFlags;
	VkCommandPool cmdPool;
	VK_CHECK_RESULT(vkCreateCommandPool(logicalDevice, &cmdPoolInfo, nullptr, &cmdPool));
	
	return cmdPool;
}

VkCommandBuffer VulkanDevice::createCommandBuffer(VkCommandBufferLevel level, VkCommandPool pool, bool begin)
{
	VkCommandBufferAllocateInfo cmdBufAllocateInfo{};
	cmdBufAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdBufAllocateInfo.commandPool = pool;
	cmdBufAllocateInfo.level = level;
	cmdBufAllocateInfo.commandBufferCount = 1;

	VkCommandBuffer cmdBuffer;
	VK_CHECK_RESULT(vkAllocateCommandBuffers(logicalDevice, &cmdBufAllocateInfo, &cmdBuffer));

	if(begin)
	{
		VkCommandBufferBeginInfo cmdBufInfo{};
		cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo));
	}

	return cmdBuffer;
}

VkCommandBuffer VulkanDevice::createCommandBuffer(VkCommandBufferLevel level, bool begin)
{
	return createCommandBuffer(level, commandPool, begin);
}

void VulkanDevice::flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, VkCommandPool pool, bool free)
{
	if(commandBuffer == VK_NULL_HANDLE)
	{
		return;
	}

	VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FLAGS_NONE;

	VkFence fence;
	VK_CHECK_RESULT(vkCreateFence(logicalDevice, &fenceInfo, nullptr, &fence));
	VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, fence));
	VK_CHECK_RESULT(vkWaitForFences(logicalDevice, 1, &fence, VK_TRUE, DEFAULT_FENCE_TIMEOUT));
	vkDestroyFence(logicalDevice, fence, nullptr);
	if(free)
	{
		vkFreeCommandBuffers(logicalDevice, pool, 1, &commandBuffer);
	}
}

void VulkanDevice::flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free)
{
	return flushCommandBuffer(commandBuffer, queue, commandPool, free);
}

bool VulkanDevice::extensionSupported(std::string extension)
{
	return (std::find(supportedExtensions.begin(), supportedExtensions.end(), extension) != supportedExtensions.end());
}

VkFormat VulkanDevice::getSupportedDepthFormat(bool checkSamplingSupport)
{
	std::vector<VkFormat> depthFormats = { VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT, VK_FORMAT_D16_UNORM };
	for (auto& format : depthFormats)
	{
		VkFormatProperties formatProperties;
		vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &formatProperties);
		// Format must support depth stencil attachment for optimal tiling
		if (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
		{
			if (checkSamplingSupport) {
				if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) {
					continue;
				}
			}
			return format;
		}
	}
	throw std::runtime_error("Could not find a matching depth format");
}
