#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

#include <ktxvulkan.h>

#include "Demo.h"

#include "VulkanTools.h"
#include "VulkanInitializers.hpp"

void Demo::run()
{
	Init();
	Update();
	CleanUp();
}

void Demo::Init()
{
	VkApp::Init();
	SetupCallBacks();
	
	LoadMeshAndObjects();
	LoadTextures();
	CreateLight();
	LoadCubemap();
	CreateCamera();
	CreateSyncObjects();

	mGFrameBuffer.Init(this);
	mLFrameBuffer.Init(this);
	mPFrameBuffer.Init(this, &mGFrameBuffer.depth, &mLFrameBuffer.composition);

	CreateUniformBuffers();

	CreateDescriptorPool();
	CreateSampler();
	CreateDescriptorSetLayout();
	CreateDescriptorSet();

	CreateGraphicsPipelines();
	CreateCommandBuffers();

	InitGUI();
}

void Demo::Update()
{
	while (glfwWindowShouldClose(mWindow) == false)
	{
		FrameStart();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		DrawGUI();
		VkApp::Update();
		ProcessInput();

		Draw();

		FrameEnd();
	}

	vkDeviceWaitIdle(mVulkanDevice->logicalDevice);
}

void Demo::Draw()
{
	VkApp::Draw();
	vkWaitForFences(mVulkanDevice->logicalDevice, 1, &inFlightFence, VK_TRUE, UINT64_MAX);

	uint32_t imageindex;
	VkResult result = vkAcquireNextImageKHR(mVulkanDevice->logicalDevice, mSwapChain->mSwapChain, UINT64_MAX, presentComplete, VK_NULL_HANDLE, &imageindex);


	BuildGCommandBuffer();
	BuildLightCommandBuffer();//TODO: Can transfer to Init. Not draw.
	

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

	UpdateUniformBuffer(currentFrame);

	vkResetFences(mVulkanDevice->logicalDevice, 1, &inFlightFence);

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

	VK_CHECK_RESULT(vkQueueSubmit(mGraphicsQueue, 1, &GSubmitInfo, nullptr))

	VkSubmitInfo lightSubmitInfo = {};
	lightSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	lightSubmitInfo.pWaitSemaphores = &GBufferComplete;
	lightSubmitInfo.waitSemaphoreCount = 1;
	lightSubmitInfo.pSignalSemaphores = &renderComplete;
	lightSubmitInfo.signalSemaphoreCount = 1;
	lightSubmitInfo.commandBufferCount = 1;
	lightSubmitInfo.pCommandBuffers = &LightingCommandBuffer;
	lightSubmitInfo.pWaitDstStageMask = waitStages;
	VK_CHECK_RESULT(vkQueueSubmit(mGraphicsQueue, 1, &lightSubmitInfo, nullptr))

	
	BuildPostCommandBuffer(0);
	VkSubmitInfo postSubmitInfo = {};
	postSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	postSubmitInfo.pWaitSemaphores = &renderComplete;
	postSubmitInfo.waitSemaphoreCount = 1;
	postSubmitInfo.pSignalSemaphores = &PostComplete;
	postSubmitInfo.signalSemaphoreCount = 1;
	postSubmitInfo.commandBufferCount = 1;
	postSubmitInfo.pCommandBuffers = &PostCommandBuffer;
	postSubmitInfo.pWaitDstStageMask = waitStages;
	VK_CHECK_RESULT(vkQueueSubmit(mGraphicsQueue, 1, &postSubmitInfo, inFlightFence))

	CopyImage(mPFrameBuffer.colorResult->image, VK_ACCESS_MEMORY_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			mSwapChain->mSwapChainRenderDatas[imageindex].mFrameBufferData.mColorAttachment.image, VK_ACCESS_MEMORY_READ_BIT, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

	VkPresentInfoKHR presentInfo {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &PostComplete;

	VkSwapchainKHR swapChains[] = { mSwapChain->mSwapChain };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &imageindex;

	presentInfo.pResults = nullptr;

	result = vkQueuePresentKHR(mPresentQueue, &presentInfo);

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

void Demo::CleanUp()
{
	VkApp::CleanUp();
}

void Demo::SetupCallBacks()
{
	glfwSetWindowUserPointer(mWindow, this);
	glfwSetCursorPosCallback(mWindow, MouseCallBack);
}

void Demo::MouseCallBack(GLFWwindow* window, double xposIn, double yposIn)
{
	auto app = reinterpret_cast<Demo*>(glfwGetWindowUserPointer(window));
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

	if(glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT))
		app->camera->ProcessMouseMovement(xoffset, yoffset);
}

void Demo::LoadMeshAndObjects()
{
	redMesh = new Mesh;
	redMesh->loadAndCreateMesh("../models/Sphere.obj", mVulkanDevice, glm::vec3(0.5, 0.5, 0.5));

	greenMesh = new Mesh;
	greenMesh->loadAndCreateMesh("../models/Plane.obj", mVulkanDevice, glm::vec3(0.5, 0.5, 0.5));

	BlueMesh = new Mesh;
	BlueMesh->loadAndCreateMesh("../models/Torus.obj", mVulkanDevice, glm::vec3(0.8, 0.8, 0.8));


	objects.push_back(new Object(redMesh, glm::vec3(0.f, 0.f, 3.f)));
	objects.push_back(new Object(greenMesh, glm::vec3(3.f, 0.f, 0.f)));
	objects.push_back(new Object(BlueMesh, glm::vec3(-3.f, 0.f, 0.f)));
}

void Demo::LoadCubemap()
{
	ktxResult result;
	ktxTexture* ktxTexture;

	std::string filename = "../texture/cubemap_yokohama_rgba.ktx";

	if (!vks::tools::fileExists(filename)) 
	{
		assert("Failed to load cubemap");
	}
	result = ktxTexture_CreateFromNamedFile(filename.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxTexture);
	assert(result == KTX_SUCCESS);

	// Get properties required for using and upload texture data from the ktx texture object
	testCubemap.width = ktxTexture->baseWidth;
	testCubemap.height = ktxTexture->baseHeight;
	testCubemap.mipLevels = ktxTexture->numLevels;
	ktx_uint8_t* ktxTextureData = ktxTexture_GetData(ktxTexture);
	ktx_size_t ktxTextureSize = ktxTexture_GetDataSize(ktxTexture);

	VkMemoryAllocateInfo memAllocInfo = initializers::memoryAllocateInfo();
	VkMemoryRequirements memReqs;

	//// Create a host-visible staging buffer that contains the raw image data
	//VkBuffer stagingBuffer;
	//VkDeviceMemory stagingMemory;

	//VkBufferCreateInfo bufferCreateInfo = initializers::bufferCreateInfo();
	//bufferCreateInfo.size = ktxTextureSize;
	//// This buffer is used as a transfer source for the buffer copy
	//bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	//bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	//VK_CHECK_RESULT(vkCreateBuffer(device, &bufferCreateInfo, nullptr, &stagingBuffer));

	//// Get memory requirements for the staging buffer (alignment, memory type bits)
	//vkGetBufferMemoryRequirements(device, stagingBuffer, &memReqs);
	//memAllocInfo.allocationSize = memReqs.size;
	//// Get memory type index for a host visible buffer
	//memAllocInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	//VK_CHECK_RESULT(vkAllocateMemory(device, &memAllocInfo, nullptr, &stagingMemory));
	//VK_CHECK_RESULT(vkBindBufferMemory(device, stagingBuffer, stagingMemory, 0));

	//// Copy texture data into staging buffer
	//uint8_t* data;
	//VK_CHECK_RESULT(vkMapMemory(device, stagingMemory, 0, memReqs.size, 0, (void**)&data));
	//memcpy(data, ktxTextureData, ktxTextureSize);
	//vkUnmapMemory(device, stagingMemory);

	//// Create optimal tiled target image
	//VkImageCreateInfo imageCreateInfo = initializers::imageCreateInfo();
	//imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	//imageCreateInfo.format = format;
	//imageCreateInfo.mipLevels = cubeMap.mipLevels;
	//imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	//imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	//imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	//imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	//imageCreateInfo.extent = { cubeMap.width, cubeMap.height, 1 };
	//imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	//// Cube faces count as array layers in Vulkan
	//imageCreateInfo.arrayLayers = 6;
	//// This flag is required for cube map images
	//imageCreateInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

	//VK_CHECK_RESULT(vkCreateImage(device, &imageCreateInfo, nullptr, &cubeMap.image));

	//vkGetImageMemoryRequirements(device, cubeMap.image, &memReqs);

	//memAllocInfo.allocationSize = memReqs.size;
	//memAllocInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	//VK_CHECK_RESULT(vkAllocateMemory(device, &memAllocInfo, nullptr, &cubeMap.deviceMemory));
	//VK_CHECK_RESULT(vkBindImageMemory(device, cubeMap.image, cubeMap.deviceMemory, 0));

	//VkCommandBuffer copyCmd = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

	//// Setup buffer copy regions for each face including all of its miplevels
	//std::vector<VkBufferImageCopy> bufferCopyRegions;
	//uint32_t offset = 0;

	//for (uint32_t face = 0; face < 6; face++)
	//{
	//	for (uint32_t level = 0; level < cubeMap.mipLevels; level++)
	//	{
	//		// Calculate offset into staging buffer for the current mip level and face
	//		ktx_size_t offset;
	//		KTX_error_code ret = ktxTexture_GetImageOffset(ktxTexture, level, 0, face, &offset);
	//		assert(ret == KTX_SUCCESS);
	//		VkBufferImageCopy bufferCopyRegion = {};
	//		bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	//		bufferCopyRegion.imageSubresource.mipLevel = level;
	//		bufferCopyRegion.imageSubresource.baseArrayLayer = face;
	//		bufferCopyRegion.imageSubresource.layerCount = 1;
	//		bufferCopyRegion.imageExtent.width = ktxTexture->baseWidth >> level;
	//		bufferCopyRegion.imageExtent.height = ktxTexture->baseHeight >> level;
	//		bufferCopyRegion.imageExtent.depth = 1;
	//		bufferCopyRegion.bufferOffset = offset;
	//		bufferCopyRegions.push_back(bufferCopyRegion);
	//	}
	//}

	//// Image barrier for optimal image (target)
	//// Set initial layout for all array layers (faces) of the optimal (target) tiled texture
	//VkImageSubresourceRange subresourceRange = {};
	//subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	//subresourceRange.baseMipLevel = 0;
	//subresourceRange.levelCount = cubeMap.mipLevels;
	//subresourceRange.layerCount = 6;

	//vks::tools::setImageLayout(
	//	copyCmd,
	//	cubeMap.image,
	//	VK_IMAGE_LAYOUT_UNDEFINED,
	//	VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	//	subresourceRange);

	//// Copy the cube map faces from the staging buffer to the optimal tiled image
	//vkCmdCopyBufferToImage(
	//	copyCmd,
	//	stagingBuffer,
	//	cubeMap.image,
	//	VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	//	static_cast<uint32_t>(bufferCopyRegions.size()),
	//	bufferCopyRegions.data()
	//);

	//// Change texture image layout to shader read after all faces have been copied
	//cubeMap.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	//vks::tools::setImageLayout(
	//	copyCmd,
	//	cubeMap.image,
	//	VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	//	cubeMap.imageLayout,
	//	subresourceRange);

	//vulkanDevice->flushCommandBuffer(copyCmd, queue, true);

	//// Create sampler
	//VkSamplerCreateInfo sampler = vks::initializers::samplerCreateInfo();
	//sampler.magFilter = VK_FILTER_LINEAR;
	//sampler.minFilter = VK_FILTER_LINEAR;
	//sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	//sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	//sampler.addressModeV = sampler.addressModeU;
	//sampler.addressModeW = sampler.addressModeU;
	//sampler.mipLodBias = 0.0f;
	//sampler.compareOp = VK_COMPARE_OP_NEVER;
	//sampler.minLod = 0.0f;
	//sampler.maxLod = cubeMap.mipLevels;
	//sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	//sampler.maxAnisotropy = 1.0f;
	//if (vulkanDevice->features.samplerAnisotropy)
	//{
	//	sampler.maxAnisotropy = vulkanDevice->properties.limits.maxSamplerAnisotropy;
	//	sampler.anisotropyEnable = VK_TRUE;
	//}
	//VK_CHECK_RESULT(vkCreateSampler(device, &sampler, nullptr, &cubeMap.sampler));

	//// Create image view
	//VkImageViewCreateInfo view = vks::initializers::imageViewCreateInfo();
	//// Cube map view type
	//view.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
	//view.format = format;
	//view.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
	//view.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	//// 6 array layers (faces)
	//view.subresourceRange.layerCount = 6;
	//// Set number of mip levels
	//view.subresourceRange.levelCount = cubeMap.mipLevels;
	//view.image = cubeMap.image;
	//VK_CHECK_RESULT(vkCreateImageView(device, &view, nullptr, &cubeMap.view));

	//// Clean up staging resources
	//vkFreeMemory(device, stagingMemory, nullptr);
	//vkDestroyBuffer(device, stagingBuffer, nullptr);
	//ktxTexture_Destroy(ktxTexture);
}

void Demo::LoadTextures()
{
	CreateTextureImage("../textures/block.jpg", testImage.image, testImage.memory);
	testImage.imageView = CreateImageView(testImage.image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
	testImage.sampler = CreateTextureSampler();
	testImage.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
}

void Demo::CreateLight()
{
	lightsData.point_light[0] = PointLight(glm::vec3(0.1f, 0.5f, 0.1f), glm::vec3(-10.f, -10.f, -10.f));
	lightsData.point_light[1] = PointLight(glm::vec3(0.5f, 0.1f, 0.1), glm::vec3(10.f, 10.f, 10.f));
	lightsData.point_light[2] = PointLight(glm::vec3(0.1f, 0.1f, 0.5f), glm::vec3(-10.f, 10.f, 10.f));
}

void Demo::CreateCamera()
{
	camera = new Camera(glm::vec3(0, 0, 5), glm::vec3(0, 1, 0));
}

void Demo::CreateSyncObjects()
{
	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	if (vkCreateSemaphore(mVulkanDevice->logicalDevice, &semaphoreInfo, nullptr, &GBufferComplete) != VK_SUCCESS ||
		vkCreateSemaphore(mVulkanDevice->logicalDevice, &semaphoreInfo, nullptr, &renderComplete) != VK_SUCCESS ||
		vkCreateSemaphore(mVulkanDevice->logicalDevice, &semaphoreInfo, nullptr, &presentComplete) != VK_SUCCESS ||
		vkCreateSemaphore(mVulkanDevice->logicalDevice, &semaphoreInfo, nullptr, &PostComplete) != VK_SUCCESS ||
		vkCreateFence(mVulkanDevice->logicalDevice, &fenceInfo, nullptr, &inFlightFence) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create semaphores!");
	}
}


void Demo::CreateUniformBuffers()
{
	VkDeviceSize MatbufferSize = sizeof(UniformBufferMat);
	mVulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &matUBO, MatbufferSize);

	VkDeviceSize LightbufferSize = sizeof(lightsData);
	mVulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &lightUBO, LightbufferSize);
}

void Demo::CreateDescriptorPool()
{
	VkDescriptorPoolSize matPoolsize{};
	matPoolsize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	matPoolsize.descriptorCount = 2;//2 for view, project

	VkDescriptorPoolSize Lightpoolsize{};
	Lightpoolsize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	Lightpoolsize.descriptorCount = 2;//2 for point lights, look vec

	VkDescriptorPoolSize GBufferAttachmentSize{};
	GBufferAttachmentSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	GBufferAttachmentSize.descriptorCount = 3;//3 for albedo, normal, position

	VkDescriptorPoolSize ModelTexturesSize{};
	ModelTexturesSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	ModelTexturesSize.descriptorCount = 1;//1 for diffuse

	std::vector<VkDescriptorPoolSize> poolSizes = { matPoolsize, Lightpoolsize, GBufferAttachmentSize, ModelTexturesSize };

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();

	poolInfo.maxSets = poolSizes.size();

	if (vkCreateDescriptorPool(mVulkanDevice->logicalDevice, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create descriptor pool!");
	}
}

void Demo::CreateSampler()
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
	VK_CHECK_RESULT(vkCreateSampler(mVulkanDevice->logicalDevice, &sampler, nullptr, &colorSampler));
}

void Demo::CreateDescriptorSetLayout()
{
	//Binding 0: view/projection mat & view vector
	VkDescriptorSetLayoutBinding matLayoutBinding{};
	matLayoutBinding.binding = 0;
	matLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	matLayoutBinding.descriptorCount = 1;
	matLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT;
	matLayoutBinding.pImmutableSamplers = nullptr;

	//Binding 1: view/projection mat & view vector
	VkDescriptorSetLayoutBinding lightLayoutBinding{};
	lightLayoutBinding.binding = 1;
	lightLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	lightLayoutBinding.descriptorCount = 1;
	lightLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_GEOMETRY_BIT;
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

	VkDescriptorSetLayoutBinding diffuseTextureBinding{};
	diffuseTextureBinding.binding = 5;
	diffuseTextureBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	diffuseTextureBinding.descriptorCount = 1;
	diffuseTextureBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	diffuseTextureBinding.pImmutableSamplers = nullptr;

	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = { lightLayoutBinding, positionTextureBinding, normalTextureBinding, albedoTextureBinding };
	VkDescriptorSetLayoutCreateInfo lightDescriptorLayout = initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(mVulkanDevice->logicalDevice, &lightDescriptorLayout, nullptr, &LightDescriptorSetLayout))

	VkPipelineLayoutCreateInfo LightpipelineLayoutCreateInfo = initializers::pipelineLayoutCreateInfo(&LightDescriptorSetLayout, 1);
	VK_CHECK_RESULT(vkCreatePipelineLayout(mVulkanDevice->logicalDevice, &LightpipelineLayoutCreateInfo, nullptr, &LightPipelineLayout))
	//Create Lighting descriptor set layout and pipeilne layout

	std::vector<VkDescriptorSetLayoutBinding> GLayoutBinding = { matLayoutBinding, diffuseTextureBinding };
	VkDescriptorSetLayoutCreateInfo GDescriptorLayout = initializers::descriptorSetLayoutCreateInfo(GLayoutBinding);

	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(mVulkanDevice->logicalDevice, &GDescriptorLayout, nullptr, &GDescriptorSetLayout))
	VkPushConstantRange pushConstant;
	pushConstant.offset = 0;
	pushConstant.size = sizeof(glm::mat4);
	pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT;

	VkPipelineLayoutCreateInfo GpipelineLayoutCreateInfo = initializers::pipelineLayoutCreateInfo(&GDescriptorSetLayout, 1);
	GpipelineLayoutCreateInfo.pushConstantRangeCount = 1;
	GpipelineLayoutCreateInfo.pPushConstantRanges = &pushConstant;

	VK_CHECK_RESULT(vkCreatePipelineLayout(mVulkanDevice->logicalDevice, &GpipelineLayoutCreateInfo, nullptr, &GPipelineLayout))
	//Create G path descriptor set layout and pipeline layout

	std::vector<VkDescriptorSetLayoutBinding> PLayoutbindings = { matLayoutBinding };
	VkDescriptorSetLayoutCreateInfo PDescriptorSetLayoutCI = initializers::descriptorSetLayoutCreateInfo(PLayoutbindings);
	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(mVulkanDevice->logicalDevice, &PDescriptorSetLayoutCI, nullptr, &PostDescriptorSetLayout))

	VkPipelineLayoutCreateInfo PostPipelineLayoutCreateInfo = initializers::pipelineLayoutCreateInfo(&PostDescriptorSetLayout, 1);
	PostPipelineLayoutCreateInfo.pushConstantRangeCount = 1;
	PostPipelineLayoutCreateInfo.pPushConstantRanges = &pushConstant;
	VK_CHECK_RESULT(vkCreatePipelineLayout(mVulkanDevice->logicalDevice, &PostPipelineLayoutCreateInfo, nullptr, &PostPipelineLayaout))
	//Create P path descriptor set layout and pipeline layout.
}

