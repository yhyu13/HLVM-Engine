/**
* Copyright (c) 2025. MIT License. All rights reserved.
*/

#pragma once

#include "RHI/RHIResource.h"
#include "VulkanRHIResourcePre.h"

// Vulkan-specific RHI shader
class FVulkanShader : public FRHIShader, public FVulkanResource
{
public:
	FVulkanShader(VkShaderModule InShaderModule, const FShaderCreateInfo& InCreateInfo)
		: FRHIShader(InCreateInfo), ShaderModule(InShaderModule)
	{
	}

	// Returns the shader stage (e.g., vertex, pixel, compute)
	virtual EShaderStage GetStage() const override { return CreateInfo.Stage; }

	// Returns the Vulkan shader module handle
	VkShaderModule GetShaderModule() const { return ShaderModule; }

private:
	VkShaderModule ShaderModule;
};

// Vulkan-specific RHI shader resource view
class FVulkanShaderResourceView : public FRHIShaderResourceView, public FVulkanResource
{
public:
	FVulkanShaderResourceView(VkImageView InImageView, const FRHIShaderResourceViewCreateInfo& InCreateInfo)
		: FRHIShaderResourceView(InCreateInfo), ImageView(InImageView)
	{
	}

	// Returns the Vulkan image view handle
	VkImageView GetImageView() const { return ImageView; }

private:
	VkImageView ImageView;
};

using FVulkanShaderRef = TRefCountPtr<FVulkanShader>;
using FVulkanShaderResourceViewRef = TRefCountPtr<FVulkanShaderResourceView>;
