#include "Shader.h"
#include "VulkanTools.h"
Shader::Shader(const std::string& path, VkShaderStageFlagBits stage, VkDevice device)
	:mStage(stage)
{
	mShaderCode = readFile(path.c_str());
	BuildShaderModule(device);
}

VkPipelineShaderStageCreateInfo Shader::GetShaderStageCreateInfo(VkDevice device)
{
	VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = mStage;
	vertShaderStageInfo.module = mShaderModule;
	vertShaderStageInfo.pName = "main";

	return vertShaderStageInfo;
}

void Shader::BuildShaderModule(VkDevice device)
{
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = mShaderCode.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(mShaderCode.data());

	if (vkCreateShaderModule(device, &createInfo, nullptr, &mShaderModule) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create shader module!");
	}
}