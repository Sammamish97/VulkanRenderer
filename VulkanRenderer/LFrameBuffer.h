#pragma once
#include "vulkan/vulkan.h"
#include "Attachment.h"

class VkApp;
struct LFrameBuffer
{
private:
	VkApp* app = nullptr;
public:
	int32_t width, height;
	VkFramebuffer framebuffer;
	FrameBufferAttachment composition;
	VkRenderPass renderPass;

	void Init(VkApp* appPtr);
	void CreateRenderPass();
	void CreateFrameBuffer();
};