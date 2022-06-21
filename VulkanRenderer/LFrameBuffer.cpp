#include "LFrameBuffer.h"
#include "VulkanTools.h"
#include "VkApp.h"
void LFrameBuffer::Init(VkApp* appPtr)
{
	app = appPtr;
	CreateRenderPass();
	CreateFrameBuffer();
}

void LFrameBuffer::CreateRenderPass()
{
	//Create Render Pass for GBuffer
	width = WIDTH;
	height = HEIGHT;

	VkImageUsageFlagBits lightAttachmentUsage = static_cast<VkImageUsageFlagBits>(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

	// (World space) Positions
	app->CreateAttachment(VK_FORMAT_B8G8R8A8_SRGB, lightAttachmentUsage, &composition);

	VkAttachmentDescription compositionDesc = {};
	compositionDesc.samples = VK_SAMPLE_COUNT_1_BIT;
	compositionDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	compositionDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	compositionDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	compositionDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	compositionDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	compositionDesc.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	compositionDesc.format = composition.format;

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
	VK_CHECK_RESULT(vkCreateRenderPass(app->mVulkanDevice->logicalDevice, &renderPassInfo, nullptr, &renderPass));
}

void LFrameBuffer::CreateFrameBuffer()
{
	VkImageView attachment = composition.view;

	VkFramebufferCreateInfo fbufCreateInfo = {};
	fbufCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	fbufCreateInfo.pNext = NULL;
	fbufCreateInfo.renderPass = renderPass;
	fbufCreateInfo.pAttachments = &attachment;
	fbufCreateInfo.attachmentCount = 1;
	fbufCreateInfo.width = width;
	fbufCreateInfo.height = height;
	fbufCreateInfo.layers = 1;
	VK_CHECK_RESULT(vkCreateFramebuffer(app->mVulkanDevice->logicalDevice, &fbufCreateInfo, nullptr, &framebuffer));
}