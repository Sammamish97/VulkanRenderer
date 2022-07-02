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
	LoadCubemap("../textures/cubemap_yokohama_rgba.ktx", VK_FORMAT_R8G8B8A8_UNORM, false);
	CreateCamera();
	CreateSyncObjects();

	shadow_pass.Init(this, WIDTH, HEIGHT);
	geometry_pass.Init(this, WIDTH, HEIGHT);
	occlusion_pass.Init(this, WIDTH, HEIGHT);
	lighting_pass.Init(this, WIDTH, HEIGHT);
	post_pass.Init(this, WIDTH, HEIGHT, &lighting_pass.mComposition, &geometry_pass.mDepth);

	InitDescriptorPool();
	InitDescriptorLayout();
	InitDescriptorSet();

	shadow_pass.CreateFrameData();
	shadow_pass.CreatePipelineData();

	geometry_pass.CreateFrameData();
	geometry_pass.CreatePipelineData();

	occlusion_pass.CreateFrameData();
	occlusion_pass.CreatePipelineData();

	lighting_pass.CreateFrameData();
	lighting_pass.CreatePipelineData();

	post_pass.CreateFrameData();
	post_pass.CreatePipelineData();

	CreateUniformBuffers();
	CreateSampler();
	CreateShadowDepthSampler();
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

	UpdateUniformBuffer();
	UpdateDescriptorSet();
	BuildShadowCommandBuffer();
	BuildGCommandBuffer();
	BuildOCommandBuffer();
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

	vkResetFences(mVulkanDevice->logicalDevice, 1, &inFlightFence);

	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

	VkSubmitInfo SSubmitInfo = {};
	SSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	SSubmitInfo.pWaitSemaphores = &presentComplete;
	SSubmitInfo.waitSemaphoreCount = 1;
	SSubmitInfo.pSignalSemaphores = &ShadowComplete;
	SSubmitInfo.signalSemaphoreCount = 1;
	SSubmitInfo.commandBufferCount = 1;
	SSubmitInfo.pCommandBuffers = &ShadowCommandBuffer;
	SSubmitInfo.pWaitDstStageMask = waitStages;
	VK_CHECK_RESULT(vkQueueSubmit(mGraphicsQueue, 1, &SSubmitInfo, nullptr))


	VkSubmitInfo GSubmitInfo = {};
	GSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	GSubmitInfo.pWaitSemaphores = &ShadowComplete;
	GSubmitInfo.waitSemaphoreCount = 1;
	GSubmitInfo.pSignalSemaphores = &GBufferComplete;
	GSubmitInfo.signalSemaphoreCount = 1;
	GSubmitInfo.commandBufferCount = 1;
	GSubmitInfo.pCommandBuffers = &GCommandBuffer;
	GSubmitInfo.pWaitDstStageMask = waitStages;
	VK_CHECK_RESULT(vkQueueSubmit(mGraphicsQueue, 1, &GSubmitInfo, nullptr))

	VkSubmitInfo occulusionSubmitInfo = {};
	occulusionSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	occulusionSubmitInfo.pWaitSemaphores = &GBufferComplete;
	occulusionSubmitInfo.waitSemaphoreCount = 1;
	occulusionSubmitInfo.pSignalSemaphores = &OcculusionComplete;
	occulusionSubmitInfo.signalSemaphoreCount = 1;
	occulusionSubmitInfo.commandBufferCount = 1;
	occulusionSubmitInfo.pCommandBuffers = &OCommandBuffer;
	occulusionSubmitInfo.pWaitDstStageMask = waitStages;
	VK_CHECK_RESULT(vkQueueSubmit(mGraphicsQueue, 1, &occulusionSubmitInfo, nullptr))

	VkSubmitInfo lightSubmitInfo = {};
	lightSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	lightSubmitInfo.pWaitSemaphores = &OcculusionComplete;
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

	CopyImage(post_pass.mLColorResult->image, VK_ACCESS_MEMORY_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
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
	greenMesh->loadAndCreateMesh("../models/Monkey.obj", mVulkanDevice, glm::vec3(0.5, 0.5, 0.5));

	BlueMesh = new Mesh;
	BlueMesh->loadAndCreateMesh("../models/Torus.obj", mVulkanDevice, glm::vec3(0.8, 0.8, 0.8));

	floor = new Mesh;
	floor->loadAndCreateMesh("../models/Plane.obj", mVulkanDevice, glm::vec3(0.8, 0.8, 0.8));

	Skybox = new Mesh;
	Skybox->loadAndCreateMesh("../models/Skybox.obj", mVulkanDevice, glm::vec3(0.8, 0.8, 0.8));

	objects.push_back(new Object(redMesh, glm::vec3(0.f, 3.f, 3.f)));
	objects.push_back(new Object(greenMesh, glm::vec3(3.f, 3.f, 0.f)));
	objects.push_back(new Object(BlueMesh, glm::vec3(-3.f, 3.f, 0.f)));
	objects.push_back(new Object(floor, glm::vec3(0, 0.0, 0)));
}

