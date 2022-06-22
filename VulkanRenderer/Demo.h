#pragma once
#include "VkApp.h"
#include "Mesh.h"
#include "Camera.h"
#include "Object.h"
#include "UniformStructure.h"
#include "FrameBuffer.h"
#include "ImageWrap.h"
#include "GFrameBuffer.h"
#include "LFrameBuffer.h"
#include "PostFrameBuffer.h"

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
	void LoadTextures();
	void CreateLight();
	void CreateCamera();
	void CreateSyncObjects();
	void LoadCubemap();

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
	ImageWrap testImage;

//FrameBuffer & Render related
	Buffer textureUBO;
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

