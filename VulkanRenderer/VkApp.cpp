#include "VkApp.h"
#include "vulkan/vulkan.h"
#include "VulkanTools.h"
#include "VulkanInitializers.hpp"

#include <set>

void VkApp::Init()
{
	InitWindow();
	InitVulkan();
}

void VkApp::InitWindow()
{
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	mWindow = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
}

void VkApp::InitVulkan()
{
	CreateInstance();
	SetupDebugMessenger();
	CreateSurface();
	CreateDevice();
	CreateSwapChain();
}

void VkApp::Update()
{
	glfwPollEvents();
}

void VkApp::Draw()
{

}

void VkApp::CleanUp()
{
	
}

void VkApp::FrameStart()
{
	frameStart = std::chrono::system_clock::now();
}

void VkApp::FrameEnd()
{
	frameEnd = std::chrono::system_clock::now();
	deltaTime = std::chrono::duration<float>(frameEnd - frameStart).count();
}

/*************************************************************************************************************/
void VkApp::CreateInstance()
{
	if (enableValidationLayers && CheckValidationLayerSupport() == false)
	{
		throw std::runtime_error("validation layers requested, but not available!");
	}
	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Hello Triangle";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "No Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	auto extensions = GetRequiredExtensions();
	createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.data();

	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
	if (enableValidationLayers)
	{
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();

		PopulateDebugMessengerCreateInfo(debugCreateInfo);
		createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
	}
	else
	{
		createInfo.enabledLayerCount = 0;
		createInfo.pNext = nullptr;
	}

	if (vkCreateInstance(&createInfo, nullptr, &mInstance) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create instance!");
	}
}

void VkApp::CreateSurface()
{
	VK_CHECK_RESULT(glfwCreateWindowSurface(mInstance, mWindow, nullptr, &mSurface))
}

VkPhysicalDevice VkApp::PickPhysicalDevice()
{
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(mInstance, &deviceCount, nullptr);

	if (deviceCount == 0)
	{
		throw std::runtime_error("failed to find GPUs with Vulkan support!");
	}

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(mInstance, &deviceCount, devices.data());
	VkPhysicalDevice availableDevice{};
	for (const auto& device : devices)
	{
		if (IsDeviceSuitable(device))
		{
			availableDevice = device;
			break;
		}
	}

	if (availableDevice == VK_NULL_HANDLE)
	{
		throw std::runtime_error("failed to find suitable GPU!");
	}

	return availableDevice;
}

bool VkApp::IsDeviceSuitable(VkPhysicalDevice device)
{
	bool extensionsSupported = CheckDeviceExtensionSupport(device);

	bool swapChainAdequate = false;
	if (extensionsSupported)
	{
		SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(device);
		swapChainAdequate = (swapChainSupport.formats.empty() == false) && (swapChainSupport.presentModes.empty() == false);
	}
	return extensionsSupported && swapChainAdequate;
}

bool VkApp::CheckDeviceExtensionSupport(VkPhysicalDevice device)
{
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

	std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

	for (const auto& extension : availableExtensions)
	{
		requiredExtensions.erase(extension.extensionName);
	}
	return requiredExtensions.empty();
}

SwapChainSupportDetails VkApp::QuerySwapChainSupport(VkPhysicalDevice physicalDevice)
{
	SwapChainSupportDetails details;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, mSurface, &details.capabilities);

	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, mSurface, &formatCount, nullptr);

	if (formatCount != 0)
	{
		details.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, mSurface, &formatCount, details.formats.data());
	}

	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, mSurface, &presentModeCount, nullptr);

	if (presentModeCount != 0)
	{
		details.presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, mSurface, &presentModeCount, details.presentModes.data());
	}
	return details;
}

void VkApp::CreateDevice()
{
	mVulkanDevice = new VulkanDevice(PickPhysicalDevice());

	VkPhysicalDeviceFeatures deviceFeatures{};
	deviceFeatures.samplerAnisotropy = VK_TRUE;

	VkResult res = mVulkanDevice->createLogicalDevice(deviceFeatures, deviceExtensions, nullptr);
	if (res == VK_FALSE)
	{
		assert("Failed to create Logical Device!");
	}

	vkGetDeviceQueue(mVulkanDevice->logicalDevice, mVulkanDevice->getQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT), 0, &mGraphicsQueue);
	mPresentQueue = mGraphicsQueue;//TODO: PlaceHolder
}