void Demo::CreateDescriptorSet()
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
	if (vkAllocateDescriptorSets(mVulkanDevice->logicalDevice, &lightingAllocInfo, &LightingDescriptorSet) != VK_SUCCESS)
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

	vkUpdateDescriptorSets(mVulkanDevice->logicalDevice, static_cast<uint32_t>(lightWriteDescriptorSets.size()), lightWriteDescriptorSets.data(), 0, nullptr);
	// L-Buffer description set

	VkDescriptorSetAllocateInfo GAllocInfo{};
	GAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	GAllocInfo.descriptorPool = descriptorPool;
	GAllocInfo.descriptorSetCount = 1;
	GAllocInfo.pSetLayouts = &GDescriptorSetLayout;

	VkDescriptorImageInfo modelDiffuseDisc;
	modelDiffuseDisc.sampler = testImage.sampler;
	modelDiffuseDisc.imageView = testImage.imageView;
	modelDiffuseDisc.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	if (vkAllocateDescriptorSets(mVulkanDevice->logicalDevice, &GAllocInfo, &GBufferDescriptorSet) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate descriptor sets");
	}

	VkDescriptorBufferInfo GBufferInfo{};
	GBufferInfo.buffer = matUBO.buffer;
	GBufferInfo.offset = 0;
	GBufferInfo.range = sizeof(UniformBufferMat);

	std::vector<VkWriteDescriptorSet> GBufWriteDescriptorSets;
	GBufWriteDescriptorSets = {
		initializers::writeDescriptorSet(GBufferDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &GBufferInfo),
		initializers::writeDescriptorSet(GBufferDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 5, &modelDiffuseDisc)
	};

	vkUpdateDescriptorSets(mVulkanDevice->logicalDevice, static_cast<uint32_t>(GBufWriteDescriptorSets.size()), GBufWriteDescriptorSets.data(), 0, nullptr);
	// G-Buffer description set

	VkDescriptorSetAllocateInfo PAllocInfo = {};
	PAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	PAllocInfo.descriptorPool = descriptorPool;
	PAllocInfo.descriptorSetCount = 1;
	PAllocInfo.pSetLayouts = &PostDescriptorSetLayout;

	if (vkAllocateDescriptorSets(mVulkanDevice->logicalDevice, &PAllocInfo, &PostDescriptorSet) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate descriptor sets");
	}

	VkDescriptorBufferInfo PBufferInfo{};
	PBufferInfo.buffer = matUBO.buffer;
	PBufferInfo.offset = 0;
	PBufferInfo.range = sizeof(UniformBufferMat);

	std::vector<VkWriteDescriptorSet> PBufWriteDescriptorSets;
	PBufWriteDescriptorSets = {
		initializers::writeDescriptorSet(PostDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &PBufferInfo)
	};

	vkUpdateDescriptorSets(mVulkanDevice->logicalDevice, static_cast<uint32_t>(PBufWriteDescriptorSets.size()), PBufWriteDescriptorSets.data(), 0, nullptr);
}

