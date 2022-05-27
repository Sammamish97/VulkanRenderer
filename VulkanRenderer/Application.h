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
#include "DirLight.h"

const uint32_t WIDTH = 1080;
const uint32_t HEIGHT = 720;

const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };
const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

#ifdef  NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif //  NDEBUG

struct UniformBufferObject
{
	glm::mat4 view;
	glm::mat4 proj;
};

struct UniformBufferLights
{
	DirLight dir_light[3];
};

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

class HelloTriangleApplication
{
public:
	void run();

private:
	void initWindow();

	static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

	static void mouseCallback(GLFWwindow* window, double xposIn, double yposIn);

	void initVulkan();

	void createTextureImage();

	void createDepthResources();

	VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags feature);

	VkFormat findDepthFormat();

	bool hasStencilComponent(VkFormat format);

	void createDescriptorPool();

	void createDescriptorSet();

	void createUniformBuffers();

	void createDescriptorSetLayout();

	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);

	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

	void cleanupSwapChain();

	void recreateSwapChain();

	void createSyncObjects();

	void createCommandBuffers();

	void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

	VkCommandBuffer beginSingleTimeCommands();

	void endSingleTimeCommands(VkCommandBuffer commandBuffer);

	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

	void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);

	void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

	void createFramebuffers();

	void createRenderPass();

	VkShaderModule createShaderModule(const std::vector<char>& code);

	void createGraphicsPipeline();

	void mainLoop();

	void drawFrame();

	void updateUniformBuffer(uint32_t currentImage);

	void cleanup();

	void createTextureImageView();

	void createTextureSampler();

	VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);

	void createImageViews();

	void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
		VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);

	void createSwapChain();

	void createSurface();

	bool isDeviceSuitable(VkPhysicalDevice device);

	bool checkDeviceExtensionSupport(VkPhysicalDevice device);
	
	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);

	VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);

	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

	void createDeviceModule();

	void pickPhysicalDevice();

	void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

	void setupDebugMessenger();

	void createInstance();

	void checkExtensions();

	bool checkValidationLayerSupport();

	/**/
	void loadMeshAndObjects();

	void createCamera();

	void createLight();

	void processInput();

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

	std::chrono::system_clock::time_point frameStart;
	std::chrono::system_clock::time_point frameEnd;
	float deltaTime;

private:
	bool framebufferResized = false;
	uint32_t currentFrame = 0;
	VkInstance instance;
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkDevice device;
	VkQueue graphicsQueue;
	VkQueue presentQueue;
	VkDebugUtilsMessengerEXT debugMessenger;
	VkSurfaceKHR surface;
	VkSwapchainKHR swapChain;
	VkRenderPass renderPass;
	VkPipelineLayout pipelineLayout;
	VkPipeline graphicsPipeline;
	VkCommandPool commandPool;

	VkDescriptorPool descriptorPool;
	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorSet descriptorSet;

	VkCommandBuffer commandBuffer;
	VkSemaphore imageAvailableSemaphore;
	VkSemaphore renderFinishedSemaphore;
	VkFence inFlightFence;

	std::vector<VkFramebuffer> swapChainFramebuffers;
	std::vector <VkImage> swapChainImages;
	std::vector <VkImageView> swapChainImageViews;
	
	VkImage textureImage;
	VkDeviceMemory textureImageMemory;
	VkImageView textureImageView;
	VkSampler textureSampler;

	VkImage depthImage;
	VkDeviceMemory depthImageMemory;
	VkImageView depthImageView;

	VkFormat swapChainImageFormat;
	VkExtent2D swapChainExtent;
	GLFWwindow* window = nullptr;
};