/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "RHI/RHIResource.h"
#include "VulkanRHIResourceDeclaration.h"
#include "VulkanSwapChain.h"
#include "VulkanTexture.h"

class FVulkanViewport;
class FVulkanBackBuffer : public FVulkanTexture
{
public:
	FVulkanBackBuffer(VkImage InImage, const FRHITextureCreateDesc& InCreateDesc, FVulkanViewport* /*InViewport*/)
		: FVulkanTexture(InImage, InCreateDesc)
	//, Viewport(InViewport)
	{
	}

	~FVulkanBackBuffer() override
	{
		// HLVM_ASSERT(Viewport);
	}

private:
	// FVulkanViewport* Viewport;
};

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
	void* GetSwapChain() const override { return mSwapChain.Get(); }

	// Resizes the viewport and swap chain
	virtual void Resize(const FUIntVec2& NewDimensions) override;

	// Presents the viewport (swaps the back buffer)
	virtual void Present() override;

	void CreateSwapChain(FVulkanSwapChain::FRecreateInfo& InCreateInfo);

private:
	FVulkanSwapChainRef mSwapChain;

	// TODO
	// TUINT32 currentFrameIndex = 0;
	// TUINT32 maxFramesInFlight = 2; // TODO: use console variable to control it, but modern driver suggest just implement double buffering
};

using FVulkanViewportRef = TRefCountPtr<FVulkanViewport>;
