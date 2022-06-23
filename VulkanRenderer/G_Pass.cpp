#include "G_Pass.h"
#include "VkApp.h"
#include "VulkanInitializers.hpp"
#include "VulkanTools.h"
#include <glm/glm.hpp>

#include "Mesh.h"

void G_Pass::Init(VkApp* app, uint32_t width, uint32_t height)
{
	mApp = app;
	mWidth = width;
	mHeight = height;

	CreateAttachment();
	CreateRenderPass();
	CreateFrameBuffer();

	CreateDescriptorPool();
	//CreateDescriptorLayout();TODO: 외부에서 선언된 VkDescriptorSetLayoutBinding을 통해 초기화 해야함!
	CreateDescriptorSet();

	CreatePipelineLayout();
	CreatePipeline();
}

void G_Pass::CreateAttachment()
{
	mApp->CreateAttachment(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &mPosition);
	mApp->CreateAttachment(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &mNormal);
	mApp->CreateAttachment(VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &mAlbedo);

	VkFormat attDepthFormat = mApp->FindDepthFormat();
	mApp->CreateAttachment(attDepthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, &mDepth);
}

void G_Pass::CreateRenderPass()
{
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

	attachmentDescs[0].format = mPosition.format;
	attachmentDescs[1].format = mNormal.format;
	attachmentDescs[2].format = mAlbedo.format;
	attachmentDescs[3].format = mDepth.format;

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
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
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
	VK_CHECK_RESULT(vkCreateRenderPass(mApp->mVulkanDevice->logicalDevice, &renderPassInfo, nullptr, &mRenderPass))
}

void G_Pass::CreateFrameBuffer()
{
	std::array<VkImageView, 4> attachments;
	attachments[0] = mPosition.view;
	attachments[1] = mNormal.view;
	attachments[2] = mAlbedo.view;
	attachments[3] = mDepth.view;

	VkFramebufferCreateInfo fbufCreateInfo = {};
	fbufCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	fbufCreateInfo.pNext = NULL;
	fbufCreateInfo.renderPass = mRenderPass;
	fbufCreateInfo.pAttachments = attachments.data();
	fbufCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	fbufCreateInfo.width = mWidth;
	fbufCreateInfo.height = mHeight;
	fbufCreateInfo.layers = 1;
	VK_CHECK_RESULT(vkCreateFramebuffer(mApp->mVulkanDevice->logicalDevice, &fbufCreateInfo, nullptr, &mFrameBuffer));
}

void G_Pass::CreateDescriptorPool()
{
	//G_Pass use matrix for Uniform data and push constant.
	//Later, maybe use diffuse, normal, specular map.

	VkDescriptorPoolSize matPoolsize{};
	matPoolsize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	matPoolsize.descriptorCount = 2;//2 for view, project

	std::vector<VkDescriptorPoolSize> poolSizes = { matPoolsize };

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();

	poolInfo.maxSets = poolSizes.size();

	if (vkCreateDescriptorPool(mApp->mVulkanDevice->logicalDevice, &poolInfo, nullptr, &mDescriptorPool) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create descriptor pool!");
	}
}

void G_Pass::CreateDescriptorLayout(const std::vector<VkDescriptorSetLayoutBinding>& setLayoutBindings)
{
	VkDescriptorSetLayoutCreateInfo lightDescriptorLayout = initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(mApp->mVulkanDevice->logicalDevice, &lightDescriptorLayout, nullptr, &mDescriptorLayout))
}

void G_Pass::CreateDescriptorSet()
{
	VkDescriptorSetAllocateInfo setAllocInfo{};
	setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	setAllocInfo.descriptorPool = mDescriptorPool;
	setAllocInfo.descriptorSetCount = 1;
	setAllocInfo.pSetLayouts = &mDescriptorLayout;

	if (vkAllocateDescriptorSets(mApp->mVulkanDevice->logicalDevice, &setAllocInfo, &mDescriptorSet) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate descriptor sets");
	}
}

void G_Pass::UpdateDescriptorSet(const std::vector<VkWriteDescriptorSet>& writeDescSets)
{
	//TODO: 외부에 저장된 data들을 가지고 와야함!
}

void G_Pass::CreatePipelineLayout()
{
	VkPushConstantRange pushConstant;
	pushConstant.offset = 0;
	pushConstant.size = sizeof(glm::mat4);
	pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT;

	VkPipelineLayoutCreateInfo pipelinelayoutCI = initializers::pipelineLayoutCreateInfo(&mDescriptorLayout, 1);
	pipelinelayoutCI.pushConstantRangeCount = 1;
	pipelinelayoutCI.pPushConstantRanges = &pushConstant;

	VK_CHECK_RESULT(vkCreatePipelineLayout(mApp->mVulkanDevice->logicalDevice, &pipelinelayoutCI, nullptr, &mPipelineLayout))
}

void G_Pass::CreatePipeline()
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

	VkGraphicsPipelineCreateInfo pipelineCI = initializers::pipelineCreateInfo(mPipelineLayout, mRenderPass);
	pipelineCI.pInputAssemblyState = &inputAssemblyState;
	pipelineCI.pRasterizationState = &rasterizationState;
	pipelineCI.pColorBlendState = &colorBlendState;
	pipelineCI.pMultisampleState = &multisampleState;
	pipelineCI.pViewportState = &viewportState;
	pipelineCI.pDepthStencilState = &depthStencilState;
	pipelineCI.pDynamicState = &dynamicState;
	pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
	pipelineCI.pStages = shaderStages.data();


	VkPipelineVertexInputStateCreateInfo vertexInputInfo {};
	auto bindingDescription = Vertex::getBindingDescription();
	auto attributeDescriptions = Vertex::getAttributeDescriptions();

	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

	pipelineCI.layout = mPipelineLayout;
	pipelineCI.pVertexInputState = &vertexInputInfo;
	rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;

	shaderStages[0] = createShaderStageCreateInfo("../shaders/GBufferVert.spv", VK_SHADER_STAGE_VERTEX_BIT, mApp->mVulkanDevice->logicalDevice);
	shaderStages[1] = createShaderStageCreateInfo("../shaders/GBufferFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT, mApp->mVulkanDevice->logicalDevice);

	pipelineCI.renderPass = mRenderPass;

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

	VK_CHECK_RESULT(vkCreateGraphicsPipelines(mApp->mVulkanDevice->logicalDevice, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &mPipeline))
}

void G_Pass::Update()
{
}