#include "L_Pass.h"
#include "VkApp.h"
#include "VulkanInitializers.hpp"
#include "VulkanTools.h"
#include <glm/glm.hpp>

#include "Mesh.h"

void L_Pass::Init(VkApp* app, uint32_t width, uint32_t height)
{
	mApp = app;
	mWidth = width;
	mHeight = height;
}

void L_Pass::CreateFrameData()
{
	CreateAttachment();
	CreateRenderPass();
	CreateFrameBuffer();

}

void L_Pass::CreatePipelineData()
{
	CreatePipelineLayout();
	CreatePipeline();
}

void L_Pass::CreateAttachment()
{
	VkImageUsageFlagBits lightAttachmentUsage = static_cast<VkImageUsageFlagBits>(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
	mApp->CreateAttachment(VK_FORMAT_B8G8R8A8_SRGB, lightAttachmentUsage, &mComposition);
}

void L_Pass::CreateRenderPass()
{
	VkAttachmentDescription compositionDesc = {};
	compositionDesc.samples = VK_SAMPLE_COUNT_1_BIT;
	compositionDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	compositionDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	compositionDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	compositionDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	compositionDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	compositionDesc.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	compositionDesc.format = mComposition.format;

	VkAttachmentReference compositionReference = {};
	compositionReference.attachment = 0;
	compositionReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.pColorAttachments = &compositionReference;
	subpass.colorAttachmentCount = 1;
	subpass.pDepthStencilAttachment = VK_NULL_HANDLE;

	// Use subpass dependencies for attachment layout transitions
	// src: Before render
	// dst: After render
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
	renderPassInfo.pAttachments = &compositionDesc;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 2;
	renderPassInfo.pDependencies = dependencies.data();
	VK_CHECK_RESULT(vkCreateRenderPass(mApp->mVulkanDevice->logicalDevice, &renderPassInfo, nullptr, &mRenderPass))
}

void L_Pass::CreateFrameBuffer()
{
	VkImageView attachment = mComposition.view;

	VkFramebufferCreateInfo fbufCreateInfo = {};
	fbufCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	fbufCreateInfo.pNext = NULL;
	fbufCreateInfo.renderPass = mRenderPass;
	fbufCreateInfo.pAttachments = &attachment;
	fbufCreateInfo.attachmentCount = 1;
	fbufCreateInfo.width = mWidth;
	fbufCreateInfo.height = mHeight;
	fbufCreateInfo.layers = 1;
	VK_CHECK_RESULT(vkCreateFramebuffer(mApp->mVulkanDevice->logicalDevice, &fbufCreateInfo, nullptr, &mFrameBuffer));
}

void L_Pass::CreateDescriptorPool(const std::vector<VkDescriptorPoolSize>& poolSizes)
{
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

void L_Pass::CreateDescriptorLayout(const std::vector<VkDescriptorSetLayoutBinding>& setLayoutBindings)
{
	VkDescriptorSetLayoutCreateInfo lightDescriptorLayout = initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(mApp->mVulkanDevice->logicalDevice, &lightDescriptorLayout, nullptr, &mDescriptorLayout))
}

void L_Pass::CreateDescriptorSet()
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

void L_Pass::UpdateDescriptorSet(const std::vector<VkWriteDescriptorSet>& writeDescSets)
{
	vkUpdateDescriptorSets(mApp->mVulkanDevice->logicalDevice, static_cast<uint32_t>(writeDescSets.size()), writeDescSets.data(), 0, nullptr);
}

void L_Pass::CreatePipelineLayout()
{
	VkPipelineLayoutCreateInfo LightpipelineLayoutCreateInfo = initializers::pipelineLayoutCreateInfo(&mDescriptorLayout, 1);
	VK_CHECK_RESULT(vkCreatePipelineLayout(mApp->mVulkanDevice->logicalDevice, &LightpipelineLayoutCreateInfo, nullptr, &mPipelineLayout))
}

void L_Pass::CreatePipeline()
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

	rasterizationState.cullMode = VK_CULL_MODE_FRONT_BIT;
	shaderStages[0] = createShaderStageCreateInfo("../shaders/LightingVert.spv", VK_SHADER_STAGE_VERTEX_BIT, mApp->mVulkanDevice->logicalDevice);
	shaderStages[1] = createShaderStageCreateInfo("../shaders/LightingFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT, mApp->mVulkanDevice->logicalDevice);

	VkPipelineVertexInputStateCreateInfo emptyInput = initializers::pipelineVertexInputStateCreateInfo();
	pipelineCI.pVertexInputState = &emptyInput;
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(mApp->mVulkanDevice->logicalDevice, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &mPipeline))
}

void L_Pass::Update()
{
}