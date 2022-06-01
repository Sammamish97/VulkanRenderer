#include "SwapChain.h"
#include "Application.h"
#include "VulkanTools.h"
#include <GLFW/glfw3.h>

SwapChain::SwapChain(HelloTriangleApplication* vkApp)
	:mApp(vkApp)
{
	CreateSwapChain();
}

void SwapChain::CreateSwapChain()
{
	SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport();
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
	createInfo.surface = mSurface;
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

void SwapChain::CreateSurface()
{
	VK_CHECK_RESULT(glfwCreateWindowSurface(mApp->instance, mApp->window, nullptr, &mSurface))
}

void SwapChain::CreateSwapChainImageView()
{
}

void SwapChain::CreateSwapChainRenderPass()
{
}

void SwapChain::CreateSwapChainFrameBuffer()
{
}

SwapChainSupportDetails SwapChain::QuerySwapChainSupport()
{
	SwapChainSupportDetails details;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mApp->vulkanDevice->physicalDevice, mSurface, &details.capabilities);

	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(mApp->vulkanDevice->physicalDevice, mSurface, &formatCount, nullptr);

	if(formatCount != 0)
	{
		details.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(mApp->vulkanDevice->physicalDevice, mSurface, &formatCount, details.formats.data());
	}

	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(mApp->vulkanDevice->physicalDevice, mSurface, &presentModeCount, nullptr);

	if (presentModeCount != 0)
	{
		details.presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(mApp->vulkanDevice->physicalDevice, mSurface, &presentModeCount, details.presentModes.data());
	}
	return details;
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