void Demo::CreateGraphicsPipelines()
{
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
		initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
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

	VkGraphicsPipelineCreateInfo pipelineCI = initializers::pipelineCreateInfo(LightPipelineLayout, mLFrameBuffer.renderPass);
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
	shaderStages[0] = createShaderStageCreateInfo("../shaders/LightingVert.spv", VK_SHADER_STAGE_VERTEX_BIT, mVulkanDevice->logicalDevice);
	shaderStages[1] = createShaderStageCreateInfo("../shaders/LightingFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT, mVulkanDevice->logicalDevice);

	VkPipelineVertexInputStateCreateInfo emptyInput = initializers::pipelineVertexInputStateCreateInfo();
	pipelineCI.pVertexInputState = &emptyInput;
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(mVulkanDevice->logicalDevice, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &LightingPipeline))
		//Finish create lighting graphics pipeline.
		//Sequently create G buffer graphics pipeline for utilize created pipeline parameters.

	VkPipelineVertexInputStateCreateInfo vertexInputInfo {};

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

	shaderStages[0] = createShaderStageCreateInfo("../shaders/GBufferVert.spv", VK_SHADER_STAGE_VERTEX_BIT, mVulkanDevice->logicalDevice);
	shaderStages[1] = createShaderStageCreateInfo("../shaders/GBufferFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT, mVulkanDevice->logicalDevice);//TODO: GBuffer GLSL 새로 써야함.

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

	VK_CHECK_RESULT(vkCreateGraphicsPipelines(mVulkanDevice->logicalDevice, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &GBufferPipeline))
	// Finish create G-graphics pipeline

	std::array<VkPipelineShaderStageCreateInfo, 3> geometryShaderStages;
	geometryShaderStages[0] = createShaderStageCreateInfo("../shaders/BaseVert.spv", VK_SHADER_STAGE_VERTEX_BIT, mVulkanDevice->logicalDevice);
	geometryShaderStages[1] = createShaderStageCreateInfo("../shaders/NormalDebug.spv", VK_SHADER_STAGE_GEOMETRY_BIT, mVulkanDevice->logicalDevice);
	geometryShaderStages[2] = createShaderStageCreateInfo("../shaders/BaseFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT, mVulkanDevice->logicalDevice);

	pipelineCI.stageCount = static_cast<uint32_t>(geometryShaderStages.size());
	pipelineCI.pStages = geometryShaderStages.data();

	pipelineCI.layout = PostPipelineLayaout;
	pipelineCI.pVertexInputState = &vertexInputInfo;
	rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;//TODO: Maybe problem.

	pipelineCI.renderPass = mPFrameBuffer.renderPass;

	//TODO: Maybe blend state is problem.
	colorBlendState = initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(mVulkanDevice->logicalDevice, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &PostPipeline))
}

void Demo::CreateCommandBuffers()
{
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = mVulkanDevice->mCommandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;

	if (vkAllocateCommandBuffers(mVulkanDevice->logicalDevice, &allocInfo, &GCommandBuffer) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate command buffers!");
	}

	if (vkAllocateCommandBuffers(mVulkanDevice->logicalDevice, &allocInfo, &LightingCommandBuffer) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate command buffers!");
	}

	if (vkAllocateCommandBuffers(mVulkanDevice->logicalDevice, &allocInfo, &PostCommandBuffer) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate command buffers!");
	}
}

void Demo::BuildLightCommandBuffer()
{
	VkCommandBufferBeginInfo cmdBufInfo = initializers::commandBufferBeginInfo();

	VkClearValue clearValues[2];
	clearValues[0].color = { {0.f, 0.f, 0.2f, 0.f} };
	clearValues[1].depthStencil = { 1.f, 0 };

	VkRenderPassBeginInfo renderPassBeginInfo = initializers::renderPassBeginInfo();
	renderPassBeginInfo.renderPass = mLFrameBuffer.renderPass;
	renderPassBeginInfo.renderArea.offset.x = 0;
	renderPassBeginInfo.renderArea.offset.y = 0;
	renderPassBeginInfo.renderArea.extent.width = mLFrameBuffer.width;
	renderPassBeginInfo.renderArea.extent.height = mLFrameBuffer.height;
	renderPassBeginInfo.clearValueCount = 2;
	renderPassBeginInfo.pClearValues = clearValues;

	renderPassBeginInfo.framebuffer = mLFrameBuffer.framebuffer;

	VK_CHECK_RESULT(vkBeginCommandBuffer(LightingCommandBuffer, &cmdBufInfo))
		vkCmdBeginRenderPass(LightingCommandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
	VkViewport viewport = initializers::viewport((float)mLFrameBuffer.width, (float)mLFrameBuffer.height, 0.f, 1.f);
	vkCmdSetViewport(LightingCommandBuffer, 0, 1, &viewport);
	VkRect2D scissor = initializers::rect2D(mLFrameBuffer.width, mLFrameBuffer.height, 0, 0);
	vkCmdSetScissor(LightingCommandBuffer, 0, 1, &scissor);

	vkCmdBindDescriptorSets(LightingCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, LightPipelineLayout, 0, 1, &LightingDescriptorSet, 0, nullptr);

	vkCmdBindPipeline(LightingCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, LightingPipeline);
	vkCmdDraw(LightingCommandBuffer, 3, 1, 0, 0);

	ImGui::Render();
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), LightingCommandBuffer);

	vkCmdEndRenderPass(LightingCommandBuffer);

	VK_CHECK_RESULT(vkEndCommandBuffer(LightingCommandBuffer));
}

