/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#include "VulkanViewport.h"

FVulkanBackBuffer::FVulkanBackBuffer(VkImage InImage, const FRHITextureCreateInfo& InCreateInfo, FVulkanViewport* InViewport)
	: FVulkanTexture(InImage, InCreateInfo),
	OwnerViewport(InViewport)
{
	// Vulkan back buffer holds a image from swapchain and we don't own it
	OwnerShip = EOwnerShip::None;
}

FVulkanBackBuffer::~FVulkanBackBuffer()
{
	Image = VK_NULL_HANDLE;
}

FVulkanViewport::~FVulkanViewport()
{
	SwapChain.Reset();
}

void FVulkanViewport::Resize(const FUIntVec2& NewDimensions)
{
	// Reset back buffer
	IntermediateBackBuffer.Reset();
	RHIBackBuffer.Reset();

	// Recreate swapchain
	if (ShouldUseStandardSwapChain())
	{
		HLVM_ASSERT(SwapChain != nullptr);
		FVulkanSwapChain::FRecreateInfo ReCreateInfo;
		SwapChain->DestroySwapChain(&ReCreateInfo);
		SwapChain->OwnerViewport = nullptr; // Release ownership to prevent swapchain calling destroy twice
		SwapChain.Reset();

		CreateInfo.Dimensions = NewDimensions;
		CreateSwapChain(ReCreateInfo);

		HLVM_ASSERT(SwapChain->swapChainExtent.width == NewDimensions.x && SwapChain->swapChainExtent.height == NewDimensions.y);
	}
}

void FVulkanViewport::CreateSwapChain(FVulkanSwapChain::FRecreateInfo& InCreateInfo)
{
	if (ShouldUseStandardSwapChain())
	{
		HLVM_ASSERT(SwapChain == nullptr);
		SwapChain = new FVulkanSwapChain(this, &InCreateInfo);
	}
}

bool FVulkanViewport::ShouldUseStandardSwapChain() const
{
	return !CreateInfo.bHeadlessRendering;
}

bool FVulkanViewport::AcquireNextImageIndex()
{
	if (SwapChain)
	{
		if (!!SwapChain->AcquireNextImageIndex(SwapChainImageIndex, ImageAcquireSemaphore)
			&& SwapChainImageIndex != TUINT32_MAX && ImageAcquireSemaphore)
		{
			return true;
		}
	}
	return false;
}
