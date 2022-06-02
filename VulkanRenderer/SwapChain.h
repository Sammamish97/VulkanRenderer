#pragma once
#include <vulkan/vulkan.hpp>
#include "FrameBuffer.h"

class HelloTriangleApplication;

struct SwapChainSupportDetails
{
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};
struct SwapChainFrameBuffer
{
	SwapChainFrameBuffer() = default;
	SwapChainFrameBuffer(int32_t width, int32_t height)
		: mWidth(width), mHeight(height) {}
	VkFramebuffer mFramebuffer;
	
	FrameBufferAttachment mColorAttachment;
	int32_t mWidth, mHeight;
};

struct SwapChainPipeline
{
	VkPipeline mPipeline;
	VkRenderPass mRenderPass;
	VkPipelineLayout mPipelineLayout;

	VkShaderModule mVertexShaderModule;
	VkShaderModule mFragmentShaderModule;
};

struct SwapChainRenderData
{
	SwapChainRenderData(int32_t width, int32_t height)
		: mFrameBufferData(width, height){}
	SwapChainFrameBuffer mFrameBufferData;
	SwapChainPipeline mPipelineData;
};


class SwapChain
{
public:
	VkSwapchainKHR mSwapChain;

	VkExtent2D mSwapChainExtent;
	VkSurfaceFormatKHR mSwapChainFormat;
	uint32_t mImageCount = 0;
	
	std::vector<SwapChainRenderData> mSwapChainRenderDatas;

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
	void CreateSwapChainPipeline();
	void CreateSwapChainPipelineLayout();

	VkSurfaceFormatKHR PickSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
	VkPresentModeKHR PickPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
	VkExtent2D PickExtent(const VkSurfaceCapabilitiesKHR& capabilities);
};

