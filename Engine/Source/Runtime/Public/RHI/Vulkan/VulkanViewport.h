/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "RHI/RHIResource.h"
#include "VulkanRHIResourceDeclaration.h"
#include "VulkanSwapChain.h"
#include "VulkanTexture.h"

class FVulkanViewport;
/**
 * @brief Back buffer that holds a specific index of swapchain images that are used to render to the screen.
 */
class FVulkanBackBuffer : public FVulkanTexture
{
public:
	FVulkanBackBuffer(VkImage InImage, const FRHITextureCreateDesc& InCreateDesc, FVulkanViewport* InViewport);
	~FVulkanBackBuffer() override;

private:
	TNoNullablePtr<FVulkanViewport> OwnerViewport;
};
using FVulkanBackBufferRef = TRefCountPtr<FVulkanSwapChain>;

/**
 * @brief Viewport that holds a swap chain and a back buffer. Be able to present the viewport
 * @details Viewport works like a wrapper of swap chain that interface with RHI command buffers,
 * 			because there exists cases where standard swap chain is not available (e.g. headless rendering).
 */
class FVulkanViewport : public FRHIViewport, public FVulkanResource, public FVulkanMinimalContext
{
public:
	FVulkanViewport(const FRHIViewportCreateDesc& InCreateDesc,
		const FVulkanMinimalContext&			  InContext)
		: FRHIViewport(InCreateDesc), FVulkanMinimalContext(InContext)
	{
	}

	~FVulkanViewport() override;

	// Returns the Vulkan swap chain handle
	void* GetSwapChain() const override { return SwapChain.Get(); }

	// Resizes the viewport and swap chain
	virtual void Resize(const FUIntVec2& NewDimensions) override;

//	// Presents the viewport (swaps the back buffer)
//	virtual void Present() override;

	// Creates the swap chain
	void CreateSwapChain(FVulkanSwapChain::FRecreateInfo& InCreateInfo);

	// Returns true if the swap chain should be created using standard Vulkan swap chain
	bool ShouldUseStandardSwapChain() const;

private:
	bool AcquireNextImageIndex();

private:
	FVulkanSwapChainRef SwapChain;
	FVulkanBackBufferRef RHIBackBuffer; // Normal back buffer to use
	FVulkanTextureRef IntermediateBackBuffer; // Back buffer that handle surface lost where RHI back buffer is not available

	TUINT32 SwapChainImageIndex = TUINT32_MAX;
	VkSemaphore ImageAcquireSemaphore = VK_NULL_HANDLE;
	TFixedSizeVector<FVulkanSemaphoreRef, FVulkanSwapChain::MAX_FRAMES_IN_FLIGHT> RenderingDoneSemaphores;
};

using FVulkanViewportRef = TRefCountPtr<FVulkanViewport>;
