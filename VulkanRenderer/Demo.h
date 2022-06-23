#pragma once
#include "VkApp.h"
#include "Mesh.h"
#include "Camera.h"
#include "Object.h"
#include "UniformStructure.h"
#include "ImageWrap.h"
#include "G_Pass.h"
#include "L_Pass.h"
#include "P_Pass.h"

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
	void LoadCubemap(std::string filename, VkFormat format, bool forceLinearTiling);

	void InitDescriptorPool();
	void InitDescriptorLayout();
	void InitDescriptorSet();

	void UpdateUniformBuffer();
	void UpdateDescriptorSet();

	void CreateSampler();

	void CreateUniformBuffers();

	void CreateCommandBuffers();

	void BuildLightCommandBuffer();
	void BuildGCommandBuffer();
	void BuildPostCommandBuffer(int swapChianIndex);

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
	ImageWrap testCubemap;

//FrameBuffer & Render related
	Buffer textureUBO;
	Buffer matUBO;
	Buffer lightUBO;

	G_Pass geometry_pass;
	L_Pass lighting_pass;
	P_Pass post_pass;
	
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

