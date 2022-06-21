#pragma once
#include "vulkan/vulkan.h"
#include "FrameBuffer.h"

class VkApp;
struct PostFrameBuffer
{
private:
	VkApp* app = nullptr;

public:
	int32_t width, height;
	VkFramebuffer framebuffer;
	VkRenderPass renderPass;

	FrameBufferAttachment* colorResult = nullptr;
	FrameBufferAttachment* depthResult = nullptr;

	void Init(VkApp* appPtr, FrameBufferAttachment* GDepthResult, FrameBufferAttachment* LColorResult);
	void CreateRenderPass();
	void CreateFrameBuffer();
};