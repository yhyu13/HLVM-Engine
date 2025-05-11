/**
* Copyright (c) 2025. MIT License. All rights reserved.
*/

#include "VulkanState.h"
#include "RHI/Vulkan/IVulkanDynamicRHI.h"

FVulkanSamplerState::FVulkanSamplerState(const FRHISamplerStateCreateInfo& InCreateInfo)
	: CreateInfo(InCreateInfo)
{
	Sampler = GetDynamicRHI<IVulkanDynamicRHI>()->CreateVulkanSampler(CreateInfo);
}

FVulkanBlendState::FVulkanBlendState(const FRHIBlendStateCreateInfo& InCreateInfo)
	: FRHIBlendState(InCreateInfo)
{
}