void Demo::BuildGCommandBuffer()
{
	GCommandBuffer = mVulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);

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

	int accumulatingVertices = 0;
	int accumulatingFaces = 0;
	for (Object* object : objects)
	{
		accumulatingVertices += object->mMesh->vertexNum;
		accumulatingFaces += object->mMesh->faceNum;

		VkBuffer vertexBuffers[] = { object->mMesh->vertexBuffer };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(GCommandBuffer, 0, 1, vertexBuffers, offsets);
		glm::mat4 modelMat = object->BuildModelMat();
		vkCmdPushConstants(GCommandBuffer, GPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT, 0, sizeof(glm::mat4), &modelMat);
		vkCmdDraw(GCommandBuffer, static_cast<uint32_t>(object->mMesh->vertices.size()), 1, 0, 0);
	}
	totalVertices = accumulatingVertices;
	totalFaces = accumulatingFaces; 

	vkCmdEndRenderPass(GCommandBuffer);

	VK_CHECK_RESULT(vkEndCommandBuffer(GCommandBuffer));
}

void Demo::BuildPostCommandBuffer(int swapChianIndex)
{
	VkCommandBufferBeginInfo cmdBufInfo = initializers::commandBufferBeginInfo();

	// Clear values for all attachments written in the fragment shader
	std::array<VkClearValue, 2> clearValues;
	clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
	clearValues[1].depthStencil = { 1.0f, 0 };

	VkRenderPassBeginInfo renderPassBeginInfo = initializers::renderPassBeginInfo();
	renderPassBeginInfo.renderPass = mPFrameBuffer.renderPass;
	renderPassBeginInfo.framebuffer = mPFrameBuffer.framebuffer;
	renderPassBeginInfo.renderArea.extent.width = mPFrameBuffer.width;
	renderPassBeginInfo.renderArea.extent.height = mPFrameBuffer.height;
	renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassBeginInfo.pClearValues = clearValues.data();

	VK_CHECK_RESULT(vkBeginCommandBuffer(PostCommandBuffer, &cmdBufInfo))

	vkCmdBeginRenderPass(PostCommandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	VkViewport viewport = initializers::viewport((float)mPFrameBuffer.width, (float)mPFrameBuffer.height, 0.0f, 1.0f);
	vkCmdSetViewport(PostCommandBuffer, 0, 1, &viewport);

	VkRect2D scissor = initializers::rect2D(mPFrameBuffer.width, mPFrameBuffer.height, 0, 0);
	vkCmdSetScissor(PostCommandBuffer, 0, 1, &scissor);

	vkCmdBindPipeline(PostCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, PostPipeline);

	vkCmdBindDescriptorSets(PostCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, PostPipelineLayaout, 0, 1, &PostDescriptorSet, 0, nullptr);

	if (DrawNormal == true)
	{
		for (Object* object : objects)
		{
			VkBuffer vertexBuffers[] = { object->mMesh->vertexBuffer };
			VkDeviceSize offsets[] = { 0 };
			vkCmdBindVertexBuffers(PostCommandBuffer, 0, 1, vertexBuffers, offsets);
			glm::mat4 modelMat = object->BuildModelMat();
			vkCmdPushConstants(PostCommandBuffer, PostPipelineLayaout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT, 0, sizeof(glm::mat4), &modelMat);
			vkCmdDraw(PostCommandBuffer, static_cast<uint32_t>(object->mMesh->vertices.size()), 1, 0, 0);
		}
	}

	ImGui::Render();
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), PostCommandBuffer);

	vkCmdEndRenderPass(PostCommandBuffer);

	VK_CHECK_RESULT(vkEndCommandBuffer(PostCommandBuffer));
}

