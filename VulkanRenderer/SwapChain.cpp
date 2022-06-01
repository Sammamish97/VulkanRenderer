#include "SwapChain.h"
#include "Application.h"
#include "VulkanTools.h"
#include "VulkanInitializers.hpp"
#include <GLFW/glfw3.h>

SwapChain::SwapChain(HelloTriangleApplication* vkApp)
	:mApp(vkApp)
{
	CreateSwapChain();
	CacheSwapChainImage();
	CreateSwapChainImageView();
	CreateSwapChainRenderPass();
	CreateSwapChainFrameBuffer();
}

void SwapChain::CreateSwapChain()
{
	SwapChainSupportDetails swapChainSupport = mApp->QuerySwapChainSupport(mApp->vulkanDevice->physicalDevice);
	mSwapChainFormat = PickSurfaceFormat(swapChainSupport.formats);
	VkPresentModeKHR presentMode = PickPresentMode(swapChainSupport.presentModes);
	mSwapChainExtent = PickExtent(swapChainSupport.capabilities);

	mImageCount = swapChainSupport.capabilities.minImageCount + 1;

	if(swapChainSupport.capabilities.maxImageCount > 0 && mImageCount > swapChainSupport.capabilities.maxImageCount)
	{
		mImageCount = swapChainSupport.capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = mApp->mSurface;
	createInfo.minImageCount = mImageCount;
	createInfo.imageFormat = mSwapChainFormat.format;
	createInfo.imageColorSpace = mSwapChainFormat.colorSpace;
	createInfo.imageExtent = mSwapChainExtent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	auto queueFamilyIndices = mApp->vulkanDevice->queueFamilyIndices;
	createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	createInfo.queueFamilyIndexCount = 0;
	createInfo.pQueueFamilyIndices = nullptr;//TODO: Find purpose of this codes. Family indices and this init code do not used.

	createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;
	createInfo.oldSwapchain = VK_NULL_HANDLE;

	VK_CHECK_RESULT(vkCreateSwapchainKHR(mApp->vulkanDevice->logicalDevice, &createInfo, nullptr, &mSwapChain))
}

void SwapChain::CacheSwapChainImage()
{
	uint32_t imageCount;

	std::vector<VkImage> swapChainImages;
	vkGetSwapchainImagesKHR(mApp->vulkanDevice->logicalDevice, mSwapChain, &imageCount, nullptr);
	swapChainImages.resize(imageCount);

	vkGetSwapchainImagesKHR(mApp->vulkanDevice->logicalDevice, mSwapChain, &imageCount, swapChainImages.data());

	for (uint32_t i = 0; i < imageCount; ++i)
	{
		mSwapChainFrameBuffers.push_back(SwapChainFrameBuffer(mSwapChainExtent.width, mSwapChainExtent.height));
		mSwapChainFrameBuffers[i].mColorAttachment.image = swapChainImages[i];
		mSwapChainFrameBuffers[i].mColorAttachment.format = mSwapChainFormat.format;
	}
}

VkSwapchainKHR SwapChain::GetSwapChain()
{
	return mSwapChain;
}

SwapChainFrameBuffer SwapChain::GetFrameBuffer(uint32_t availableIndex)
{
	return mSwapChainFrameBuffers[availableIndex];
}

void SwapChain::CreateSwapChainImageView()
{
	for (size_t i = 0; i < mSwapChainFrameBuffers.size(); ++i)
	{
		VkImageViewCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = mSwapChainFrameBuffers[i].mColorAttachment.image;
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = mSwapChainFrameBuffers[i].mColorAttachment.format;

		createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;

		VK_CHECK_RESULT(vkCreateImageView(mApp->vulkanDevice->logicalDevice, &createInfo, nullptr, &mSwapChainFrameBuffers[i].mColorAttachment.view))
	}
}

void SwapChain::CreateSwapChainFrameBuffer()
{
	//Image view must created before call this function.
	//Render pass must built before call this function.
	for (int i = 0; i < mSwapChainFrameBuffers.size(); ++i)
	{
		VkFramebufferCreateInfo frameBufferCI = {};
		frameBufferCI.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		frameBufferCI.pNext = nullptr;
		frameBufferCI.renderPass = mSwapChainFrameBuffers[i].mRenderPass;
		frameBufferCI.attachmentCount = 1;
		frameBufferCI.pAttachments = &mSwapChainFrameBuffers[i].mColorAttachment.view;
		frameBufferCI.width = mSwapChainFrameBuffers[i].mWidth;
		frameBufferCI.height = mSwapChainFrameBuffers[i].mHeight;
		frameBufferCI.layers = 1;

		VK_CHECK_RESULT(vkCreateFramebuffer(mApp->vulkanDevice->logicalDevice, &frameBufferCI, nullptr, &mSwapChainFrameBuffers[i].mFramebuffer))
	}
}

void SwapChain::CreateSwapChainRenderPass()
{
	//Swap chain framebuffer use only one color attachment.
	//Color attachment layout: Render(write) -> Read(Present)
	for (int i = 0; i < mSwapChainFrameBuffers.size(); ++i)
	{
		VkAttachmentDescription colorAttachmentDesc = {};
		colorAttachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;//Do not deal with multi-sample yet.
		colorAttachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;//Store attachment data after rendering for present.
		colorAttachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachmentDesc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;//TODO: What if layout changed between the middle of operation?
		colorAttachmentDesc.format = mSwapChainFrameBuffers[i].mColorAttachment.format;

		VkAttachmentReference colorAttachRef = {};
		colorAttachRef.attachment = 0;
		colorAttachRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpassDesc = {};
		subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDesc.colorAttachmentCount = 1;
		subpassDesc.pColorAttachments = &colorAttachRef;
		subpassDesc.pDepthStencilAttachment = VK_NULL_HANDLE;
		subpassDesc.inputAttachmentCount = 0;
		subpassDesc.pInputAttachments = nullptr;
		subpassDesc.preserveAttachmentCount = 0;
		subpassDesc.pPreserveAttachments = nullptr;
		subpassDesc.pResolveAttachments = nullptr;

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

		VkRenderPassCreateInfo swapChainRenderPassCI = {};
		swapChainRenderPassCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		swapChainRenderPassCI.attachmentCount = 1;
		swapChainRenderPassCI.pAttachments = &colorAttachmentDesc;
		swapChainRenderPassCI.subpassCount = 1;
		swapChainRenderPassCI.pSubpasses = &subpassDesc;
		swapChainRenderPassCI.dependencyCount = static_cast<uint32_t>(dependencies.size());
		swapChainRenderPassCI.pDependencies = dependencies.data();

		VK_CHECK_RESULT(vkCreateRenderPass(mApp->vulkanDevice->logicalDevice, &swapChainRenderPassCI, nullptr, &mSwapChainFrameBuffers[i].mRenderPass));
	}
}

VkSurfaceFormatKHR SwapChain::PickSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
{
	for (const auto& availableFormat : availableFormats)
	{
		if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			return availableFormat;
		}
	}
	return availableFormats[0];
}

VkPresentModeKHR SwapChain::PickPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
{
	for (const auto& availablePresentMode : availablePresentModes)
	{
		if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			return availablePresentMode;
		}
	}
	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D SwapChain::PickExtent(const VkSurfaceCapabilitiesKHR& capabilities)
{
	if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
	{
		return capabilities.currentExtent;
	}
	else
	{
		int width, height;
		glfwGetFramebufferSize(mApp->window, &width, &height);

		VkExtent2D actualExtent = {
			static_cast<uint32_t>(width),
			static_cast<uint32_t>(height)
		};

		actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

		return actualExtent;
	}
}