void Demo::LoadCubemap(std::string filename, VkFormat format, bool forceLinearTiling)
{
	ktxResult result;
	ktxTexture* ktxTexture;

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
	ktx_size_t ktxTextureSize = ktxTexture_GetSize(ktxTexture);

	VkMemoryAllocateInfo memAllocInfo = initializers::memoryAllocateInfo();
	VkMemoryRequirements memReqs;

	// Create a host-visible staging buffer that contains the raw image data
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingMemory;

	VkBufferCreateInfo bufferCreateInfo = initializers::bufferCreateInfo();
	bufferCreateInfo.size = ktxTextureSize;
	// This buffer is used as a transfer source for the buffer copy
	bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VK_CHECK_RESULT(vkCreateBuffer(mVulkanDevice->logicalDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

	// Get memory requirements for the staging buffer (alignment, memory type bits)
	vkGetBufferMemoryRequirements(mVulkanDevice->logicalDevice, stagingBuffer, &memReqs);
	memAllocInfo.allocationSize = memReqs.size;
	// Get memory type index for a host visible buffer
	memAllocInfo.memoryTypeIndex = mVulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(mVulkanDevice->logicalDevice, &memAllocInfo, nullptr, &stagingMemory));
	VK_CHECK_RESULT(vkBindBufferMemory(mVulkanDevice->logicalDevice, stagingBuffer, stagingMemory, 0));

	// Copy texture data into staging buffer
	uint8_t* data;
	VK_CHECK_RESULT(vkMapMemory(mVulkanDevice->logicalDevice, stagingMemory, 0, memReqs.size, 0, (void**)&data));
	memcpy(data, ktxTextureData, ktxTextureSize);
	vkUnmapMemory(mVulkanDevice->logicalDevice, stagingMemory);

	// Create optimal tiled target image
	VkImageCreateInfo imageCreateInfo = initializers::imageCreateInfo();
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.format = format;
	imageCreateInfo.mipLevels = testCubemap.mipLevels;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageCreateInfo.extent = { testCubemap.width, testCubemap.height, 1 };
	imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	// Cube faces count as array layers in Vulkan
	imageCreateInfo.arrayLayers = 6;
	// This flag is required for cube map images
	imageCreateInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

	VK_CHECK_RESULT(vkCreateImage(mVulkanDevice->logicalDevice, &imageCreateInfo, nullptr, &testCubemap.image));

	vkGetImageMemoryRequirements(mVulkanDevice->logicalDevice, testCubemap.image, &memReqs);

	memAllocInfo.allocationSize = memReqs.size;
	memAllocInfo.memoryTypeIndex = mVulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	VK_CHECK_RESULT(vkAllocateMemory(mVulkanDevice->logicalDevice, &memAllocInfo, nullptr, &testCubemap.memory));
	VK_CHECK_RESULT(vkBindImageMemory(mVulkanDevice->logicalDevice, testCubemap.image, testCubemap.memory, 0));

	VkCommandBuffer copyCmd = mVulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

	// Setup buffer copy regions for each face including all of its miplevels
	std::vector<VkBufferImageCopy> bufferCopyRegions;
	uint32_t offset = 0;

	for (uint32_t face = 0; face < 6; face++)
	{
		for (uint32_t level = 0; level < testCubemap.mipLevels; level++)
		{
			// Calculate offset into staging buffer for the current mip level and face
			ktx_size_t offset;
			KTX_error_code ret = ktxTexture_GetImageOffset(ktxTexture, level, 0, face, &offset);
			assert(ret == KTX_SUCCESS);
			VkBufferImageCopy bufferCopyRegion = {};
			bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			bufferCopyRegion.imageSubresource.mipLevel = level;
			bufferCopyRegion.imageSubresource.baseArrayLayer = face;
			bufferCopyRegion.imageSubresource.layerCount = 1;
			bufferCopyRegion.imageExtent.width = ktxTexture->baseWidth >> level;
			bufferCopyRegion.imageExtent.height = ktxTexture->baseHeight >> level;
			bufferCopyRegion.imageExtent.depth = 1;
			bufferCopyRegion.bufferOffset = offset;
			bufferCopyRegions.push_back(bufferCopyRegion);
		}
	}

	// Image barrier for optimal image (target)
	// Set initial layout for all array layers (faces) of the optimal (target) tiled texture
	VkImageSubresourceRange subresourceRange = {};
	subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = testCubemap.mipLevels;
	subresourceRange.layerCount = 6;

	vks::tools::setImageLayout(
		copyCmd,
		testCubemap.image,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		subresourceRange);

	// Copy the cube map faces from the staging buffer to the optimal tiled image
	vkCmdCopyBufferToImage(
		copyCmd,
		stagingBuffer,
		testCubemap.image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		static_cast<uint32_t>(bufferCopyRegions.size()),
		bufferCopyRegions.data()
	);

	// Change texture image layout to shader read after all faces have been copied
	testCubemap.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	vks::tools::setImageLayout(
		copyCmd,
		testCubemap.image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		testCubemap.imageLayout,
		subresourceRange);

	mVulkanDevice->flushCommandBuffer(copyCmd, mGraphicsQueue, true);

	// Create sampler
	VkSamplerCreateInfo sampler = initializers::samplerCreateInfo();
	sampler.magFilter = VK_FILTER_LINEAR;
	sampler.minFilter = VK_FILTER_LINEAR;
	sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler.addressModeV = sampler.addressModeU;
	sampler.addressModeW = sampler.addressModeU;
	sampler.mipLodBias = 0.0f;
	sampler.compareOp = VK_COMPARE_OP_NEVER;
	sampler.minLod = 0.0f;
	sampler.maxLod = testCubemap.mipLevels;
	sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	sampler.maxAnisotropy = 1.0f;
	if (mVulkanDevice->features.samplerAnisotropy)
	{
		sampler.maxAnisotropy = mVulkanDevice->properties.limits.maxSamplerAnisotropy;
		sampler.anisotropyEnable = VK_TRUE;
	}
	VK_CHECK_RESULT(vkCreateSampler(mVulkanDevice->logicalDevice, &sampler, nullptr, &testCubemap.sampler));

	// Create image view
	VkImageViewCreateInfo view = initializers::imageViewCreateInfo();
	// Cube map view type
	view.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
	view.format = format;
	view.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
	view.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	// 6 array layers (faces)
	view.subresourceRange.layerCount = 6;
	// Set number of mip levels
	view.subresourceRange.levelCount = testCubemap.mipLevels;
	view.image = testCubemap.image;
	VK_CHECK_RESULT(vkCreateImageView(mVulkanDevice->logicalDevice, &view, nullptr, &testCubemap.imageView));

	// Clean up staging resources
	vkFreeMemory(mVulkanDevice->logicalDevice, stagingMemory, nullptr);
	vkDestroyBuffer(mVulkanDevice->logicalDevice, stagingBuffer, nullptr);
	ktxTexture_Destroy(ktxTexture);
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
	camera = new Camera(glm::vec3(10, 10, 0), glm::vec3(0, 1, 0));
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
		vkCreateSemaphore(mVulkanDevice->logicalDevice, &semaphoreInfo, nullptr, &ShadowComplete) != VK_SUCCESS ||
		vkCreateSemaphore(mVulkanDevice->logicalDevice, &semaphoreInfo, nullptr, &OcculusionComplete) != VK_SUCCESS ||
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

	VkDeviceSize LightMatSize = sizeof(LightMatUBO);
	mVulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &lightMatUBO, LightMatSize);
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

void Demo::CreateShadowDepthSampler()
{
	VkSamplerCreateInfo sampler = initializers::samplerCreateInfo();
	sampler.magFilter = VK_FILTER_NEAREST;
	sampler.minFilter = VK_FILTER_NEAREST;
	sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	sampler.mipLodBias = 0.0f;
	sampler.maxAnisotropy = 1.0f;
	sampler.minLod = 0.0f;
	sampler.maxLod = 1.0f;
	VK_CHECK_RESULT(vkCreateSampler(mVulkanDevice->logicalDevice, &sampler, nullptr, &shadowDepthSampler));
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

	if (vkAllocateCommandBuffers(mVulkanDevice->logicalDevice, &allocInfo, &OCommandBuffer) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate command buffers!");
	}
}

void Demo::InitDescriptorPool()
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

	VkDescriptorPoolSize cubemapSize{};
	cubemapSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	cubemapSize.descriptorCount = 1;//1 for cubemap

	VkDescriptorPoolSize shadowMatSize{};
	shadowMatSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	shadowMatSize.descriptorCount = 1;//1 for MVP matrix which already multiplied.

	VkDescriptorPoolSize ShadowDepthTextureSize{};
	ShadowDepthTextureSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	ShadowDepthTextureSize.descriptorCount = 1;//1 for depth

	std::vector<VkDescriptorPoolSize> sPoolSizes = { shadowMatSize };
	std::vector<VkDescriptorPoolSize> gPoolSizes = { matPoolsize, ModelTexturesSize };
	std::vector<VkDescriptorPoolSize> oPoolSizes = { GBufferAttachmentSize };
	std::vector<VkDescriptorPoolSize> lPoolSizes = { Lightpoolsize, GBufferAttachmentSize, shadowMatSize, ShadowDepthTextureSize };
	std::vector<VkDescriptorPoolSize> pPoolSizes = { matPoolsize, cubemapSize };
	
	shadow_pass.CreateDescriptorPool(sPoolSizes);
	geometry_pass.CreateDescriptorPool(gPoolSizes);
	occlusion_pass.CreateDescriptorPool(oPoolSizes);
	lighting_pass.CreateDescriptorPool(lPoolSizes);
	post_pass.CreateDescriptorPool(pPoolSizes);
}