void Demo::UpdateUniformBuffer(uint32_t currentImage)
{
	static auto startTime = std::chrono::high_resolution_clock::now();

	auto currentTime = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

	UniformBufferMat ubo{};
	ubo.view = camera->getViewMatrix();
	ubo.proj = glm::perspective(glm::radians(45.f), mSwapChain->mSwapChainExtent.width / (float)mSwapChain->mSwapChainExtent.height, 0.1f, 100.f);
	ubo.proj[1][1] *= -1;

	float radius = 5.f;
	float rotateAmount = 0.f;
	if (RotatingLight == true)
	{
		rotateAmount = accumulatingDT;
	}
	for (int i = 0; i < 3; ++i)
	{
		lightsData.point_light[i].mPos = glm::vec3(radius * cos(rotateAmount + glm::radians(120.f * i)), 0.f, radius * sin(rotateAmount + glm::radians(120.f * i)));
	}

	void* Matdata;
	vkMapMemory(mVulkanDevice->logicalDevice, matUBO.memory, 0, sizeof(ubo), 0, &Matdata);
	memcpy(Matdata, &ubo, sizeof(ubo));
	vkUnmapMemory(mVulkanDevice->logicalDevice, matUBO.memory);

	lightsData.lookVec = (camera->front - camera->position);
	void* Lightdata;
	vkMapMemory(mVulkanDevice->logicalDevice, lightUBO.memory, 0, sizeof(UniformBufferLights), 0, &Lightdata);
	memcpy(Lightdata, &lightsData, sizeof(lightsData));
	vkUnmapMemory(mVulkanDevice->logicalDevice, lightUBO.memory);
}

