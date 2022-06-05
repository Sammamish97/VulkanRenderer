#pragma once
#include "VkApp.h"
#include "Mesh.h"
#include "Camera.h"
#include "Object.h"
#include "UniformStructure.h"
#include "FrameBuffer.h"

struct GFrameBuffer
{
	int32_t width, height;
	VkFramebuffer framebuffer;
	FrameBufferAttachment position, normal, albedo;
	FrameBufferAttachment depth;
	VkRenderPass renderPass;
};

struct MouseInfo
{
	float lastX = 540;
	float lastY = 360;
	bool firstMouse = true;
};

class Demo : public VkApp
{
public:
	void run();
private:
	void Init() override;
	void Update() override;
	void Draw() override;
	void CleanUp() override;

private:
	void SetupCallBacks();

	void LoadMeshAndObjects();
	void CreateLight();
	void CreateCamera();
	void CreateSyncObjects();

	void CreateGRenderPass();
	void CreateGFramebuffer();

	void CreateUniformBuffers();
	void CreateDescriptorPool();
	void CreateSampler();
	void CreateDescriptorSetLayout();
	void CreateDescriptorSet();

	void CreateGraphicsPipelines();

	void CreateCommandBuffers();

	void BuildLightCommandBuffer(int swapChianIndex);
	void BuildGCommandBuffer();

	void UpdateUniformBuffer(uint32_t currentImage);

private:
	void ProcessInput();
	static void MouseCallBack(GLFWwindow* window, double xposIn, double yposIn);

private:
	MouseInfo mouseInfo;

	Mesh* redMesh;
	Mesh* greenMesh;
	Mesh* BlueMesh;
	std::vector<Object*> objects;

	Camera* camera;
	UniformBufferLights lightsData;

	Buffer matUBO;
	Buffer lightUBO;

	GFrameBuffer mGFrameBuffer;

	VkPipeline GBufferPipeline;
	VkPipeline LightingPipeline;

	VkPipelineLayout GPipelineLayout;
	VkPipelineLayout LightPipelineLayout;

	VkDescriptorSetLayout GDescriptorSetLayout;
	VkDescriptorSetLayout LightDescriptorSetLayout;

	VkDescriptorSet GBufferDescriptorSet;
	VkDescriptorSet LightingDescriptorSet;

	VkDescriptorPool descriptorPool;

	VkSampler colorSampler;

	VkCommandBuffer GCommandBuffer;
	VkCommandBuffer LightingCommandBuffer;

	//Synchronize
	VkSemaphore GBufferComplete;
	VkSemaphore renderComplete;
	VkSemaphore presentComplete;
	VkFence inFlightFence;
};

