/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#include "RHI/Vulkan/VulkanViewport.h"

FVulkanViewport::~FVulkanViewport()
{
	mSwapChain.Reset();
}

void FVulkanViewport::Resize(const FUIntVec2& NewDimensions)
{
	FVulkanSwapChain::FRecreateInfo ReCreateInfo;
	mSwapChain->DestroySwapChain(&ReCreateInfo);
	mSwapChain->OwnerViewport = nullptr; // Release ownership to prevent swapchain calling destroy twice
	mSwapChain.Reset();

	CreateDesc.Dimensions = NewDimensions;
	CreateSwapChain(ReCreateInfo);
}

void FVulkanViewport::Present()
{
}

void FVulkanViewport::CreateSwapChain(FVulkanSwapChain::FRecreateInfo& InCreateInfo)
{
	HLVM_ASSERT(mSwapChain == nullptr);
	mSwapChain = new FVulkanSwapChain(this, &InCreateInfo);
}
