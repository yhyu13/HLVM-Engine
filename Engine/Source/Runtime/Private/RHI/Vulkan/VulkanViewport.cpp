/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#include "RHI/Vulkan/VulkanViewport.h"

FVulkanViewport::~FVulkanViewport()
{
	delete mSwapChain;
}

void FVulkanViewport::Resize(const FUIntVec2& NewDimensions)
{
	FVulkanSwapChain::FRecreateInfo InOutCreateInfo;
	mSwapChain->DestroySwapChain(&InOutCreateInfo);
	mSwapChain->OwnerViewport = nullptr; // Release ownership to prevent swapchain calling destroy twice
	delete mSwapChain;
	mSwapChain = nullptr;

	CreateDesc.Dimensions = NewDimensions;
	CreateSwapChain(InOutCreateInfo);
}

void FVulkanViewport::Present()
{
}
void FVulkanViewport::CreateSwapChain(FVulkanSwapChain::FRecreateInfo& InCreateInfo)
{
	HLVM_ASSERT(mSwapChain == nullptr);
	mSwapChain = new FVulkanSwapChain(this, &InCreateInfo);
}
