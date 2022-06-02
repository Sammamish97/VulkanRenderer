#pragma once
#include <vulkan/vulkan.hpp>
#include <string>

class Shader
{
public:
	Shader(const std::string& path, VkShaderStageFlagBits stage, VkDevice device);
	VkPipelineShaderStageCreateInfo GetShaderStageCreateInfo(VkDevice device);
	void BuildShaderModule(VkDevice device);
private:
	VkShaderModule mShaderModule;
	VkShaderStageFlagBits mStage;
	std::vector<char> mShaderCode;
};