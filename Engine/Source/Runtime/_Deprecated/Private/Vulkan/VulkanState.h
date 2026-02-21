/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "Renderer/RHI/_Deprecated/RHIPipeline.h"
#include "VulkanResourcePre.h"

// Vulkan-specific RHI sampler state
class FVulkanSamplerState : public FRHISamplerState, public FVulkanResource
{
public:
	FVulkanSamplerState(const FRHISamplerStateCreateInfo& InCreateInfo);

	// Returns the Vulkan sampler handle
	VkSampler GetSampler() const { return Sampler; }

private:
	VkSampler				   Sampler;
};

class FVulkanRasterizerState : public FRHIRasterizerState, public FVulkanResource
{
public:
	FVulkanRasterizerState(const FRHIRasterizerStateCreateInfo& InCreateInfo);

	static void ResetCreateInfo(VkPipelineRasterizationStateCreateInfo& OutInfo)
	{
		VulkanRHI::ZeroVulkanStruct(&OutInfo, VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO);
		OutInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
		OutInfo.lineWidth = 1.0f;
	}

public:
	VkPipelineRasterizationStateCreateInfo RasterizerState;
};

class FVulkanDepthStencilState : public FRHIDepthStencilState, public FVulkanResource
{
public:
	FVulkanDepthStencilState(const FRHIDepthStencilStateCreateInfo& InCreateInfo);

	void SetupCreateInfo(const FGraphicsPSOCreateInfo& GfxPSOCreateInfo, VkPipelineDepthStencilStateCreateInfo& OutDepthStencilState);
};

class FVulkanBlendState : public FRHIBlendState, public FVulkanResource
{
public:
	FVulkanBlendState(const FRHIBlendStateCreateInfo& InCreateInfo);

	static void ResetCreateInfo(VkPipelineColorBlendStateCreateInfo& OutInfo)
	{
		VulkanRHI::ZeroVulkanStruct(&OutInfo, VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO);
	}
public:
	// array the pipeline state can point right to
	VkPipelineColorBlendAttachmentState BlendStates[RHI::MAX_RT_ATTACHMENTS];
};

using FVulkanSamplerStateRef = TRefCountPtr<FVulkanSamplerState>;
using FVulkanRasterizerStateRef = TRefCountPtr<FVulkanRasterizerState>;
using FVulkanDepthStencilStateRef = TRefCountPtr<FVulkanDepthStencilState>;
using FVulkanBlendStateRef = TRefCountPtr<FVulkanBlendState>;