void VkApp::CreateSwapChain()
{
	mSwapChain = new SwapChain(this);
}

/*************************************************************************************************************/

void VkApp::CreateAttachment(VkFormat format, VkImageUsageFlagBits usage, FrameBufferAttachment* attachment)
{
	VkImageAspectFlags aspectMask = 0;
	VkImageLayout imageLayout;

	attachment->format = format;

	if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
	{
		aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	}
	if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
	{
		aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	}

	assert(aspectMask > 0);

	VkImageCreateInfo image{};
	image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image.imageType = VK_IMAGE_TYPE_2D;
	image.format = format;
	image.extent.width = FB_DIM;
	image.extent.height = FB_DIM;
	image.extent.depth = 1;
	image.mipLevels = 1;
	image.arrayLayers = 1;
	image.samples = VK_SAMPLE_COUNT_1_BIT;
	image.tiling = VK_IMAGE_TILING_OPTIMAL;
	image.usage = usage | VK_IMAGE_USAGE_SAMPLED_BIT;

	VkMemoryAllocateInfo memAlloc{};
	memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;

	VkMemoryRequirements memReqs;

	VK_CHECK_RESULT(vkCreateImage(mVulkanDevice->logicalDevice, &image, nullptr, &attachment->image));
	vkGetImageMemoryRequirements(mVulkanDevice->logicalDevice, attachment->image, &memReqs);
	memAlloc.allocationSize = memReqs.size;
	memAlloc.memoryTypeIndex = mVulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(mVulkanDevice->logicalDevice, &memAlloc, nullptr, &attachment->memory));
	VK_CHECK_RESULT(vkBindImageMemory(mVulkanDevice->logicalDevice, attachment->image, attachment->memory, 0));

	VkImageViewCreateInfo imageView = initializers::imageViewCreateInfo();
	imageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageView.format = format;
	imageView.subresourceRange = {};
	imageView.subresourceRange.aspectMask = aspectMask;
	imageView.subresourceRange.baseMipLevel = 0;
	imageView.subresourceRange.levelCount = 1;
	imageView.subresourceRange.baseArrayLayer = 0;
	imageView.subresourceRange.layerCount = 1;
	imageView.image = attachment->image;
	VK_CHECK_RESULT(vkCreateImageView(mVulkanDevice->logicalDevice, &imageView, nullptr, &attachment->view));
}

VkFormat VkApp::FindDepthFormat()
{
	return FindSupportedFormat({ VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

VkFormat VkApp::FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling,
	VkFormatFeatureFlags features)
{
	for (VkFormat format : candidates)
	{
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(mVulkanDevice->physicalDevice, format, &props);

		if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
		{
			return format;
		}
		else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
		{
			return format;
		}
	}
	throw std::runtime_error("failed to find supported format!");
}

/*************************************************************************************************************/


void VkApp::SetupDebugMessenger()
{
	if (!enableValidationLayers)
		return;

	VkDebugUtilsMessengerCreateInfoEXT createInfo{};
	PopulateDebugMessengerCreateInfo(createInfo);

	if (CreateDebugUtilsMessengerEXT(mInstance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to set up debug messenger!");
	}
}

bool VkApp::CheckValidationLayerSupport()
{
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	for (const char* layerName : validationLayers)
	{
		bool layerFound = false;

		for (const auto& layerProperties : availableLayers)
		{
			if (strcmp(layerName, layerProperties.layerName) == false)
			{
				layerFound = true;
				break;
			}
		}

		if (layerFound == false)
		{
			return false;
		}
	}
	return true;
}

std::vector<const char*> VkApp::GetRequiredExtensions()
{
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

	if (enableValidationLayers)
	{
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}
	return extensions;
}

void VkApp::PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
{
	createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
		| VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
		| VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
		| VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

	createInfo.pfnUserCallback = DebugCallback;
}

VKAPI_ATTR VkBool32 VKAPI_CALL VkApp::DebugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData)
{
	std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
	return VK_FALSE;
}

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
	const VkAllocationCallbacks* pAllocator,
	VkDebugUtilsMessengerEXT* pDebugMessenger)
{
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr)
	{
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	}
	else
	{
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}
