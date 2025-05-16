/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#include "VulkanState.h"
#include "RHI/Vulkan/IVulkanDynamicRHI.h"

FVulkanSamplerState::FVulkanSamplerState(const FRHISamplerStateCreateInfo& InCreateInfo)
	: FRHISamplerState(InCreateInfo)
{
	Sampler = GetDynamicRHI<IVulkanDynamicRHI>()->CreateVulkanSampler(CreateInfo);
}

FVulkanBlendState::FVulkanBlendState(const FRHIBlendStateCreateInfo& InCreateInfo)
	: FRHIBlendState(InCreateInfo)
{
	FMemory::MemzeroArray(&BlendStates);
}

FVulkanRasterizerState::FVulkanRasterizerState(const FRHIRasterizerStateCreateInfo& InCreateInfo)
	: FRHIRasterizerState(InCreateInfo)
{
	ResetCreateInfo(RasterizerState);
}

FVulkanDepthStencilState::FVulkanDepthStencilState(const FRHIDepthStencilStateCreateInfo& InCreateInfo)
	: FRHIDepthStencilState(InCreateInfo)
{
}

void FVulkanDepthStencilState::SetupCreateInfo(const FGraphicsPSOCreateInfo& GfxPSOCreateInfo, VkPipelineDepthStencilStateCreateInfo& OutDepthStencilState)
{
	VulkanRHI::ZeroVulkanStruct(&OutDepthStencilState, VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO);

	OutDepthStencilState.depthTestEnable = (CreateInfo.DepthTest != ECompareFunction::Always || CreateInfo.bEnableDepthWrite) ? VK_TRUE : VK_FALSE;
	OutDepthStencilState.depthCompareOp = VulkanRHI::VulkanCompareOpFromRHI(CreateInfo.DepthTest);
	OutDepthStencilState.depthWriteEnable = CreateInfo.bEnableDepthWrite ? VK_TRUE : VK_FALSE;

	{
		// This will be filled in from the PSO
		OutDepthStencilState.depthBoundsTestEnable = GfxPSOCreateInfo.bDepthBounds;
		OutDepthStencilState.minDepthBounds = 0.0f;
		OutDepthStencilState.maxDepthBounds = 1.0f;
	}

	OutDepthStencilState.stencilTestEnable = (CreateInfo.bEnableFrontFaceStencil || CreateInfo.bEnableBackFaceStencil) ? VK_TRUE : VK_FALSE;

	// Front
	OutDepthStencilState.back.failOp = VulkanRHI::VulkanStencilOpFromRHI(CreateInfo.FrontFaceStencilFailStencilOp);
	OutDepthStencilState.back.passOp = VulkanRHI::VulkanStencilOpFromRHI(CreateInfo.FrontFacePassStencilOp);
	OutDepthStencilState.back.depthFailOp = VulkanRHI::VulkanStencilOpFromRHI(CreateInfo.FrontFaceDepthFailStencilOp);
	OutDepthStencilState.back.compareOp = VulkanRHI::VulkanCompareOpFromRHI(CreateInfo.FrontFaceStencilTest);
	OutDepthStencilState.back.compareMask = CreateInfo.StencilReadMask;
	OutDepthStencilState.back.writeMask = CreateInfo.StencilWriteMask;
	OutDepthStencilState.back.reference = 0;

	if (CreateInfo.bEnableBackFaceStencil)
	{
		// Back
		OutDepthStencilState.front.failOp = VulkanRHI::VulkanStencilOpFromRHI(CreateInfo.BackFaceStencilFailStencilOp);
		OutDepthStencilState.front.passOp = VulkanRHI::VulkanStencilOpFromRHI(CreateInfo.BackFacePassStencilOp);
		OutDepthStencilState.front.depthFailOp = VulkanRHI::VulkanStencilOpFromRHI(CreateInfo.BackFaceDepthFailStencilOp);
		OutDepthStencilState.front.compareOp = VulkanRHI::VulkanCompareOpFromRHI(CreateInfo.BackFaceStencilTest);
		OutDepthStencilState.front.compareMask = CreateInfo.StencilReadMask;
		OutDepthStencilState.front.writeMask = CreateInfo.StencilWriteMask;
		OutDepthStencilState.front.reference = 0;
	}
	else
	{
		OutDepthStencilState.front = OutDepthStencilState.back;
	}
}
