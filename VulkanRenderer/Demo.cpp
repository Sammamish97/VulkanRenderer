#include "Demo.h"
#include "VulkanTools.h"
#include "VulkanInitializers.hpp"
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"

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
	CreateLight();
	CreateCamera();
	CreateSyncObjects();

	CreateGRenderPass();
	CreateGFramebuffer();

	CreateUniformBuffers();

	CreateDescriptorPool();
	CreateSampler();
	CreateDescriptorSetLayout();
	CreateDescriptorSet();

	CreateGraphicsPipelines();
	CreateCommandBuffers();
}

void Demo::Update()
{
	while (glfwWindowShouldClose(mWindow) == false)
	{
		FrameStart();

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
	BuildLightCommandBuffer(imageindex);

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
	VK_CHECK_RESULT(vkQueueSubmit(mGraphicsQueue, 1, &lightSubmitInfo, inFlightFence))

		//

		VkPresentInfoKHR presentInfo {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &renderComplete;

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
	//
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


	objects.push_back(new Object(redMesh, glm::vec3(0.f, 3.f, 0.f)));
	objects.push_back(new Object(greenMesh, glm::vec3(1.5f, 0.f, 0.f)));
	objects.push_back(new Object(BlueMesh, glm::vec3(-1.5f, 0.f, 0.f)));
}

void Demo::CreateLight()
{
	lightsData.dir_light[0] = DirLight(glm::vec3(0.1f, 0.5f, 0.1f), glm::vec3(-0.5f, -0.5f, -0.5f));
	lightsData.dir_light[1] = DirLight(glm::vec3(0.5f, 0.1f, 0.1), glm::vec3(0.5f, 0.5f, 0.5f));
	lightsData.dir_light[2] = DirLight(glm::vec3(0.1f, 0.1f, 0.5f), glm::vec3(-0.5f, 0.5f, 0.5f));
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
		vkCreateFence(mVulkanDevice->logicalDevice, &fenceInfo, nullptr, &inFlightFence) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create semaphores!");
	}
}

void Demo::CreateGRenderPass()
{
	//Create Render Pass for GBuffer
	mGFrameBuffer.width = FB_DIM;
	mGFrameBuffer.height = FB_DIM;

	// (World space) Positions
	CreateAttachment(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &mGFrameBuffer.position);

	// (World space) Normals
	CreateAttachment(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &mGFrameBuffer.normal);

	// Albedo (color)
	CreateAttachment(VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &mGFrameBuffer.albedo);

	VkFormat attDepthFormat = FindDepthFormat();

	// Depth
	CreateAttachment(attDepthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, &mGFrameBuffer.depth);

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
	VK_CHECK_RESULT(vkCreateRenderPass(mVulkanDevice->logicalDevice, &renderPassInfo, nullptr, &mGFrameBuffer.renderPass));
}

void Demo::CreateGFramebuffer()
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
	VK_CHECK_RESULT(vkCreateFramebuffer(mVulkanDevice->logicalDevice, &fbufCreateInfo, nullptr, &mGFrameBuffer.framebuffer));
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
	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(mVulkanDevice->logicalDevice, &lightDescriptorLayout, nullptr, &LightDescriptorSetLayout))

		VkPipelineLayoutCreateInfo LightpipelineLayoutCreateInfo = initializers::pipelineLayoutCreateInfo(&LightDescriptorSetLayout, 1);
	VK_CHECK_RESULT(vkCreatePipelineLayout(mVulkanDevice->logicalDevice, &LightpipelineLayoutCreateInfo, nullptr, &LightPipelineLayout))

		//Create Lighting descriptor set layout and pipeilne
		std::vector<VkDescriptorSetLayoutBinding> GLayoutBinding = { matLayoutBinding };

	VkDescriptorSetLayoutCreateInfo GDescriptorLayout = initializers::descriptorSetLayoutCreateInfo(GLayoutBinding);

	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(mVulkanDevice->logicalDevice, &GDescriptorLayout, nullptr, &GDescriptorSetLayout))
		VkPushConstantRange pushConstant;
	pushConstant.offset = 0;
	pushConstant.size = sizeof(glm::mat4);
	pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkPipelineLayoutCreateInfo GpipelineLayoutCreateInfo = initializers::pipelineLayoutCreateInfo(&GDescriptorSetLayout, 1);
	GpipelineLayoutCreateInfo.pushConstantRangeCount = 1;
	GpipelineLayoutCreateInfo.pPushConstantRanges = &pushConstant;

	VK_CHECK_RESULT(vkCreatePipelineLayout(mVulkanDevice->logicalDevice, &GpipelineLayoutCreateInfo, nullptr, &GPipelineLayout))
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

	if (vkAllocateDescriptorSets(mVulkanDevice->logicalDevice, &GAllocInfo, &GBufferDescriptorSet) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate descriptor sets");
	}

	std::vector<VkWriteDescriptorSet> GBufWriteDescriptorSets;
	GBufWriteDescriptorSets = {
		initializers::writeDescriptorSet(GBufferDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &GBufferInfo)
	};

	vkUpdateDescriptorSets(mVulkanDevice->logicalDevice, static_cast<uint32_t>(GBufWriteDescriptorSets.size()), GBufWriteDescriptorSets.data(), 0, nullptr);
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

	//Need to know swap chain index for get proper swap chain image.
	VkGraphicsPipelineCreateInfo pipelineCI = initializers::pipelineCreateInfo(LightPipelineLayout, mSwapChain->mSwapChainRenderPass);
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
}

