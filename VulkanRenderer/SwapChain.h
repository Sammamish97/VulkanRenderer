#pragma once
#include <vulkan/vulkan.hpp>
#include "FrameBuffer.h"

class HelloTriangleApplication;

struct SwapChainFrameBuffer
{
	SwapChainFrameBuffer() = default;
	SwapChainFrameBuffer(int32_t width, int32_t height)
		: mWidth(width), mHeight(height) {}

	int32_t mWidth, mHeight;
	VkFramebuffer mFramebuffer;
	FrameBufferAttachment mColorAttachment;
	FrameBufferAttachment mDepthAttachment;
	VkRenderPass mRenderPass;
};

struct SwapChainSupportDetails
{
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

class SwapChain
{
public:
	VkSwapchainKHR mSwapChain;

	VkRenderPass mGlobalSwapChainRenderPass;

	VkExtent2D mSwapChainExtent;
	VkSurfaceFormatKHR mSwapChainFormat;
	
	std::vector<SwapChainFrameBuffer> mSwapChainFrameBuffers;

	uint32_t mImageCount = 0;
	//Pipeline

private:
	HelloTriangleApplication* mApp = nullptr;

public:
	SwapChain(HelloTriangleApplication* vkApp);

public:
	void CreateSwapChain();
	VkSwapchainKHR GetSwapChain();
	SwapChainFrameBuffer GetFrameBuffer(uint32_t availableIndex);

private:
	void CacheSwapChainImage();
	void CreateSwapChainImageView();
	void CreateSwapChainRenderPass();
	void CreateSwapChainFrameBuffer();

	VkSurfaceFormatKHR PickSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
	VkPresentModeKHR PickPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
	VkExtent2D PickExtent(const VkSurfaceCapabilitiesKHR& capabilities);
};

