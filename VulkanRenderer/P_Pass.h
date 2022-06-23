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

	void CreateDescriptorPool(const std::vector<VkDescriptorPoolSize>& poolSizes);
	void CreateDescriptorLayout(const std::vector<VkDescriptorSetLayoutBinding>& setLayoutBindings);
	void CreateSkyDescriptorLayout(const std::vector<VkDescriptorSetLayoutBinding>& setLayoutBindings);
	void CreateDescriptorSet();
	void CreateSkyDescriptorSet();

	void CreateFrameData();
	void CreatePipelineData();

	void UpdateDescriptorSet(const std::vector<VkWriteDescriptorSet>& writeDescSets);//TODO: Why this function contained Pass? Update in the demo.
	void UpdateSkyDescriptorSet(const std::vector<VkWriteDescriptorSet>& writeDescSets);
private:
	void CreateRenderPass();
	void CreateFrameBuffer();

	void CreatePipelineLayout();
	void CreatePipeline();

	void CreateSkyPipelineLayout();
	void CreateSkyPipeline();

public:
	uint32_t mWidth, mHeight;

	VkFramebuffer mFrameBuffer;
	VkRenderPass mRenderPass;

	FrameBufferAttachment* mLColorResult;
	FrameBufferAttachment* mGDepthResult;

	VkDescriptorPool mDescriptorPool;

	//Pipeline for draw Debug Normal
	VkDescriptorSetLayout mDescriptorLayout;
	VkDescriptorSet mDescriptorSet;

	VkPipelineLayout mPipelineLayout;
	VkPipeline mPipeline;

	//Pipeline for draw skybox
	VkDescriptorSetLayout mSkyDescriptorLayout;
	VkDescriptorSet mSkyDescriptorSet;

	VkPipelineLayout mSkyPipelineLayout;
	VkPipeline mSkyPipeline;
};