void Demo::InitDescriptorLayout()
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

	VkDescriptorSetLayoutBinding cubemapBinding{};
	cubemapBinding.binding = 6;
	cubemapBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	cubemapBinding.descriptorCount = 1;
	cubemapBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	cubemapBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding lightMVPBinding{};
	lightMVPBinding.binding = 7;
	lightMVPBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	lightMVPBinding.descriptorCount = 1;
	lightMVPBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	lightMVPBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding shadowDepthbinding{};
	shadowDepthbinding.binding = 8;
	shadowDepthbinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	shadowDepthbinding.descriptorCount = 1;
	shadowDepthbinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	shadowDepthbinding.pImmutableSamplers = nullptr;

	std::vector<VkDescriptorSetLayoutBinding> SLayoutBinding = { lightMVPBinding };
	shadow_pass.CreateDescriptorLayout(SLayoutBinding);

	std::vector<VkDescriptorSetLayoutBinding> GLayoutBinding = { matLayoutBinding, diffuseTextureBinding };
	geometry_pass.CreateDescriptorLayout(GLayoutBinding);

	std::vector<VkDescriptorSetLayoutBinding> OLayoutBindings = { positionTextureBinding, normalTextureBinding, albedoTextureBinding };
	occlusion_pass.CreateDescriptorLayout(OLayoutBindings);

	std::vector<VkDescriptorSetLayoutBinding> LLayoutBindings = { lightLayoutBinding, positionTextureBinding, normalTextureBinding, albedoTextureBinding, lightMVPBinding, shadowDepthbinding };
	lighting_pass.CreateDescriptorLayout(LLayoutBindings);

	std::vector<VkDescriptorSetLayoutBinding> PLayoutBindings = { matLayoutBinding };
	post_pass.CreateDescriptorLayout(PLayoutBindings);

	std::vector<VkDescriptorSetLayoutBinding> PSkyLayoutBindings = { matLayoutBinding, cubemapBinding };
	post_pass.CreateSkyDescriptorLayout(PSkyLayoutBindings);
}

