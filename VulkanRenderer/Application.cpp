#define STB_IMAGE_IMPLEMENTATION
#include "Application.h"
#include "VulkanTools.h"
#include "VulkanInitializers.hpp"
#include <stb_image.h>

#include "SwapChain.h"

#define TEX_DIM 2048
#define TEX_FILTER VK_FILTER_LINEAR
#define FB_DIM TEX_DIM

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
	const VkAllocationCallbacks* pAllocator,
	VkDebugUtilsMessengerEXT* pDebugMessenger)
{
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr)
	{
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	}
	else
	{
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
{
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr)
	{
		func(instance, debugMessenger, pAllocator);
	}
}

/*************************************************/

void HelloTriangleApplication::run()
{
	initWindow();
	initVulkan();
	mainLoop();
	cleanup();
}

void HelloTriangleApplication::initWindow()
{
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
	glfwSetWindowUserPointer(window, this);
	glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
	glfwSetCursorPosCallback(window, mouseCallback);
}

void HelloTriangleApplication::framebufferResizeCallback(GLFWwindow* window, int width, int height)
{
	auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window));
	app->framebufferResized = true;
}

void HelloTriangleApplication::initVulkan()
{
	createInstance();
	setupDebugMessenger();

	createDeviceModule();
	createSwapChain();

	loadMeshAndObjects();
	createLight();
	createCamera();

	//Create SwapChain here

	createGRenderPass();
	createGFramebuffer();
	
	createUniformBuffers();

	createDescriptorPool();
	createSampler();
	createDescriptorSetLayout();
	createDescriptorSet();

	createGraphicsPipelines();

	createCommandBuffers();
	
	createSyncObjects();
}

void HelloTriangleApplication::createDescriptorPool()
{
	VkDescriptorPoolSize matPoolsize{};
	matPoolsize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	matPoolsize.descriptorCount = 2;

	VkDescriptorPoolSize Lightpoolsize{};
	Lightpoolsize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	Lightpoolsize.descriptorCount = 2;

	VkDescriptorPoolSize GBufferAttachmentSize{};
	GBufferAttachmentSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	GBufferAttachmentSize.descriptorCount = 3;

	std::vector<VkDescriptorPoolSize> poolSizes = { matPoolsize, Lightpoolsize, GBufferAttachmentSize };

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();

	poolInfo.maxSets = 2;

	if (vkCreateDescriptorPool(vulkanDevice->logicalDevice, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create descriptor pool!");
	}
}

void HelloTriangleApplication::createDescriptorSet()
{
	VkDescriptorSetAllocateInfo lightingAllocInfo{};
	lightingAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	lightingAllocInfo.descriptorPool = descriptorPool;
	lightingAllocInfo.descriptorSetCount = 1;
	lightingAllocInfo.pSetLayouts = &LightDescriptorSetLayout;

	VkDescriptorImageInfo texPosDisc;
	texPosDisc.sampler = colorSampler;
	texPosDisc.imageView = mGFrameBuffer.position.view;
	texPosDisc.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkDescriptorImageInfo texNormalDisc;
	texNormalDisc.sampler = colorSampler;
	texNormalDisc.imageView = mGFrameBuffer.normal.view;
	texNormalDisc.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkDescriptorImageInfo texColorDisc;
	texColorDisc.sampler = colorSampler;
	texColorDisc.imageView = mGFrameBuffer.albedo.view;
	texColorDisc.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	// Deferred composition
	if (vkAllocateDescriptorSets(vulkanDevice->logicalDevice, &lightingAllocInfo, &LightingDescriptorSet) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate descriptor sets");
	}
	
	VkDescriptorBufferInfo LightbufferInfo{};
	LightbufferInfo.buffer = lightUBO.buffer;
	LightbufferInfo.offset = 0;
	LightbufferInfo.range = sizeof(UniformBufferLights);

	//Descriptor Sets for Lighting
	std::vector<VkWriteDescriptorSet> lightWriteDescriptorSets;
	lightWriteDescriptorSets = {
		initializers::writeDescriptorSet(LightingDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, &LightbufferInfo),
		initializers::writeDescriptorSet(LightingDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &texPosDisc),
		initializers::writeDescriptorSet(LightingDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, &texNormalDisc),
		initializers::writeDescriptorSet(LightingDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4, &texColorDisc),
	};

	vkUpdateDescriptorSets(vulkanDevice->logicalDevice, static_cast<uint32_t>(lightWriteDescriptorSets.size()), lightWriteDescriptorSets.data(), 0, nullptr);

	// G-Buffer description


	VkDescriptorSetAllocateInfo GAllocInfo{};
	GAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	GAllocInfo.descriptorPool = descriptorPool;
	GAllocInfo.descriptorSetCount = 1;
	GAllocInfo.pSetLayouts = &GDescriptorSetLayout;


	VkDescriptorBufferInfo GBufferInfo{};
	GBufferInfo.buffer = matUBO.buffer;
	GBufferInfo.offset = 0;
	GBufferInfo.range = sizeof(UniformBufferMat);

	if (vkAllocateDescriptorSets(vulkanDevice->logicalDevice, &GAllocInfo, &GBufferDescriptorSet) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate descriptor sets");
	}

	std::vector<VkWriteDescriptorSet> GBufWriteDescriptorSets;
	GBufWriteDescriptorSets = {
		initializers::writeDescriptorSet(GBufferDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &GBufferInfo)
	};
	
	vkUpdateDescriptorSets(vulkanDevice->logicalDevice, static_cast<uint32_t>(GBufWriteDescriptorSets.size()), GBufWriteDescriptorSets.data(), 0, nullptr);
}

void HelloTriangleApplication::createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
	VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory)
{
	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = format;
	imageInfo.tiling = tiling;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = usage;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateImage(vulkanDevice->logicalDevice, &imageInfo, nullptr, &image) != VK_SUCCESS) {
		throw std::runtime_error("failed to create image!");
	}

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(vulkanDevice->logicalDevice, image, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

	if (vkAllocateMemory(vulkanDevice->logicalDevice, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate image memory!");
	}

	vkBindImageMemory(vulkanDevice->logicalDevice, image, imageMemory, 0);
}

//void HelloTriangleApplication::createTextureImage()
//{
//	int texWidth, texHeight, texChannels;
//	stbi_uc* pixels = stbi_load("../textures/02.png", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
//	VkDeviceSize imageSize = texWidth * texHeight * 4;//The pixels are laid out row by row with 4 bytes per pixel
//	if(pixels == nullptr)
//	{
//		throw std::runtime_error("failed to load texture image!");
//	}
//	VkBuffer stagingBuffer;
//	VkDeviceMemory stagingBufferMemory;
//	vulkanDevice->createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
//		imageSize, &stagingBuffer, &stagingBufferMemory, pixels);
//
//	stbi_image_free(pixels);
//
//	createImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 
//		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage, textureImageMemory);
//
//	transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
//	copyBufferToImage(stagingBuffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
//	transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
//
//	vkDestroyBuffer(vulkanDevice->logicalDevice, stagingBuffer, nullptr);
//	vkFreeMemory(vulkanDevice->logicalDevice, stagingBufferMemory, nullptr);
//}

VkCommandBuffer HelloTriangleApplication::beginSingleTimeCommands()
{
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = vulkanDevice->commandPool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(vulkanDevice->logicalDevice, &allocInfo, &commandBuffer);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	return commandBuffer;
}

void HelloTriangleApplication::endSingleTimeCommands(VkCommandBuffer commandBuffer)
{
	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(graphicsQueue);

	vkFreeCommandBuffers(vulkanDevice->logicalDevice, vulkanDevice->commandPool, 1, &commandBuffer);//TODO: command buffer의 재사용
}


void HelloTriangleApplication::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
	VkCommandBuffer commandBuffer = beginSingleTimeCommands();

	VkBufferCopy copyRegion{};
	copyRegion.size = size;
	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

	endSingleTimeCommands(commandBuffer);
}


