#pragma once
#include <vulkan/vulkan.h>
#include "Attachment.h"

class VkApp;
class P_Pass
{
private:
	VkApp* mApp = nullptr;
public:
	void Init(VkApp* app, uint32_t width, uint32_t height, 
		FrameBufferAttachment* pLResult, FrameBufferAttachment* pGDepth);
	void Update();

private:
	void CreateRenderPass();
	void CreateFrameBuffer();

	void CreateDescriptorPool();
	void CreateDescriptorLayout(const std::vector<VkDescriptorSetLayoutBinding>& setLayoutBindings);
	void CreateDescriptorSet();

	void UpdateDescriptorSet(const std::vector<VkWriteDescriptorSet>& writeDescSets);

	void CreatePipelineLayout();
	void CreatePipeline();

private:
	uint32_t mWidth, mHeight;

	VkFramebuffer mFrameBuffer;
	VkRenderPass mRenderPass;

	FrameBufferAttachment* mLColorResult;
	FrameBufferAttachment* mGDepthResult;

	VkDescriptorPool mDescriptorPool;
	VkDescriptorSetLayout mDescriptorLayout;
	VkDescriptorSet mDescriptorSet;

	VkPipelineLayout mPipelineLayout;
	VkPipeline mPipeline;

	/*VkShaderModule mVertexShader;
	VkShaderModule mGeometryShader;
	VkShaderModule mFragmentShader;*/
};