void Demo::InitDescriptorSet()
{
	geometry_pass.CreateDescriptorSet();

	occlusion_pass.CreateDescriptorSet();

	lighting_pass.CreateDescriptorSet();

	post_pass.CreateDescriptorSet();
	post_pass.CreateSkyDescriptorSet();

	shadow_pass.CreateDescriptorSet();
}

void Demo::BuildShadowCommandBuffer()
{
	ShadowCommandBuffer = mVulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);

	VkCommandBufferBeginInfo cmdBufInfo = initializers::commandBufferBeginInfo();

	// Clear values for all attachments written in the fragment shader
	std::array<VkClearValue, 1> clearValues;
	clearValues[0].depthStencil = { 1.0f, 0 };


	VkRenderPassBeginInfo renderPassBeginInfo = initializers::renderPassBeginInfo();
	renderPassBeginInfo.renderPass = shadow_pass.mRenderPass;
	renderPassBeginInfo.framebuffer = shadow_pass.mFrameBuffer;
	renderPassBeginInfo.renderArea.extent.width = shadow_pass.mWidth;
	renderPassBeginInfo.renderArea.extent.height = shadow_pass.mHeight;
	renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassBeginInfo.pClearValues = clearValues.data();

	VK_CHECK_RESULT(vkBeginCommandBuffer(ShadowCommandBuffer, &cmdBufInfo))

	vkCmdBeginRenderPass(ShadowCommandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	VkViewport viewport = initializers::viewport((float)shadow_pass.mWidth, (float)shadow_pass.mHeight, 0.0f, 1.0f);
	vkCmdSetViewport(ShadowCommandBuffer, 0, 1, &viewport);

	VkRect2D scissor = initializers::rect2D(shadow_pass.mWidth, shadow_pass.mHeight, 0, 0);
	vkCmdSetScissor(ShadowCommandBuffer, 0, 1, &scissor);

	vkCmdBindPipeline(ShadowCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadow_pass.mPipeline);

	vkCmdBindDescriptorSets(ShadowCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadow_pass.mPipelineLayout, 0, 1, &shadow_pass.mDescriptorSet, 0, nullptr);

	for (Object* object : objects)
	{
		VkBuffer vertexBuffers[] = { object->mMesh->vertexBuffer };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(ShadowCommandBuffer, 0, 1, vertexBuffers, offsets);
		glm::mat4 modelMat = object->BuildModelMat();
		vkCmdPushConstants(ShadowCommandBuffer, shadow_pass.mPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &modelMat);
		vkCmdDraw(ShadowCommandBuffer, static_cast<uint32_t>(object->mMesh->vertices.size()), 1, 0, 0);
	}
	vkCmdEndRenderPass(ShadowCommandBuffer);

	VK_CHECK_RESULT(vkEndCommandBuffer(ShadowCommandBuffer));
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
	renderPassBeginInfo.renderPass = geometry_pass.mRenderPass;
	renderPassBeginInfo.framebuffer = geometry_pass.mFrameBuffer;
	renderPassBeginInfo.renderArea.extent.width = geometry_pass.mWidth;
	renderPassBeginInfo.renderArea.extent.height = geometry_pass.mHeight;
	renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassBeginInfo.pClearValues = clearValues.data();

	VK_CHECK_RESULT(vkBeginCommandBuffer(GCommandBuffer, &cmdBufInfo))

		vkCmdBeginRenderPass(GCommandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	VkViewport viewport = initializers::viewport((float)geometry_pass.mWidth, (float)geometry_pass.mHeight, 0.0f, 1.0f);
	vkCmdSetViewport(GCommandBuffer, 0, 1, &viewport);

	VkRect2D scissor = initializers::rect2D(geometry_pass.mWidth, geometry_pass.mHeight, 0, 0);
	vkCmdSetScissor(GCommandBuffer, 0, 1, &scissor);

	vkCmdBindPipeline(GCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, geometry_pass.mPipeline);

	vkCmdBindDescriptorSets(GCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, geometry_pass.mPipelineLayout, 0, 1, &geometry_pass.mDescriptorSet, 0, nullptr);

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
		vkCmdPushConstants(GCommandBuffer, geometry_pass.mPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT, 0, sizeof(glm::mat4), &modelMat);
		vkCmdDraw(GCommandBuffer, static_cast<uint32_t>(object->mMesh->vertices.size()), 1, 0, 0);
	}
	totalVertices = accumulatingVertices;
	totalFaces = accumulatingFaces;

	vkCmdEndRenderPass(GCommandBuffer);

	VK_CHECK_RESULT(vkEndCommandBuffer(GCommandBuffer));
}