void HelloTriangleApplication::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout,
	VkImageLayout newLayout)
{
	VkCommandBuffer commandBuffer = beginSingleTimeCommands();
	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	VkPipelineStageFlags sourceStage;
	VkPipelineStageFlags destinationStage;

	if(newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
	{
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

		if(hasStencilComponent(format))
		{
			barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
	}
	else
	{
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	}

	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) 
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) 
	{
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) 
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	}
	else 
	{
		throw std::invalid_argument("unsupported layout transition!");
	}

	vkCmdPipelineBarrier(commandBuffer, 
		sourceStage, destinationStage,
		0,
		0, nullptr, 
		0, nullptr, 
		1, &barrier);

	endSingleTimeCommands(commandBuffer);
}

void HelloTriangleApplication::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
{
	VkCommandBuffer commandBuffer = beginSingleTimeCommands();
	VkBufferImageCopy region{};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;

	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;

	region.imageOffset = { 0, 0, 0 };
	region.imageExtent = { width, height, 1 };

	vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	endSingleTimeCommands(commandBuffer);
}

void HelloTriangleApplication::createDepthResources(VkImage depthImage, VkDeviceMemory memory)
{
	//TODO: Temporary code.
	VkFormat depthFormat = findDepthFormat();

	createImage(mSwapChain->mSwapChainExtent.width, mSwapChain->mSwapChainExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		depthImage, memory);
	auto depthImageView = createImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

	transitionImageLayout(depthImage, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
}

VkFormat HelloTriangleApplication::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling,
	VkFormatFeatureFlags features)
{
	for(VkFormat format : candidates)
	{
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(vulkanDevice->physicalDevice, format, &props);

		if(tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
		{
			return format;
		}
		else if(tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
		{
			return format;
		}
	}
	throw std::runtime_error("failed to find supported format!");
}

VkFormat HelloTriangleApplication::findDepthFormat()
{
	return findSupportedFormat({ VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

bool HelloTriangleApplication::hasStencilComponent(VkFormat format)
{
	return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

void HelloTriangleApplication::createUniformBuffers()
{
	VkDeviceSize MatbufferSize = sizeof(UniformBufferMat);
	vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &matUBO, MatbufferSize);

	VkDeviceSize LightbufferSize = sizeof(lightsData);
	vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &lightUBO, LightbufferSize);
}

void HelloTriangleApplication::createDescriptorSetLayout()
{
	//Binding 0: view/projection mat & view vector
	VkDescriptorSetLayoutBinding matLayoutBinding{};
	matLayoutBinding.binding = 0;
	matLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	matLayoutBinding.descriptorCount = 1;
	matLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	matLayoutBinding.pImmutableSamplers = nullptr;

	//Binding 1: view/projection mat & view vector
	VkDescriptorSetLayoutBinding lightLayoutBinding{};
	lightLayoutBinding.binding = 1;
	lightLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	lightLayoutBinding.descriptorCount = 1;
	lightLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	lightLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding positionTextureBinding{};
	positionTextureBinding.binding = 2;
	positionTextureBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	positionTextureBinding.descriptorCount = 1;
	positionTextureBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	positionTextureBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding normalTextureBinding{};
	normalTextureBinding.binding = 3;
	normalTextureBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	normalTextureBinding.descriptorCount = 1;
	normalTextureBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	normalTextureBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding albedoTextureBinding{};
	albedoTextureBinding.binding = 4;
	albedoTextureBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	albedoTextureBinding.descriptorCount = 1;
	albedoTextureBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	albedoTextureBinding.pImmutableSamplers = nullptr;

	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = { lightLayoutBinding, positionTextureBinding, normalTextureBinding, albedoTextureBinding };

	VkDescriptorSetLayoutCreateInfo lightDescriptorLayout = initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(vulkanDevice->logicalDevice, &lightDescriptorLayout, nullptr, &LightDescriptorSetLayout))

	VkPipelineLayoutCreateInfo LightpipelineLayoutCreateInfo = initializers::pipelineLayoutCreateInfo(&LightDescriptorSetLayout, 1);
	VK_CHECK_RESULT(vkCreatePipelineLayout(vulkanDevice->logicalDevice, &LightpipelineLayoutCreateInfo, nullptr, &LightPipelineLayout))

	//Create Lighting descriptor set layout and pipeilne
	std::vector<VkDescriptorSetLayoutBinding> GLayoutBinding = { matLayoutBinding };

	VkDescriptorSetLayoutCreateInfo GDescriptorLayout = initializers::descriptorSetLayoutCreateInfo(GLayoutBinding);

	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(vulkanDevice->logicalDevice, &GDescriptorLayout, nullptr, &GDescriptorSetLayout))
	VkPushConstantRange pushConstant;
	pushConstant.offset = 0;
	pushConstant.size = sizeof(glm::mat4);
	pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkPipelineLayoutCreateInfo GpipelineLayoutCreateInfo = initializers::pipelineLayoutCreateInfo(&GDescriptorSetLayout, 1);
	GpipelineLayoutCreateInfo.pushConstantRangeCount = 1;
	GpipelineLayoutCreateInfo.pPushConstantRanges = &pushConstant;

	VK_CHECK_RESULT(vkCreatePipelineLayout(vulkanDevice->logicalDevice, &GpipelineLayoutCreateInfo, nullptr, &GPipelineLayout))
}

uint32_t HelloTriangleApplication::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(vulkanDevice->physicalDevice, &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i)
	{
		if (typeFilter & (1 << i) &&
			(memProperties.memoryTypes[i].propertyFlags & properties))
		{
			return i;
		}
	}

	throw std::runtime_error("failed to find suitable memory type!");
}

void HelloTriangleApplication::cleanupSwapChain()
{
	/*for (size_t i = 0; i < mSwapChainFrameBufers.size(); i++) {
		vkDestroyFramebuffer(vulkanDevice->logicalDevice, mSwapChainFrameBufers[i].framebuffer, nullptr);
	}

	vkDestroyPipeline(vulkanDevice->logicalDevice, GBufferPipeline, nullptr);
	vkDestroyPipeline(vulkanDevice->logicalDevice, LightingPipeline, nullptr);

	vkDestroyPipelineLayout(vulkanDevice->logicalDevice, pipelineLayout, nullptr);
	vkDestroyRenderPass(vulkanDevice->logicalDevice, GFrameBuffer.renderPass, nullptr);
	vkDestroyRenderPass(vulkanDevice->logicalDevice, LightFrameBuffer.renderPass, nullptr);

	vkDestroyImageView(vulkanDevice->logicalDevice, depthImageView, nullptr);
	vkDestroyImage(vulkanDevice->logicalDevice, depthImage, nullptr);
	vkFreeMemory(vulkanDevice->logicalDevice, depthImageMemory, nullptr);

	for (size_t i = 0; i < swapChainImageViews.size(); i++) {
		vkDestroyImageView(vulkanDevice->logicalDevice, swapChainImageViews[i], nullptr);
	}

	vkDestroySwapchainKHR(vulkanDevice->logicalDevice, swapChain, nullptr);*/
}

void HelloTriangleApplication::createSyncObjects()
{
	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	if (vkCreateSemaphore(vulkanDevice->logicalDevice, &semaphoreInfo, nullptr, &GBufferComplete) != VK_SUCCESS ||
		vkCreateSemaphore(vulkanDevice->logicalDevice, &semaphoreInfo, nullptr, &renderComplete) != VK_SUCCESS ||
		vkCreateSemaphore(vulkanDevice->logicalDevice, &semaphoreInfo, nullptr, &presentComplete) != VK_SUCCESS ||
		vkCreateFence(vulkanDevice->logicalDevice, &fenceInfo, nullptr, &inFlightFence) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create semaphores!");
	}
}

void HelloTriangleApplication::createCommandBuffers()
{
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = vulkanDevice->commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;

	if (vkAllocateCommandBuffers(vulkanDevice->logicalDevice, &allocInfo, &GCommandBuffer) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate command buffers!");
	}

	if (vkAllocateCommandBuffers(vulkanDevice->logicalDevice, &allocInfo, &LightingCommandBuffer) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate command buffers!");
	}
}

void HelloTriangleApplication::loadMeshAndObjects()
{
	redMesh = new Mesh;
	redMesh->loadAndCreateMesh("../models/Sphere.obj", vulkanDevice, glm::vec3(0.5, 0.5, 0.5));

	greenMesh = new Mesh;
	greenMesh->loadAndCreateMesh("../models/Monkey.obj", vulkanDevice, glm::vec3(0.5, 0.5, 0.5));

	BlueMesh = new Mesh;
	BlueMesh->loadAndCreateMesh("../models/Torus.obj", vulkanDevice, glm::vec3(0.8, 0.8, 0.8));


	objects.push_back(new Object(redMesh, glm::vec3(0.f, 3.f, 0.f)));
	objects.push_back(new Object(greenMesh, glm::vec3(1.5f, 0.f, 0.f)));
	objects.push_back(new Object(BlueMesh, glm::vec3(-1.5f, 0.f, 0.f)));
}

void HelloTriangleApplication::createCamera()
{
	camera = new Camera(glm::vec3(0, 0, 5), glm::vec3(0, 1, 0));
}

void HelloTriangleApplication::createLight()
{
	lightsData.dir_light[0] = DirLight(glm::vec3(0.1f, 0.5f, 0.1f), glm::vec3(-0.5f, -0.5f, -0.5f));
	lightsData.dir_light[1] = DirLight(glm::vec3(0.5f, 0.1f, 0.1), glm::vec3(0.5f, 0.5f, 0.5f));
	lightsData.dir_light[2] = DirLight(glm::vec3(0.1f, 0.1f, 0.5f), glm::vec3(-0.5f, 0.5f, 0.5f));
}

void HelloTriangleApplication::processInput()
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);

	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
		camera->ProcessKeyboard(FORWARD, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
		camera->ProcessKeyboard(BACKWARD, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
		camera->ProcessKeyboard(LEFT, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
		camera->ProcessKeyboard(RIGHT, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
		camera->ProcessKeyboard(UP, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
		camera->ProcessKeyboard(DOWN, deltaTime);

}

void HelloTriangleApplication::mouseCallback(GLFWwindow* window, double xposIn, double yposIn)
{
	auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window));
	float xpos = static_cast<float>(xposIn);
	float ypos = static_cast<float>(yposIn);

	if (app->mouseInfo.firstMouse)
	{
		app->mouseInfo.lastX = xpos;
		app->mouseInfo.lastY = ypos;
		app->mouseInfo.firstMouse = false;
	}

	float xoffset = xpos - app->mouseInfo.lastX;
	float yoffset = app->mouseInfo.lastY - ypos; // reversed since y-coordinates go from bottom to top

	app->mouseInfo.lastX = xpos;
	app->mouseInfo.lastY = ypos;

	app->camera->ProcessMouseMovement(xoffset, yoffset);
}

void HelloTriangleApplication::BuildGCommandBuffers()
{
	VkCommandBufferBeginInfo cmdBufInfo = initializers::commandBufferBeginInfo();

	VkClearValue clearValues[2];
	clearValues[0].color = { {0.f, 0.f, 0.2f, 0.f} };
	clearValues[1].depthStencil = { 1.f, 0 };
	//TODO: Start from here. 
}

void HelloTriangleApplication::BuildLightingCommandsBuffers()
{
}

void HelloTriangleApplication::createGFramebuffer()
{
	std::array<VkImageView, 4> attachments;
	attachments[0] = mGFrameBuffer.position.view;
	attachments[1] = mGFrameBuffer.normal.view;
	attachments[2] = mGFrameBuffer.albedo.view;
	attachments[3] = mGFrameBuffer.depth.view;

	VkFramebufferCreateInfo fbufCreateInfo = {};
	fbufCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	fbufCreateInfo.pNext = NULL;
	fbufCreateInfo.renderPass = mGFrameBuffer.renderPass;
	fbufCreateInfo.pAttachments = attachments.data();
	fbufCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	fbufCreateInfo.width = mGFrameBuffer.width;
	fbufCreateInfo.height = mGFrameBuffer.height;
	fbufCreateInfo.layers = 1;
	VK_CHECK_RESULT(vkCreateFramebuffer(vulkanDevice->logicalDevice, &fbufCreateInfo, nullptr, &mGFrameBuffer.framebuffer));
}

void HelloTriangleApplication::createSampler()
{
	VkSamplerCreateInfo sampler = initializers::samplerCreateInfo();
	sampler.magFilter = VK_FILTER_NEAREST;
	sampler.minFilter = VK_FILTER_NEAREST;
	sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler.addressModeV = sampler.addressModeU;
	sampler.addressModeW = sampler.addressModeU;
	sampler.mipLodBias = 0.0f;
	sampler.maxAnisotropy = 1.0f;
	sampler.minLod = 0.0f;
	sampler.maxLod = 1.0f;
	sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	VK_CHECK_RESULT(vkCreateSampler(vulkanDevice->logicalDevice, &sampler, nullptr, &colorSampler));
}

void HelloTriangleApplication::createGRenderPass()
{
	//Create Render Pass for GBuffer
	mGFrameBuffer.width = FB_DIM;
	mGFrameBuffer.height = FB_DIM;

	// (World space) Positions
	createAttachment(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &mGFrameBuffer.position);

	// (World space) Normals
	createAttachment(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &mGFrameBuffer.normal);

	// Albedo (color)
	createAttachment(VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &mGFrameBuffer.albedo);

	VkFormat attDepthFormat = findDepthFormat();

	// Depth
	createAttachment(attDepthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, &mGFrameBuffer.depth);

	std::array<VkAttachmentDescription, 4> attachmentDescs = {};

	// Init attachment properties
	for (uint32_t i = 0; i < 4; ++i)
	{
		attachmentDescs[i].samples = VK_SAMPLE_COUNT_1_BIT;
		attachmentDescs[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachmentDescs[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachmentDescs[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachmentDescs[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		if (i == 3)//Deal with depth buffer
		{
			attachmentDescs[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachmentDescs[i].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}
		else
		{
			attachmentDescs[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachmentDescs[i].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}
	}

	attachmentDescs[0].format = mGFrameBuffer.position.format;
	attachmentDescs[1].format = mGFrameBuffer.normal.format;
	attachmentDescs[2].format = mGFrameBuffer.albedo.format;
	attachmentDescs[3].format = mGFrameBuffer.depth.format;

	std::vector<VkAttachmentReference> colorReferences;
	colorReferences.push_back({ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
	colorReferences.push_back({ 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
	colorReferences.push_back({ 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });

	VkAttachmentReference depthReference = {};
	depthReference.attachment = 3;
	depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.pColorAttachments = colorReferences.data();
	subpass.colorAttachmentCount = static_cast<uint32_t>(colorReferences.size());
	subpass.pDepthStencilAttachment = &depthReference;

	// Use subpass dependencies for attachment layout transitions
	std::array<VkSubpassDependency, 2> dependencies;

	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;//TODO: What's meaning for?
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;//먼저 color attachment로 붙여 MTR를 할수 있게 한다.
	dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;//Write한 color attachment를 read해서 다음 lighting shader에 넣는다.
	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.pAttachments = attachmentDescs.data();
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescs.size());
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 2;
	renderPassInfo.pDependencies = dependencies.data();
	VK_CHECK_RESULT(vkCreateRenderPass(vulkanDevice->logicalDevice, &renderPassInfo, nullptr, &mGFrameBuffer.renderPass));
}


void HelloTriangleApplication::createGraphicsPipelines()
{
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
		initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0,VK_FALSE);
	VkPipelineRasterizationStateCreateInfo rasterizationState =
		initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
	VkPipelineColorBlendAttachmentState blendAttachmentState =
		initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
	VkPipelineColorBlendStateCreateInfo colorBlendState =
		initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
	VkPipelineDepthStencilStateCreateInfo depthStencilState =
		initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
	VkPipelineViewportStateCreateInfo viewportState =
		initializers::pipelineViewportStateCreateInfo(1, 1, 0);
	VkPipelineMultisampleStateCreateInfo multisampleState =
		initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
	std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamicState =
		initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

	//Need to know swap chain index for get proper swap chain image.
	VkGraphicsPipelineCreateInfo pipelineCI = initializers::pipelineCreateInfo(LightPipelineLayout, mGlobalSwapChainRenderPass);
	pipelineCI.pInputAssemblyState = &inputAssemblyState;
	pipelineCI.pRasterizationState = &rasterizationState;
	pipelineCI.pColorBlendState = &colorBlendState;
	pipelineCI.pMultisampleState = &multisampleState;
	pipelineCI.pViewportState = &viewportState;
	pipelineCI.pDepthStencilState = &depthStencilState;
	pipelineCI.pDynamicState = &dynamicState;
	pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
	pipelineCI.pStages = shaderStages.data();

	rasterizationState.cullMode = VK_CULL_MODE_FRONT_BIT;
	shaderStages[0] = createShaderStageCreateInfo("../shaders/LightingVert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = createShaderStageCreateInfo("../shaders/LightingFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

	VkPipelineVertexInputStateCreateInfo emptyInput = initializers::pipelineVertexInputStateCreateInfo();
	pipelineCI.pVertexInputState = &emptyInput;
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(vulkanDevice->logicalDevice, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &LightingPipeline))
	//Finish create lighting graphics pipeline.
	//Sequently create G buffer graphics pipeline for utilize created pipeline parameters.
	
	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};

	auto bindingDescription = Vertex::getBindingDescription();
	auto attributeDescriptions = Vertex::getAttributeDescriptions();

	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

	pipelineCI.layout = GPipelineLayout;
	pipelineCI.pVertexInputState = &vertexInputInfo;
	rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;

	shaderStages[0] = createShaderStageCreateInfo("../shaders/GBufferVert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = createShaderStageCreateInfo("../shaders/GBufferFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);//TODO: GBuffer GLSL 새로 써야함.

	pipelineCI.renderPass = mGFrameBuffer.renderPass;

	// Blend attachment states required for all color attachments
		// This is important, as color write mask will otherwise be 0x0 and you
		// won't see anything rendered to the attachment
	std::array<VkPipelineColorBlendAttachmentState, 3> blendAttachmentStates = {
		initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE),
		initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE),
		initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE)
	};

	colorBlendState.attachmentCount = static_cast<uint32_t>(blendAttachmentStates.size());
	colorBlendState.pAttachments = blendAttachmentStates.data();

	VK_CHECK_RESULT(vkCreateGraphicsPipelines(vulkanDevice->logicalDevice, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &GBufferPipeline))
	// Finish create G-graphics pipeline
}

void HelloTriangleApplication::buildGCommandBuffer()
{
	GCommandBuffer = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);

	VkCommandBufferBeginInfo cmdBufInfo = initializers::commandBufferBeginInfo();

	// Clear values for all attachments written in the fragment shader
	std::array<VkClearValue, 4> clearValues;
	clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
	clearValues[1].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
	clearValues[2].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
	clearValues[3].depthStencil = { 1.0f, 0 };

	VkRenderPassBeginInfo renderPassBeginInfo = initializers::renderPassBeginInfo();
	renderPassBeginInfo.renderPass = mGFrameBuffer.renderPass;
	renderPassBeginInfo.framebuffer = mGFrameBuffer.framebuffer;
	renderPassBeginInfo.renderArea.extent.width = mGFrameBuffer.width;
	renderPassBeginInfo.renderArea.extent.height = mGFrameBuffer.height;
	renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassBeginInfo.pClearValues = clearValues.data();

	VK_CHECK_RESULT(vkBeginCommandBuffer(GCommandBuffer, &cmdBufInfo))

	vkCmdBeginRenderPass(GCommandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	VkViewport viewport = initializers::viewport((float)mGFrameBuffer.width, (float)mGFrameBuffer.height, 0.0f, 1.0f);
	vkCmdSetViewport(GCommandBuffer, 0, 1, &viewport);

	VkRect2D scissor = initializers::rect2D(mGFrameBuffer.width, mGFrameBuffer.height, 0, 0);
	vkCmdSetScissor(GCommandBuffer, 0, 1, &scissor);

	vkCmdBindPipeline(GCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, GBufferPipeline);

	vkCmdBindDescriptorSets(GCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, GPipelineLayout, 0, 1, &GBufferDescriptorSet, 0, nullptr);

	for (Object* object : objects)
	{
		VkBuffer vertexBuffers[] = { object->mMesh->vertexBuffer };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(GCommandBuffer, 0, 1, vertexBuffers, offsets);
		glm::mat4 modelMat = object->BuildModelMat();
		vkCmdPushConstants(GCommandBuffer, GPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &modelMat);
		vkCmdDraw(GCommandBuffer, static_cast<uint32_t>(object->mMesh->vertices.size()), 1, 0, 0);
	}

	vkCmdEndRenderPass(GCommandBuffer);

	VK_CHECK_RESULT(vkEndCommandBuffer(GCommandBuffer));
}

void HelloTriangleApplication::buildLightCommandBuffer(int swapChianIndex)
{
	VkCommandBufferBeginInfo cmdBufInfo = initializers::commandBufferBeginInfo();

	VkClearValue clearValues[2];
	clearValues[0].color = { {0.f, 0.f, 0.2f, 0.f} };
	clearValues[1].depthStencil = { 1.f, 0 };

	VkRenderPassBeginInfo renderPassBeginInfo = initializers::renderPassBeginInfo();
	renderPassBeginInfo.renderPass = mSwapChain->mSwapChainFrameBuffers[swapChianIndex].mRenderPass;
	renderPassBeginInfo.renderArea.offset.x = 0;
	renderPassBeginInfo.renderArea.offset.y = 0;
	renderPassBeginInfo.renderArea.extent.width = WIDTH;
	renderPassBeginInfo.renderArea.extent.height = HEIGHT;
	renderPassBeginInfo.clearValueCount = 2;
	renderPassBeginInfo.pClearValues = clearValues;

	renderPassBeginInfo.framebuffer = mSwapChain->mSwapChainFrameBuffers[swapChianIndex].mFramebuffer;

	VK_CHECK_RESULT(vkBeginCommandBuffer(LightingCommandBuffer, &cmdBufInfo))
	vkCmdBeginRenderPass(LightingCommandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
	VkViewport viewport = initializers::viewport((float)WIDTH, (float)HEIGHT, 0.f, 1.f);
	vkCmdSetViewport(LightingCommandBuffer, 0, 1, &viewport);
	VkRect2D scissor = initializers::rect2D(WIDTH, HEIGHT, 0, 0);
	vkCmdSetScissor(LightingCommandBuffer, 0, 1, &scissor);

	vkCmdBindDescriptorSets(LightingCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, LightPipelineLayout, 0, 1, &LightingDescriptorSet, 0, nullptr);

	vkCmdBindPipeline(LightingCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, LightingPipeline);

	vkCmdDraw(LightingCommandBuffer, 3, 1, 0, 0);

	vkCmdEndRenderPass(LightingCommandBuffer);

	VK_CHECK_RESULT(vkEndCommandBuffer(LightingCommandBuffer));
}

void HelloTriangleApplication::mainLoop()
{
	while (glfwWindowShouldClose(window) == false)
	{
		FrameStart();
		glfwPollEvents();
		processInput();
		Update();
		drawFrame();
		FrameEnd();
	}

	vkDeviceWaitIdle(vulkanDevice->logicalDevice);
}

void HelloTriangleApplication::drawFrame()
{
	vkWaitForFences(vulkanDevice->logicalDevice, 1, &inFlightFence, VK_TRUE, UINT64_MAX);

	uint32_t imageindex;
	VkResult result = vkAcquireNextImageKHR(vulkanDevice->logicalDevice, mSwapChain->mSwapChain, UINT64_MAX, presentComplete, VK_NULL_HANDLE, &imageindex);


	buildGCommandBuffer();
	buildLightCommandBuffer(imageindex);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized)
	{
		framebufferResized = false;
		//recreateSwapChain();
		return;
	}
	else if (result != VK_SUCCESS)
	{
		throw std::runtime_error("failed to acquire swap chain image!");
	}

	updateUniformBuffer(currentFrame);

	vkResetFences(vulkanDevice->logicalDevice, 1, &inFlightFence);

	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

	VkSubmitInfo GSubmitInfo = {};
	GSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	GSubmitInfo.pWaitSemaphores = &presentComplete;
	GSubmitInfo.waitSemaphoreCount = 1;
	GSubmitInfo.pSignalSemaphores = &GBufferComplete;
	GSubmitInfo.signalSemaphoreCount = 1;
	GSubmitInfo.commandBufferCount = 1;
	GSubmitInfo.pCommandBuffers = &GCommandBuffer;
	GSubmitInfo.pWaitDstStageMask = waitStages;

	VK_CHECK_RESULT(vkQueueSubmit(graphicsQueue, 1, &GSubmitInfo, nullptr))

	VkSubmitInfo lightSubmitInfo = {};
	lightSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	lightSubmitInfo.pWaitSemaphores = &GBufferComplete;
	lightSubmitInfo.waitSemaphoreCount = 1;
	lightSubmitInfo.pSignalSemaphores = &renderComplete;
	lightSubmitInfo.signalSemaphoreCount = 1;
	lightSubmitInfo.commandBufferCount = 1;
	lightSubmitInfo.pCommandBuffers = &LightingCommandBuffer;
	lightSubmitInfo.pWaitDstStageMask = waitStages;
	VK_CHECK_RESULT(vkQueueSubmit(graphicsQueue, 1, &lightSubmitInfo, inFlightFence))

	//

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &renderComplete;

	VkSwapchainKHR swapChains[] = { mSwapChain->mSwapChain };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &imageindex;

	presentInfo.pResults = nullptr;

	result = vkQueuePresentKHR(presentQueue, &presentInfo);

	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		//recreateSwapChain();
		return;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
	{
		throw std::runtime_error("failed to acquire swap chain image!");
	}
}

void HelloTriangleApplication::updateUniformBuffer(uint32_t currentImage)
{
	static auto startTime = std::chrono::high_resolution_clock::now();

	auto currentTime = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

	UniformBufferMat ubo{};
	ubo.view = camera->getViewMatrix();
	ubo.proj = glm::perspective(glm::radians(45.f), mSwapChain->mSwapChainExtent.width / (float)mSwapChain->mSwapChainExtent.height, 0.1f, 100.f);
	ubo.proj[1][1] *= -1;
	

	void* Matdata;
	vkMapMemory(vulkanDevice->logicalDevice, matUBO.memory, 0, sizeof(ubo), 0, &Matdata);
	memcpy(Matdata, &ubo, sizeof(ubo));
	vkUnmapMemory(vulkanDevice->logicalDevice, matUBO.memory);

	lightsData.lookVec = (camera->front - camera->position);
	void* Lightdata;
	vkMapMemory(vulkanDevice->logicalDevice, lightUBO.memory, 0, sizeof(UniformBufferLights), 0, &Lightdata);
	memcpy(Lightdata, &lightsData, sizeof(lightsData));
	vkUnmapMemory(vulkanDevice->logicalDevice, lightUBO.memory);
}

void HelloTriangleApplication::cleanup()
{
	//delete redMesh;
	//delete greenMesh;
	//delete BlueMesh;

	//cleanupSwapChain();

	///*vkDestroySampler(vulkanDevice->logicalDevice, textureSampler, nullptr);
	//vkDestroyImageView(vulkanDevice->logicalDevice, textureImageView, nullptr);

	//vkDestroyImage(vulkanDevice->logicalDevice, textureImage, nullptr);
	//vkFreeMemory(vulkanDevice->logicalDevice, textureImageMemory, nullptr);*/

	//matUBO.destroy();
	//lightUBO.destroy();

	//vkDestroyDescriptorPool(vulkanDevice->logicalDevice, descriptorPool, nullptr);

	//vkDestroyDescriptorSetLayout(vulkanDevice->logicalDevice, descriptorSetLayout, nullptr);

	//vkDestroySemaphore(vulkanDevice->logicalDevice, GBufferSemaphore, nullptr);
	//vkDestroySemaphore(vulkanDevice->logicalDevice, renderComplete, nullptr);
	//vkDestroySemaphore(vulkanDevice->logicalDevice, presentComplete, nullptr);
	//vkDestroyFence(vulkanDevice->logicalDevice, inFlightFence, nullptr);

	//if (enableValidationLayers)
	//{
	//	DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
	//}
	//vkDestroySurfaceKHR(instance, surface, nullptr);
	//delete vulkanDevice;

	//vkDestroyInstance(instance, nullptr);

	//glfwDestroyWindow(window);

	//glfwTerminate();
}

void HelloTriangleApplication::CreateSurface()
{
	VK_CHECK_RESULT(glfwCreateWindowSurface(instance, window, nullptr, &mSurface))
}

SwapChainSupportDetails HelloTriangleApplication::QuerySwapChainSupport(VkPhysicalDevice physicalDevice)
{
	SwapChainSupportDetails details;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, mSurface, &details.capabilities);

	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, mSurface, &formatCount, nullptr);

	if (formatCount != 0)
	{
		details.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, mSurface, &formatCount, details.formats.data());
	}

	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, mSurface, &presentModeCount, nullptr);

	if (presentModeCount != 0)
	{
		details.presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, mSurface, &presentModeCount, details.presentModes.data());
	}
	return details;
}

VkImageView HelloTriangleApplication::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
{
	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = format;
	viewInfo.subresourceRange.aspectMask = aspectFlags;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	VkImageView imageView;
	if (vkCreateImageView(vulkanDevice->logicalDevice, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
		throw std::runtime_error("failed to create texture image view!");
	}

	return imageView;
}

bool HelloTriangleApplication::isDeviceSuitable(VkPhysicalDevice device)
{
	bool extensionsSupported = checkDeviceExtensionSupport(device);

	bool swapChainAdequate = false;
	if (extensionsSupported)
	{
		SwapChainSupportDetails swapChainSupport =QuerySwapChainSupport(device);
		swapChainAdequate = (swapChainSupport.formats.empty() == false) && (swapChainSupport.presentModes.empty() == false);
	}
	return extensionsSupported && swapChainAdequate;
}

bool HelloTriangleApplication::checkDeviceExtensionSupport(VkPhysicalDevice device)
{
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

	std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

	for (const auto& extension : availableExtensions)
	{
		requiredExtensions.erase(extension.extensionName);
	}
	return requiredExtensions.empty();
}

VkPhysicalDevice HelloTriangleApplication::pickPhysicalDevice()
{
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

	if (deviceCount == 0)
	{
		throw std::runtime_error("failed to find GPUs with Vulkan support!");
	}

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
	VkPhysicalDevice availableDevice{};
	for (const auto& device : devices)
	{
		if (isDeviceSuitable(device))
		{
			availableDevice = device;
			break;
		}
	}

	if (availableDevice == VK_NULL_HANDLE)
	{
		throw std::runtime_error("failed to find suitable GPU!");
	}

	return availableDevice;
}

void HelloTriangleApplication::createDeviceModule()
{
	vulkanDevice = new VulkanDevice(pickPhysicalDevice());

	VkPhysicalDeviceFeatures deviceFeatures{};
	deviceFeatures.samplerAnisotropy = VK_TRUE;

	VkResult res = vulkanDevice->createLogicalDevice(deviceFeatures, deviceExtensions, nullptr);
	if(res == VK_FALSE)
	{
		assert("Failed to create Logical Device!");
	}

	vkGetDeviceQueue(vulkanDevice->logicalDevice, vulkanDevice->getQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT), 0, &graphicsQueue);
	presentQueue = graphicsQueue;
}

void HelloTriangleApplication::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
{
	createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
		| VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
		| VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
		| VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

	createInfo.pfnUserCallback = debugCallback;
}

void HelloTriangleApplication::setupDebugMessenger()
{
	if (!enableValidationLayers)
		return;

	VkDebugUtilsMessengerCreateInfoEXT createInfo{};
	populateDebugMessengerCreateInfo(createInfo);

	if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to set up debug messenger!");
	}
}

