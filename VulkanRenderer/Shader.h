#pragma once
#include <vulkan/vulkan.hpp>
#include <string>

class Shader
{
public:
	Shader(const std::string& path, VkShaderStageFlagBits stage);
	static void ReadShaderFile(const std::string& path);
private:
	VkShaderModule mShaderModule;
	VkShaderStageFlagBits mStage;
	std::vector<char> mShaderCode;
};

class ShaderProgram
{
public:

private:
	Shader mVertShader;
	Shader mFragShader;
};

