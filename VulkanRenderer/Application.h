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

struct SwapChainSupportDetails
{
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

struct MouseInfo
{
	float lastX = 540;
	float lastY = 360;
	bool firstMouse = true;
};

struct FrameBufferAttachment
{
	VkImage image;
	VkDeviceMemory memory;
	VkImageView view;
	VkFormat format;
};

struct GFrameBuffer
{
	int32_t width, height;
	VkFramebuffer framebuffer;
	FrameBufferAttachment position, normal, albedo;
	FrameBufferAttachment depth;
	VkRenderPass renderPass;
};

struct SwapChainFrameBuffer
{
	SwapChainFrameBuffer() = default;
	SwapChainFrameBuffer(int32_t width, int32_t height)
		: mWidth(width), mHeight(height){}

	int32_t mWidth, mHeight;
	VkFramebuffer framebuffer;
	FrameBufferAttachment colorAttachment;
	FrameBufferAttachment depthAttachment;
	VkRenderPass renderPass;
};

class HelloTriangleApplication
{
public:
	void run();

private:
	void initWindow();
	void initVulkan();
	void mainLoop();
	void cleanup();

private:
	void createGRenderPass();
	void createGFramebuffer();
	void createSampler();
	void createUniformBuffers();
	void createDescriptorPool();
	void createDescriptorSetLayout();
	void createDescriptorSet();
	void createGraphicsPipelines();
	void createCommandBuffers();
	void buildLightCommandBuffer(int swapChianIndex);
	void buildGCommandBuffer();
	void drawFrame();
	void updateUniformBuffer(uint32_t currentImage);

	void createSwapChain();
	void createSwapChainImageViews();
	void createSwapChainRenderPass();
	void createSwapChainFrameBuffer();

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
	VkShaderModule createShaderModule(const std::vector<char>& code);

	void setupDebugMessenger();
	static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
	static void mouseCallback(GLFWwindow* window, double xposIn, double yposIn);
	void createSurface();
	void createDeviceModule();
	void createDepthResources(VkImage depthImage, VkDeviceMemory memory);

	VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);

	void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
		VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);

	bool isDeviceSuitable(VkPhysicalDevice device);
	bool checkDeviceExtensionSupport(VkPhysicalDevice device);
	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
	VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
	VkPhysicalDevice pickPhysicalDevice();
	void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
	void createInstance();
	void checkExtensions();
	bool checkValidationLayerSupport();

	/**/
	void loadMeshAndObjects();
	void createCamera();
	void createLight();

	void processInput();

	void createAttachment(VkFormat format, VkImageUsageFlagBits usage, FrameBufferAttachment* attachment);

	void PrepareGBuffer();

	VkPipelineShaderStageCreateInfo createShaderStageCreateInfo(const std::string& path, VkShaderStageFlagBits stage);

	void FrameStart();

	void BuildGCommandBuffers();

	void BuildLightingCommandsBuffers();

	void Update();

	void FrameEnd();
	/**/

	std::vector<const char*> getRequiredExtensions();

	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData);

public:
	VulkanDevice* vulkanDevice;

	Mesh* redMesh;
	Mesh* greenMesh;
	Mesh* BlueMesh;
	std::vector<Object*> objects;

	Camera* camera;
	MouseInfo mouseInfo;
	UniformBufferLights lightsData;

	Buffer matUBO;
	Buffer lightUBO;

	GFrameBuffer mGFrameBuffer;

	VkPipeline GBufferPipeline;
	VkPipeline LightingPipeline;

	VkPipelineLayout GPipelineLayout;
	VkPipelineLayout LightPipelineLayout;

	VkDescriptorPool descriptorPool;

	VkDescriptorSetLayout GDescriptorSetLayout;
	VkDescriptorSetLayout LightDescriptorSetLayout;

	VkDescriptorSet GBufferDescriptorSet;
	VkDescriptorSet LightingDescriptorSet;

	VkSampler colorSampler;

	std::chrono::system_clock::time_point frameStart;
	std::chrono::system_clock::time_point frameEnd;
	float deltaTime;

private:
	bool framebufferResized = false;
	uint32_t currentFrame = 0;

	VkInstance instance;
	VkQueue graphicsQueue;
	VkQueue presentQueue;

	VkDebugUtilsMessengerEXT debugMessenger;

	VkSurfaceKHR surface;

	VkCommandBuffer GCommandBuffer;
	VkCommandBuffer LightingCommandBuffer;

	//Synchronize
	VkSemaphore GBufferComplete;
	VkSemaphore renderComplete;
	VkSemaphore presentComplete;
	VkFence inFlightFence;
	
	//Swap chain
	VkSwapchainKHR swapChain;
	std::vector<SwapChainFrameBuffer> mSwapChainFrameBufers;
	VkRenderPass mGlobalSwapChainRenderPass;
	VkExtent2D swapChainExtent;

	GLFWwindow* window = nullptr;
};