void HelloTriangleApplication::createInstance()
{
	if (enableValidationLayers && checkValidationLayerSupport() == false)
	{
		throw std::runtime_error("validation layers requested, but not available!");
	}
	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Hello Triangle";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "No Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	auto extensions = getRequiredExtensions();
	createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.data();

	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
	if (enableValidationLayers)
	{
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();

		populateDebugMessengerCreateInfo(debugCreateInfo);
		createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
	}
	else
	{
		createInfo.enabledLayerCount = 0;
		createInfo.pNext = nullptr;
	}

	if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create instance!");
	}
}

void HelloTriangleApplication::createSwapChain()
{
	mSwapChain = new SwapChain(this);
}


void HelloTriangleApplication::checkExtensions()
{
	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
	std::vector<VkExtensionProperties> extensions(extensionCount);

	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());
	std::cout << "available extensions:\n";

	for (const auto& extension : extensions) {
		std::cout << '\t' << extension.extensionName << '\n';
	}
}

bool HelloTriangleApplication::checkValidationLayerSupport()
{
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	for (const char* layerName : validationLayers)
	{
		bool layerFound = false;

		for (const auto& layerProperties : availableLayers)
		{
			if (strcmp(layerName, layerProperties.layerName) == false)
			{
				layerFound = true;
				break;
			}
		}

		if (layerFound == false)
		{
			return false;
		}
	}
	return true;
}

