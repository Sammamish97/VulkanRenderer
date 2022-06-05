#pragma once
#define GLFW_INCLUDE_VULKAN
#define GLM_FORCE_RADIANS
#include <GLFW/glfw3.h>
#include <cstdint>
#include <vector>
#include "VulkanDevice.h"

#include "SwapChain.h"
#include "VulkanDevice.h"
#include <chrono>

const uint32_t WIDTH = 1080;
const uint32_t HEIGHT = 720;

#define TEX_DIM 2048
#define TEX_FILTER VK_FILTER_LINEAR
#define FB_DIM TEX_DIM

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

class VkApp
{
	friend class SwapChain;
protected:
	virtual void Init();
	virtual void Update();
	virtual void Draw();
	virtual void CleanUp();

	virtual void FrameStart();
	virtual void FrameEnd();

private:
	void InitWindow();
	void InitVulkan();
	void CreateInstance();
	void CreateSurface();
	void CreateDevice();
	void CreateSwapChain();

	VkPhysicalDevice PickPhysicalDevice();
	bool IsDeviceSuitable(VkPhysicalDevice device);
	bool CheckDeviceExtensionSupport(VkPhysicalDevice device);
	SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice physicalDevice);

protected:
	void CreateAttachment(VkFormat format, VkImageUsageFlagBits usage, FrameBufferAttachment* attachment);
	VkFormat FindDepthFormat();
	VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

private:
	void SetupDebugMessenger();
	bool CheckValidationLayerSupport();
	std::vector<const char*> GetRequiredExtensions();
	void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData);

protected:
	VulkanDevice* mVulkanDevice;
	SwapChain* mSwapChain;
	VkSurfaceKHR mSurface;

	GLFWwindow* mWindow = nullptr;

	std::chrono::system_clock::time_point frameStart;
	std::chrono::system_clock::time_point frameEnd;
	float deltaTime;

	bool framebufferResized = false;
	uint32_t currentFrame = 0;

	VkInstance mInstance;
	VkQueue mGraphicsQueue;
	VkQueue mPresentQueue;

	VkDebugUtilsMessengerEXT debugMessenger;
};