void Demo::BuildOCommandBuffer()
{
	VkCommandBufferBeginInfo cmdBufInfo = initializers::commandBufferBeginInfo();

	VkClearValue clearValues[1];
	clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };

	VkRenderPassBeginInfo renderPassBeginInfo = initializers::renderPassBeginInfo();
	renderPassBeginInfo.renderPass = occlusion_pass.mRenderPass;
	renderPassBeginInfo.renderArea.offset.x = 0;
	renderPassBeginInfo.renderArea.offset.y = 0;
	renderPassBeginInfo.renderArea.extent.width = occlusion_pass.mWidth;
	renderPassBeginInfo.renderArea.extent.height = occlusion_pass.mHeight;
	renderPassBeginInfo.clearValueCount = 1;
	renderPassBeginInfo.pClearValues = clearValues;

	renderPassBeginInfo.framebuffer = occlusion_pass.mFrameBuffer;

	VK_CHECK_RESULT(vkBeginCommandBuffer(OCommandBuffer, &cmdBufInfo))
		vkCmdBeginRenderPass(OCommandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
	VkViewport viewport = initializers::viewport((float)occlusion_pass.mWidth, (float)occlusion_pass.mHeight, 0.f, 1.f);
	vkCmdSetViewport(OCommandBuffer, 0, 1, &viewport);
	VkRect2D scissor = initializers::rect2D(occlusion_pass.mWidth, occlusion_pass.mHeight, 0, 0);
	vkCmdSetScissor(OCommandBuffer, 0, 1, &scissor);

	vkCmdBindDescriptorSets(OCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, occlusion_pass.mPipelineLayout, 0, 1, &occlusion_pass.mDescriptorSet, 0, nullptr);

	vkCmdBindPipeline(OCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, occlusion_pass.mPipeline);
	vkCmdDraw(OCommandBuffer, 3, 1, 0, 0);

	vkCmdEndRenderPass(OCommandBuffer);

	VK_CHECK_RESULT(vkEndCommandBuffer(OCommandBuffer));
}


