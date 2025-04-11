/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#include "VulkanViewport.h"

FVulkanBackBuffer::FVulkanBackBuffer(VkImage InImage, const FRHITextureCreateInfo& InCreateInfo, FVulkanViewport* InViewport)
	: FVulkanTexture(InImage, InCreateInfo),
	OwnerViewport(InViewport)
{
	// Vulkan back buffer holds a image from swapchain and we don't own it
	OwnerShip = EOwnerShip::Alias;
}

FVulkanBackBuffer::~FVulkanBackBuffer()
{
	// Just release handle, no need to destroy it since it is owned by swapchain
	Image = VK_NULL_HANDLE;
}

void FVulkanBackBuffer::UpdateImage(VkImage InImage)
{
	Image = InImage;
}

FVulkanViewport::~FVulkanViewport()
{
	IntermediateBackBuffer.Reset();
	RHIBackBuffer.Reset();
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

		FRHITextureCreateInfo BackBufferCreateInfo;
		BackBufferCreateInfo.Dimensions.x = SwapChain->swapChainExtent.width;
		BackBufferCreateInfo.Dimensions.y = SwapChain->swapChainExtent.height;
		BackBufferCreateInfo.Format = VulkanRHI::RHIFormatFromVulkanFormat(SwapChain->swapChainImageFormat);
		BackBufferCreateInfo.Flags |= ETextureCreateFlag::RenderTarget; // TODO back buffer support mass resolve?
		RHIBackBuffer = new FVulkanBackBuffer(VK_NULL_HANDLE, BackBufferCreateInfo, this);
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
			//RHIBackBuffer->UpdateImage(SwapChain->swapChainImages[SwapChainImageIndex]);
			return true;
		}
	}
	return false;
}
