/**
* Copyright (c) 2025. MIT License. All rights reserved.
*/

#include "VulkanShader.h"
#include "RHI/Vulkan/IVulkanDynamicRHI.h"

FVulkanShader::FVulkanShader(const FShaderCreateInfo& InCreateInfo)
	: FRHIShader(InCreateInfo)
{
	ShaderModule = GetDynamicRHI<IVulkanDynamicRHI>()->CreateVulkanShaderModule(InCreateInfo);
	PipelineShaderStageCreateInfo = GetDynamicRHI<IVulkanDynamicRHI>()->GenerateVkPipelineShaderStageCreateInfo(InCreateInfo, ShaderModule);
	HLVM_LOG(LogVulkanRHI, trace, TXT("Created Vulkan Shader Module: {}"), *GetName());
}

FVulkanShader::~FVulkanShader()
{
	GetDynamicRHI<IVulkanDynamicRHI>()->DestroyVulkanShaderModule(ShaderModule);
	HLVM_LOG(LogVulkanRHI, trace, TXT("Destroy Vulkan Shader Module: {}"), *GetName());
}
