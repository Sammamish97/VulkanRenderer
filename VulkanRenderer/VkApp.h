#pragma once

#define GLFW_INCLUDE_VULKAN
#define GLM_FORCE_RADIANS
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <chrono>

#include <cstdint>
#include <limits>
#include <algorithm>
#include <fstream>

#include <vector>
#include <array>
#include <set>

#include "VulkanDevice.h"
#include "Mesh.h"
#include "Camera.h"
#include "Object.h"
#include "UniformStructure.h"
#include "FrameBuffer.h"
#include "SwapChain.h"
#include "Shader.h"
const uint32_t WIDTH = 1080;
const uint32_t HEIGHT = 720;

const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };
const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

#ifdef  NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif //  NDEBUG

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
	const VkAllocationCallbacks* pAllocator,
	VkDebugUtilsMessengerEXT* pDebugMessenger);

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);

static std::vector<char> readFile(const std::string& filename);

struct MouseInfo
{
	float lastX = 540;
	float lastY = 360;
	bool firstMouse = true;
};

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

class VkApp
{
	friend class SwapChain;

public:
	void run();

private:
	void initWindow();
	void initVulkan();
	void mainLoop();
	void cleanup();

private:
	SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice physicalDevice);
	void CreateSurface();
	void createSwapChain();
	void createGRenderPass();
	void createGFramebuffer();
	void createLRenderPass();
	void createLFramebuffer();
	void createSampler();
	void createUniformBuffers();
	void createDescriptorPool();
	void createDescriptorSetLayout();
	void createDescriptorSet();
	void createGPipeline();
	void createLPipeline();
	void createCommandBuffers();
	void buildGCommandBuffer();
	void buiilLCommandBuffer();
	void drawFrame();//TODO
	void updateUniformBuffer(uint32_t currentImage);

	//void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
	void createSyncObjects();

	VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags feature);
	VkFormat findDepthFormat();
	bool hasStencilComponent(VkFormat format);
	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
	void cleanupSwapChain();
	VkCommandBuffer beginSingleTimeCommands();
	void endSingleTimeCommands(VkCommandBuffer commandBuffer);
	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
	void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
	void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

	void setupDebugMessenger();
	static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
	static void mouseCallback(GLFWwindow* window, double xposIn, double yposIn);

	void createDeviceModule();
	void createDepthResources(VkImage depthImage, VkDeviceMemory memory);

	VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);

	void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
		VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);

	bool isDeviceSuitable(VkPhysicalDevice device);
	bool checkDeviceExtensionSupport(VkPhysicalDevice device);
	VkPhysicalDevice pickPhysicalDevice();
	void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
	void createInstance();
	void checkExtensions();
	bool checkValidationLayerSupport();

	/**/
	void loadMeshAndObjects();
	void createCamera();
	void createLight();
	void createShaders();

	void processInput();

	void createAttachment(VkFormat format, VkImageUsageFlagBits usage, FrameBufferAttachment* attachment);

	void PrepareGBuffer();

	void FrameStart();

	void Update();

	void FrameEnd();
	/**/

	std::vector<const char*> getRequiredExtensions();

	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData);

	//API
	VulkanDevice* vulkanDevice;
	SwapChain* mSwapChain;
	VkDescriptorPool descriptorPool;
	VkSurfaceKHR mSurface;

	Shader* mGVertShader;
	Shader* mGFragShader;

	Shader* mLVertShader;
	Shader* mLFragShader;
	
	GLFWwindow* window = nullptr;

	std::chrono::system_clock::time_point frameStart;
	std::chrono::system_clock::time_point frameEnd;
	float deltaTime;

	bool framebufferResized = false;
	uint32_t currentFrame = 0;

	VkInstance instance;
	VkQueue graphicsQueue;
	VkQueue presentQueue;

	MouseInfo mouseInfo;

	//Non-API
	Mesh* redMesh;
	Mesh* greenMesh;
	Mesh* BlueMesh;
	std::vector<Object*> objects;

	Camera* camera;

	UniformBufferLights lightsData;

	Buffer matUBO;
	Buffer lightUBO;

	GFrameBuffer mGFrameBuffer;
	LFrameBuffer mLFrameBuffer;

	VkPipeline GBufferPipeline;
	VkPipeline LPipeline;

	VkPipelineLayout GPipelineLayout;
	VkPipelineLayout LPipelineLayout;
	
	VkDescriptorSetLayout GDescriptorSetLayout;
	VkDescriptorSetLayout LightDescriptorSetLayout;

	VkDescriptorSet GBufferDescriptorSet;
	VkDescriptorSet LightingDescriptorSet;

	VkSampler colorSampler;

	VkDebugUtilsMessengerEXT debugMessenger;

	VkCommandBuffer GCommandBuffer;
	VkCommandBuffer LCommandBuffer;

	//Synchronize
	VkSemaphore GBufferComplete;
	VkSemaphore renderComplete;
	VkSemaphore presentComplete;
	VkFence inFlightFence;
};