void Demo::BuildLightCommandBuffer()
{
	VkCommandBufferBeginInfo cmdBufInfo = initializers::commandBufferBeginInfo();

	VkClearValue clearValues[2];
	clearValues[0].color = { {0.f, 0.f, 0.0f, 0.f} };
	clearValues[1].depthStencil = { 1.f, 0 };

	VkRenderPassBeginInfo renderPassBeginInfo = initializers::renderPassBeginInfo();
	renderPassBeginInfo.renderPass = lighting_pass.mRenderPass;
	renderPassBeginInfo.renderArea.offset.x = 0;
	renderPassBeginInfo.renderArea.offset.y = 0;
	renderPassBeginInfo.renderArea.extent.width = lighting_pass.mWidth;
	renderPassBeginInfo.renderArea.extent.height = lighting_pass.mHeight;
	renderPassBeginInfo.clearValueCount = 2;
	renderPassBeginInfo.pClearValues = clearValues;

	renderPassBeginInfo.framebuffer = lighting_pass.mFrameBuffer;

	VK_CHECK_RESULT(vkBeginCommandBuffer(LightingCommandBuffer, &cmdBufInfo))
		vkCmdBeginRenderPass(LightingCommandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
	VkViewport viewport = initializers::viewport((float)lighting_pass.mWidth, (float)lighting_pass.mHeight, 0.f, 1.f);
	vkCmdSetViewport(LightingCommandBuffer, 0, 1, &viewport);
	VkRect2D scissor = initializers::rect2D(lighting_pass.mWidth, lighting_pass.mHeight, 0, 0);
	vkCmdSetScissor(LightingCommandBuffer, 0, 1, &scissor);

	vkCmdBindDescriptorSets(LightingCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, lighting_pass.mPipelineLayout, 0, 1, &lighting_pass.mDescriptorSet, 0, nullptr);

	vkCmdBindPipeline(LightingCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, lighting_pass.mPipeline);
	vkCmdDraw(LightingCommandBuffer, 3, 1, 0, 0);

	vkCmdEndRenderPass(LightingCommandBuffer);

	VK_CHECK_RESULT(vkEndCommandBuffer(LightingCommandBuffer));
}

void Demo::BuildPostCommandBuffer(int swapChianIndex)
{
	VkCommandBufferBeginInfo cmdBufInfo = initializers::commandBufferBeginInfo();

	// Clear values for all attachments written in the fragment shader
	std::array<VkClearValue, 2> clearValues;
	clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
	clearValues[1].depthStencil = { 1.0f, 0 };

	VkRenderPassBeginInfo renderPassBeginInfo = initializers::renderPassBeginInfo();
	renderPassBeginInfo.renderPass = post_pass.mRenderPass;
	renderPassBeginInfo.framebuffer = post_pass.mFrameBuffer;
	renderPassBeginInfo.renderArea.extent.width = post_pass.mWidth;
	renderPassBeginInfo.renderArea.extent.height = post_pass.mHeight;
	renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassBeginInfo.pClearValues = clearValues.data();

	VK_CHECK_RESULT(vkBeginCommandBuffer(PostCommandBuffer, &cmdBufInfo))

	vkCmdBeginRenderPass(PostCommandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	VkViewport viewport = initializers::viewport((float)post_pass.mWidth, (float)post_pass.mHeight, 0.0f, 1.0f);
	vkCmdSetViewport(PostCommandBuffer, 0, 1, &viewport);

	VkRect2D scissor = initializers::rect2D(post_pass.mWidth, post_pass.mHeight, 0, 0);
	vkCmdSetScissor(PostCommandBuffer, 0, 1, &scissor);

	//
	vkCmdBindPipeline(PostCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, post_pass.mPipeline);
	vkCmdBindDescriptorSets(PostCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, post_pass.mPipelineLayout, 0, 1, &post_pass.mDescriptorSet, 0, nullptr);

	if (DrawNormal == true)
	{
		for (Object* object : objects)
		{
			VkBuffer vertexBuffers[] = { object->mMesh->vertexBuffer };
			VkDeviceSize offsets[] = { 0 };
			vkCmdBindVertexBuffers(PostCommandBuffer, 0, 1, vertexBuffers, offsets);
			glm::mat4 modelMat = object->BuildModelMat();
			vkCmdPushConstants(PostCommandBuffer, post_pass.mPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT, 0, sizeof(glm::mat4), &modelMat);
			vkCmdDraw(PostCommandBuffer, static_cast<uint32_t>(object->mMesh->vertices.size()), 1, 0, 0);
		}
	}
	//


	//
	vkCmdBindPipeline(PostCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, post_pass.mSkyPipeline);
	vkCmdBindDescriptorSets(PostCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, post_pass.mSkyPipelineLayout, 0, 1, &post_pass.mSkyDescriptorSet, 0, nullptr);
	VkBuffer vertexBuffers[] = { Skybox->vertexBuffer };
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(PostCommandBuffer, 0, 1, vertexBuffers, offsets);
	vkCmdDraw(PostCommandBuffer, static_cast<uint32_t>(Skybox->vertices.size()), 1, 0, 0);
	//

	ImGui::Render();
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), PostCommandBuffer);

	vkCmdEndRenderPass(PostCommandBuffer);

	VK_CHECK_RESULT(vkEndCommandBuffer(PostCommandBuffer));
}

