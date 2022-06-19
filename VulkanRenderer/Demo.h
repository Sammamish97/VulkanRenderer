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

struct LFrameBuffer
{
	int32_t width, height;
	VkFramebuffer framebuffer;
	FrameBufferAttachment composition;
	VkRenderPass renderPass;
};

struct PostFrameBuffer
{
	int32_t width, height;
	VkFramebuffer framebuffer;
	FrameBufferAttachment composition;
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
//
	void CreateGRenderPass();
	void CreateGFramebuffer();

	void CreateLRenderPass();
	void CreateLFramebuffer();

	void CreatePostRenderPass();
	void CreatePostFrameBuffer();

	void CreateSampler();

	void CreateUniformBuffers();
	void CreateDescriptorPool();
	
	void CreateDescriptorSetLayout();
	void CreateDescriptorSet();

	void CreateGraphicsPipelines();

	void CreateCommandBuffers();

	void BuildLightCommandBuffer();
	void BuildGCommandBuffer();
	void BuildPostCommandBuffer(int swapChianIndex);

	void UpdateUniformBuffer(uint32_t currentImage);

//
	void CreateTextureImage();
	void CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
		VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);


private:
	VkDescriptorPool mImguiDescPool{ VK_NULL_HANDLE };

	void InitGUI();
	void DrawGUI();

private:
	void ProcessInput();
	static void MouseCallBack(GLFWwindow* window, double xposIn, double yposIn);

private:
	int totalVertices = 0;
	int totalFaces = 0;

private:
	MouseInfo mouseInfo;

	Mesh* redMesh;
	Mesh* greenMesh;
	Mesh* BlueMesh;
	std::vector<Object*> objects;

	Camera* camera;
	UniformBufferLights lightsData;
	VkDescriptorPool descriptorPool;
	VkSampler colorSampler;

//Texture
	VkImage textureImage;
	VkDeviceMemory textureImageMemory;

//FrameBuffer & Render related
	Buffer matUBO;
	Buffer lightUBO;

	GFrameBuffer mGFrameBuffer;
	LFrameBuffer mLFrameBuffer;
	PostFrameBuffer mPFrameBuffer;

	VkPipeline GBufferPipeline;
	VkPipeline LightingPipeline;
	VkPipeline PostPipeline;

	VkPipelineLayout GPipelineLayout;
	VkPipelineLayout LightPipelineLayout;
	VkPipelineLayout PostPipelineLayaout;

	VkDescriptorSetLayout GDescriptorSetLayout;
	VkDescriptorSetLayout LightDescriptorSetLayout;
	VkDescriptorSetLayout PostDescriptorSetLayout;

	VkDescriptorSet GBufferDescriptorSet;
	VkDescriptorSet LightingDescriptorSet;
	VkDescriptorSet PostDescriptorSet;

	VkCommandBuffer GCommandBuffer;
	VkCommandBuffer LightingCommandBuffer;
	VkCommandBuffer PostCommandBuffer;

//Synchronize
	VkSemaphore GBufferComplete;
	VkSemaphore renderComplete;
	VkSemaphore PostComplete;
	VkSemaphore presentComplete;
	VkFence inFlightFence;

//GUI
	bool DrawNormal = false;
	bool RotatingLight = false;
};