void Demo::BuildLightCommandBuffer(int swapChianIndex)
{
	VkCommandBufferBeginInfo cmdBufInfo = initializers::commandBufferBeginInfo();

	VkClearValue clearValues[2];
	clearValues[0].color = { {0.f, 0.f, 0.2f, 0.f} };
	clearValues[1].depthStencil = { 1.f, 0 };

	VkRenderPassBeginInfo renderPassBeginInfo = initializers::renderPassBeginInfo();
	renderPassBeginInfo.renderPass = mSwapChain->mSwapChainRenderPass;
	renderPassBeginInfo.renderArea.offset.x = 0;
	renderPassBeginInfo.renderArea.offset.y = 0;
	renderPassBeginInfo.renderArea.extent.width = WIDTH;
	renderPassBeginInfo.renderArea.extent.height = HEIGHT;
	renderPassBeginInfo.clearValueCount = 2;
	renderPassBeginInfo.pClearValues = clearValues;

	renderPassBeginInfo.framebuffer = mSwapChain->mSwapChainRenderDatas[swapChianIndex].mFrameBufferData.mFramebuffer;

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

void Demo::UpdateUniformBuffer(uint32_t currentImage)
{
	static auto startTime = std::chrono::high_resolution_clock::now();

	auto currentTime = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

	UniformBufferMat ubo{};
	ubo.view = camera->getViewMatrix();
	ubo.proj = glm::perspective(glm::radians(45.f), mSwapChain->mSwapChainExtent.width / (float)mSwapChain->mSwapChainExtent.height, 0.1f, 100.f);
	ubo.proj[1][1] *= -1;


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

void Demo::CreatePostRenderPass()
{
	std::array<VkAttachmentDescription, 2> attachments{};
	// Color attachment
	attachments[0].format = VK_FORMAT_B8G8R8A8_UNORM;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;

	// Depth attachment
	attachments[1].format = VK_FORMAT_X8_D24_UNORM_PACK32;
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;

	const VkAttachmentReference colorReference{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
	const VkAttachmentReference depthReference{ 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };


	std::array<VkSubpassDependency, 1> subpassDependencies{};
	// Transition from final to initial (VK_SUBPASS_EXTERNAL refers to all commands executed outside of the actual renderpass)
	subpassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	subpassDependencies[0].dstSubpass = 0;
	subpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	subpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	subpassDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
		| VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	subpassDependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	VkSubpassDescription subpassDescription{};
	subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDescription.colorAttachmentCount = 1;
	subpassDescription.pColorAttachments = &colorReference;
	subpassDescription.pDepthStencilAttachment = &depthReference;

	VkRenderPassCreateInfo renderPassInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpassDescription;
	renderPassInfo.dependencyCount = static_cast<uint32_t>(subpassDependencies.size());
	renderPassInfo.pDependencies = subpassDependencies.data();

	vkCreateRenderPass(mVulkanDevice->logicalDevice, &renderPassInfo, nullptr, &mPostRenderPass);
	// To destroy: vkDestroyRenderPass(m_device, m_postRenderPass, nullptr);
}

void Demo::InitGUI()
{
	CreatePostRenderPass();
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

	ImGui_ImplVulkan_Init(&init_info, mPostRenderPass);

	// Upload Fonts
	VkCommandBuffer cmdbuf = CreateTempCmdBuf();
	ImGui_ImplVulkan_CreateFontsTexture(cmdbuf);
	SubmitTempCmdBuf(cmdbuf);

	ImGui_ImplGlfw_InitForVulkan(mWindow, true);
}

void Demo::DrawGUI()
{

}