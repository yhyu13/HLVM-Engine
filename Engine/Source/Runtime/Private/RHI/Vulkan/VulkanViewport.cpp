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
	delete mSwapChain;
	CreateDesc.Dimensions = NewDimensions;
	mSwapChain = new FVulkanSwapChain(this, mSwapChainCreateInfo);
}

void FVulkanViewport::Present()
{

}
void FVulkanViewport::CreateSwapChain(FVulkanSwapChain::FRecreateInfo& InCreateInfo)
{
	mSwapChainCreateInfo = InCreateInfo;
	mSwapChain = new FVulkanSwapChain(this, InCreateInfo);
}