void Demo::UpdateUniformBuffer()
{
	static auto startTime = std::chrono::high_resolution_clock::now();

	auto currentTime = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

	UniformBufferMat ubo{};
	ubo.view = camera->getViewMatrix();
	ubo.proj = glm::perspective(glm::radians(45.f), mSwapChain->mSwapChainExtent.width / (float)mSwapChain->mSwapChainExtent.height, 0.1f, 500.f);
	ubo.proj[1][1] *= -1;
	float radius = 10.f;
	float rotateAmount = 0.f;
	if (RotatingLight == true)
	{
		rotateAmount = accumulatingDT;
	}
	for (int i = 0; i < 3; ++i)
	{
		lightsData.point_light[i].mPos = glm::vec3(radius * cos(rotateAmount + glm::radians(120.f * i)), 5.f, radius * sin(rotateAmount + glm::radians(120.f * i)));
	}

	//Calculate shadowing view & projection mat

	glm::vec3 lightPos = lightsData.point_light[0].mPos + glm::vec3(0.f, 15.f, 0.f);
	glm::mat4 lightProjection = glm::perspective(glm::radians(45.f), mSwapChain->mSwapChainExtent.width / (float)mSwapChain->mSwapChainExtent.height, 1.f, 96.f);
	lightProjection[1][1] *= -1;
	glm::mat4 lightView = glm::lookAt(lightPos, glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));

	lightMatData.lightMVP = lightProjection * lightView;
	//lightMatData.lightMVP = ubo.proj * ubo.view;
	
	//Update data
	void* Matdata;
	vkMapMemory(mVulkanDevice->logicalDevice, matUBO.memory, 0, sizeof(ubo), 0, &Matdata);
	memcpy(Matdata, &ubo, sizeof(ubo));
	vkUnmapMemory(mVulkanDevice->logicalDevice, matUBO.memory);

	lightsData.lookVec = (camera->front - camera->position);
	void* Lightdata;
	vkMapMemory(mVulkanDevice->logicalDevice, lightUBO.memory, 0, sizeof(UniformBufferLights), 0, &Lightdata);
	memcpy(Lightdata, &lightsData, sizeof(lightsData));
	vkUnmapMemory(mVulkanDevice->logicalDevice, lightUBO.memory);

	void* LightMVP;
	vkMapMemory(mVulkanDevice->logicalDevice, lightMatUBO.memory, 0, sizeof(LightMatUBO), 0, &LightMVP);
	memcpy(LightMVP, &lightMatData, sizeof(lightMatData));
	vkUnmapMemory(mVulkanDevice->logicalDevice, lightMatUBO.memory);
}

