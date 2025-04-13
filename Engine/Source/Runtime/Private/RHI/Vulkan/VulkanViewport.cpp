/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#include "VulkanViewport.h"

FVulkanBackBuffer::FVulkanBackBuffer(VkImage InImage, const FRHITextureCreateInfo& InCreateInfo, FVulkanViewport* InViewport)
	: FVulkanTexture(InImage, InCreateInfo), OwnerViewport(InViewport)
{
	// Vulkan back buffer holds a image from swapchain and we don't own it
	OwnerShip = EOwnerShip::Alias;
	HLVM_LOG(LogVulkanRHI, trace, TXT("Create FVulkanBackBuffer {}"), *GetName());
}

FVulkanBackBuffer::~FVulkanBackBuffer()
{
	// Just release handle, no need to destroy it since it is owned by swapchain
	Image = VK_NULL_HANDLE;
	HLVM_LOG(LogVulkanRHI, trace, TXT("Destroy FVulkanBackBuffer {}"), *GetName());
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

		// Create back buffer
		HLVM_ASSERT(RHIBackBuffer == nullptr);
		FRHITextureCreateInfo BackBufferCreateInfo;
		BackBufferCreateInfo.Dimensions.x = SwapChain->swapChainExtent.width;
		BackBufferCreateInfo.Dimensions.y = SwapChain->swapChainExtent.height;
		BackBufferCreateInfo.Format = VulkanRHI::RHIFormatFromVulkanFormat(SwapChain->swapChainImageFormat);
		BackBufferCreateInfo.Flags |= ETextureCreateFlag::RenderTarget; // TODO back buffer support mass resolve?
		RHIBackBuffer = new FVulkanBackBuffer(VK_NULL_HANDLE, BackBufferCreateInfo, this);
	}
	else
	{
		HLVM_LOG(LogVulkanRHI, warn, TXT("CreateSwapChain: Headless rendering does not support standard swap chain"));
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
			&& SwapChainImageIndex != TUINT32_MAX && ImageAcquireSemaphore != VK_NULL_HANDLE)
		{
			return true;
		}
		else
		{
			HLVM_LOG(LogVulkanRHI, warn, TXT("AcquireNextImageIndex: Failed to acquire next image index"));
		}
	}
	else
	{
		HLVM_LOG(LogVulkanRHI, warn, TXT("AcquireNextImageIndex: SwapChain is not created"));
	}
	return false;
}

FRHITextureRef FVulkanViewport::GetBackBuffer() const
{
	HLVM_ASSERT(RHIBackBuffer != nullptr);
	return RHIBackBuffer;
}

void FVulkanViewport::BeginFrame()
{
	HLVM_ASSERT(RHIBackBuffer != nullptr);
	HLVM_ENSURE(AcquireNextImageIndex());
	RHIBackBuffer->UpdateImage(SwapChain->swapChainImages[SwapChainImageIndex]);
}

void FVulkanViewport::Present()
{
	HLVM_ASSERT(RHIBackBuffer != nullptr);
	// TODO
}