void Demo::ProcessInput()
{
	if (glfwGetKey(mWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(mWindow, true);

	if (glfwGetKey(mWindow, GLFW_KEY_W) == GLFW_PRESS)
		camera->ProcessKeyboard(FORWARD, deltaTime);
	if (glfwGetKey(mWindow, GLFW_KEY_S) == GLFW_PRESS)
		camera->ProcessKeyboard(BACKWARD, deltaTime);
	if (glfwGetKey(mWindow, GLFW_KEY_A) == GLFW_PRESS)
		camera->ProcessKeyboard(LEFT, deltaTime);
	if (glfwGetKey(mWindow, GLFW_KEY_D) == GLFW_PRESS)
		camera->ProcessKeyboard(RIGHT, deltaTime);
	if (glfwGetKey(mWindow, GLFW_KEY_Q) == GLFW_PRESS)
		camera->ProcessKeyboard(UP, deltaTime);
	if (glfwGetKey(mWindow, GLFW_KEY_E) == GLFW_PRESS)
		camera->ProcessKeyboard(DOWN, deltaTime);
}

void Demo::InitGUI()
{
	uint32_t subpassID = 0;

	// UI
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.LogFilename = nullptr;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls

	std::vector<VkDescriptorPoolSize> poolSize{ {VK_DESCRIPTOR_TYPE_SAMPLER, 1}, {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1} };
	VkDescriptorPoolCreateInfo        poolInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	poolInfo.maxSets = 2;
	poolInfo.poolSizeCount = 2;
	poolInfo.pPoolSizes = poolSize.data();
	vkCreateDescriptorPool(mVulkanDevice->logicalDevice, &poolInfo, nullptr, &mImguiDescPool);

	// Setup Platform/Renderer back ends
	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = mInstance;
	init_info.PhysicalDevice = mVulkanDevice->physicalDevice;
	init_info.Device = mVulkanDevice->logicalDevice;
	init_info.QueueFamily = mVulkanDevice->getQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT);
	init_info.Queue = mGraphicsQueue;
	init_info.PipelineCache = VK_NULL_HANDLE;
	init_info.DescriptorPool = mImguiDescPool;
	init_info.Subpass = subpassID;
	init_info.MinImageCount = 2;
	init_info.ImageCount = mSwapChain->mImageCount;
	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	init_info.CheckVkResultFn = nullptr;
	init_info.Allocator = nullptr;

	ImGui_ImplVulkan_Init(&init_info, mSwapChain->mSwapChainRenderPass);

	// Upload Fonts
	VkCommandBuffer cmdbuf = CreateTempCmdBuf();
	ImGui_ImplVulkan_CreateFontsTexture(cmdbuf);
	SubmitTempCmdBufToGraphicsQueue(cmdbuf);

	ImGui_ImplGlfw_InitForVulkan(mWindow, true);
}

void Demo::DrawGUI()
{
	ImGui::Text("Rate %.3f ms/frame (%.1f FPS)",
		1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

	ImGui::Text("Total Vertices: %d", totalVertices);
	ImGui::Text("Total Faces: %d", totalFaces);

	ImGui::Checkbox("Debug Normals", &DrawNormal);
	ImGui::Checkbox("Rotate Lights", &RotatingLight);

	ImGui::ColorPicker3("Light1", &lightsData.point_light[0].mColor[0]);
	ImGui::ColorPicker3("Light2", &lightsData.point_light[1].mColor[0]);
	ImGui::ColorPicker3("Light3", &lightsData.point_light[2].mColor[0]);
}