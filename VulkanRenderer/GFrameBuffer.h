#pragma once
#include<vulkan/vulkan.h>
#include "Attachment.h"
class VkApp;
struct GFrameBuffer
{
private:
	VkApp* app = nullptr;
public:
	int32_t width, height;
	VkFramebuffer framebuffer;
	FrameBufferAttachment position, normal, albedo;
	FrameBufferAttachment depth;
	VkRenderPass renderPass;

	void Init(VkApp* appPtr);
	void CreateRenderPass();
	void CreateFrameBuffer();
};
