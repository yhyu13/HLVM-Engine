/**
* Copyright (c) 2026. MIT License. All rights reserved.
*/

#include "VulkanShader.h"
#include "Renderer/RHI/_Deprecated/Vulkan/IVulkanDynamicRHI.h"

FVulkanShader::FVulkanShader(const FShaderCreateInfo& InCreateInfo)
	: FRHIShader(InCreateInfo), ShaderModule(this)
{
	ShaderModule = RHI::GetDynamicRHI<IVulkanDynamicRHI>()->CreateVulkanShaderModule(InCreateInfo);
	PipelineShaderStageCreateInfo = RHI::GetDynamicRHI<IVulkanDynamicRHI>()->GenerateVkPipelineShaderStageCreateInfo(InCreateInfo, ShaderModule);
	HLVM_LOG(LogVulkanRHI, trace, TXT("Created Vulkan Shader Module: {}"), *ToString());
}

FVulkanShader::~FVulkanShader()
{
	RHI::GetDynamicRHI<IVulkanDynamicRHI>()->DestroyVulkanShaderModule(ShaderModule);
	HLVM_LOG(LogVulkanRHI, trace, TXT("Destroy Vulkan Shader Module: {}"), *ToString());
}