void Demo::UpdateDescriptorSet()
{
	VkDescriptorBufferInfo MatBufferInfo{};
	MatBufferInfo.buffer = matUBO.buffer;
	MatBufferInfo.offset = 0;
	MatBufferInfo.range = sizeof(UniformBufferMat);

	VkDescriptorBufferInfo LightbufferInfo{};
	LightbufferInfo.buffer = lightUBO.buffer;
	LightbufferInfo.offset = 0;
	LightbufferInfo.range = sizeof(UniformBufferLights);

	VkDescriptorImageInfo texPosDisc;
	texPosDisc.sampler = colorSampler;
	texPosDisc.imageView = geometry_pass.mPosition.view;
	texPosDisc.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkDescriptorImageInfo texNormalDisc;
	texNormalDisc.sampler = colorSampler;
	texNormalDisc.imageView = geometry_pass.mNormal.view;
	texNormalDisc.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkDescriptorImageInfo texColorDisc;
	texColorDisc.sampler = colorSampler;
	texColorDisc.imageView = geometry_pass.mAlbedo.view;
	texColorDisc.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkDescriptorImageInfo modelDiffuseDisc;
	modelDiffuseDisc.sampler = testImage.sampler;
	modelDiffuseDisc.imageView = testImage.imageView;
	modelDiffuseDisc.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkDescriptorImageInfo cubemapDisc;
	cubemapDisc.sampler = testCubemap.sampler;
	cubemapDisc.imageView = testCubemap.imageView;
	cubemapDisc.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkDescriptorBufferInfo LightMatBufferInfo{};
	LightMatBufferInfo.buffer = lightMatUBO.buffer;
	LightMatBufferInfo.offset = 0;
	LightMatBufferInfo.range = sizeof(LightMatUBO);

	VkDescriptorImageInfo shadowDepthDisc{};
	shadowDepthDisc.sampler = shadowDepthSampler;
	shadowDepthDisc.imageView = shadow_pass.mDepth.view;
	shadowDepthDisc.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	std::vector<VkWriteDescriptorSet> GBufWriteDescriptorSets;
	GBufWriteDescriptorSets = {
		initializers::writeDescriptorSet(geometry_pass.mDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &MatBufferInfo),
		initializers::writeDescriptorSet(geometry_pass.mDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 5, &modelDiffuseDisc)
	};
	geometry_pass.UpdateDescriptorSet(GBufWriteDescriptorSets);

	std::vector<VkWriteDescriptorSet> OcclusionDescriptorSets;
	OcclusionDescriptorSets = {
		initializers::writeDescriptorSet(occlusion_pass.mDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &texPosDisc),
		initializers::writeDescriptorSet(occlusion_pass.mDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, &texNormalDisc),
		initializers::writeDescriptorSet(occlusion_pass.mDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4, &texColorDisc),
	};
	occlusion_pass.UpdateDescriptorSet(OcclusionDescriptorSets);

	std::vector<VkWriteDescriptorSet> lightWriteDescriptorSets;
	lightWriteDescriptorSets = {
		initializers::writeDescriptorSet(lighting_pass.mDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, &LightbufferInfo),
		initializers::writeDescriptorSet(lighting_pass.mDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &texPosDisc),
		initializers::writeDescriptorSet(lighting_pass.mDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, &texNormalDisc),
		initializers::writeDescriptorSet(lighting_pass.mDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4, &texColorDisc),
		initializers::writeDescriptorSet(lighting_pass.mDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 7, &LightMatBufferInfo),
		initializers::writeDescriptorSet(lighting_pass.mDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 8, &shadowDepthDisc)
	};
	lighting_pass.UpdateDescriptorSet(lightWriteDescriptorSets);
	
	std::vector<VkWriteDescriptorSet> PBufWriteDescriptorSets;
	PBufWriteDescriptorSets = {
		initializers::writeDescriptorSet(post_pass.mDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &MatBufferInfo),
	};
	post_pass.UpdateDescriptorSet(PBufWriteDescriptorSets);

	std::vector<VkWriteDescriptorSet> PSkyBufWriteDescriptorSets;
	PSkyBufWriteDescriptorSets = {
		initializers::writeDescriptorSet(post_pass.mSkyDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &MatBufferInfo),
		initializers::writeDescriptorSet(post_pass.mSkyDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 6, &cubemapDisc)
	};
	post_pass.UpdateSkyDescriptorSet(PSkyBufWriteDescriptorSets);

	std::vector<VkWriteDescriptorSet> SBufWriteDescriptorSets;
	SBufWriteDescriptorSets = {
		initializers::writeDescriptorSet(shadow_pass.mDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 7, &LightMatBufferInfo),
	};
	shadow_pass.UpdateDescriptorSet(SBufWriteDescriptorSets);
	//TODO: Update함수를 여기서 쓰는것이 더 좋아보인다.
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