std::vector<const char*> HelloTriangleApplication::getRequiredExtensions()
{
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

	if (enableValidationLayers)
	{
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}
	return extensions;
}

VKAPI_ATTR VkBool32 VKAPI_CALL HelloTriangleApplication::debugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData)
{
	std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
	return VK_FALSE;
}

void HelloTriangleApplication::FrameStart()
{
	frameStart = std::chrono::system_clock::now();
}

void HelloTriangleApplication::Update()
{
}

void HelloTriangleApplication::FrameEnd()
{
	frameEnd = std::chrono::system_clock::now();
	deltaTime = std::chrono::duration<float>(frameEnd - frameStart).count();
}

void HelloTriangleApplication::createAttachment(VkFormat format, VkImageUsageFlagBits usage,
	FrameBufferAttachment* attachment)
{
	VkImageAspectFlags aspectMask = 0;
	VkImageLayout imageLayout;

	attachment->format = format;

	if(usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
	{
		aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	}
	if(usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
	{
		aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	}

	assert(aspectMask > 0);

	VkImageCreateInfo image{};
	image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image.imageType = VK_IMAGE_TYPE_2D;
	image.format = format;
	image.extent.width = mGFrameBuffer.width;
	image.extent.height = mGFrameBuffer.height;
	image.extent.depth = 1;
	image.mipLevels = 1;
	image.arrayLayers = 1;
	image.samples = VK_SAMPLE_COUNT_1_BIT;
	image.tiling = VK_IMAGE_TILING_OPTIMAL;
	image.usage = usage | VK_IMAGE_USAGE_SAMPLED_BIT;

	VkMemoryAllocateInfo memAlloc{};
	memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;

	VkMemoryRequirements memReqs;

	VK_CHECK_RESULT(vkCreateImage(vulkanDevice->logicalDevice, &image, nullptr, &attachment->image));
	vkGetImageMemoryRequirements(vulkanDevice->logicalDevice, attachment->image, &memReqs);
	memAlloc.allocationSize = memReqs.size;
	memAlloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(vulkanDevice->logicalDevice, &memAlloc, nullptr, &attachment->memory));
	VK_CHECK_RESULT(vkBindImageMemory(vulkanDevice->logicalDevice, attachment->image, attachment->memory, 0));

	VkImageViewCreateInfo imageView = initializers::imageViewCreateInfo();
	imageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageView.format = format;
	imageView.subresourceRange = {};
	imageView.subresourceRange.aspectMask = aspectMask;
	imageView.subresourceRange.baseMipLevel = 0;
	imageView.subresourceRange.levelCount = 1;
	imageView.subresourceRange.baseArrayLayer = 0;
	imageView.subresourceRange.layerCount = 1;
	imageView.image = attachment->image;
	VK_CHECK_RESULT(vkCreateImageView(vulkanDevice->logicalDevice, &imageView, nullptr, &attachment->view));
}

void HelloTriangleApplication::PrepareGBuffer()
{
	mGFrameBuffer.width = FB_DIM;
	mGFrameBuffer.height = FB_DIM;

	createAttachment(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &mGFrameBuffer.position);
	createAttachment(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &mGFrameBuffer.normal);
	createAttachment(VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &mGFrameBuffer.albedo);

	VkFormat depthAttFormat;
	VkFormat attachmentDepthFormat = findDepthFormat();

	createAttachment(attachmentDepthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, &mGFrameBuffer.depth);

	std::array<VkAttachmentDescription, 4> attachmentDescs{};

	for(uint32_t i = 0; i < 4; ++i)
	{
		attachmentDescs[i].samples = VK_SAMPLE_COUNT_1_BIT;
		attachmentDescs[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachmentDescs[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachmentDescs[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachmentDescs[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		if(i == 3)
		{
			attachmentDescs[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachmentDescs[i].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}
		else
		{
			attachmentDescs[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachmentDescs[i].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}
	}

	attachmentDescs[0].format = mGFrameBuffer.position.format;
	attachmentDescs[1].format = mGFrameBuffer.normal.format;
	attachmentDescs[2].format = mGFrameBuffer.albedo.format;
	attachmentDescs[3].format = mGFrameBuffer.depth.format;

	std::vector<VkAttachmentReference> colorReferences;
	colorReferences.push_back({ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
	colorReferences.push_back({ 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
	colorReferences.push_back({ 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });

	VkAttachmentReference depthReference = {};
	depthReference.attachment = 3;
	depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.pColorAttachments = colorReferences.data();
	subpass.colorAttachmentCount = static_cast<uint32_t>(colorReferences.size());
	subpass.pDepthStencilAttachment = &depthReference;

	std::array<VkSubpassDependency, 2> dependencies;
	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.pAttachments = attachmentDescs.data();
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescs.size());
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 2;
	renderPassInfo.pDependencies = dependencies.data();

	VK_CHECK_RESULT(vkCreateRenderPass(vulkanDevice->logicalDevice, &renderPassInfo, nullptr, &mGFrameBuffer.renderPass));

	std::array<VkImageView, 4> attachments;
	attachments[0] = mGFrameBuffer.position.view;
	attachments[1] = mGFrameBuffer.normal.view;
	attachments[2] = mGFrameBuffer.albedo.view;
	attachments[3] = mGFrameBuffer.depth.view;

	VkFramebufferCreateInfo frameBufferCreateInfo = {};
	frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	frameBufferCreateInfo.pNext = nullptr;
	frameBufferCreateInfo.renderPass = mGFrameBuffer.renderPass;
	frameBufferCreateInfo.pAttachments = attachments.data();
	frameBufferCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	frameBufferCreateInfo.width = mGFrameBuffer.width;
	frameBufferCreateInfo.height = mGFrameBuffer.height;
	frameBufferCreateInfo.layers = 1;
	VK_CHECK_RESULT(vkCreateFramebuffer(vulkanDevice->logicalDevice, &frameBufferCreateInfo, nullptr, &mGFrameBuffer.framebuffer));

	VkSamplerCreateInfo sampler = {};
	sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sampler.maxAnisotropy = 1.f;
	sampler.magFilter = VK_FILTER_NEAREST;
	sampler.minFilter = VK_FILTER_NEAREST;
	sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler.addressModeV = sampler.addressModeU;
	sampler.addressModeW = sampler.addressModeU;
	sampler.mipLodBias = 0.0f;
	sampler.maxAnisotropy = 1.0f;
	sampler.minLod = 0.0f;
	sampler.maxLod = 1.0f;
	sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	VK_CHECK_RESULT(vkCreateSampler(vulkanDevice->logicalDevice, &sampler, nullptr, &colorSampler));
}