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
		//HLVM_ASSERT(Viewport);
	}

private:
	//FVulkanViewport* Viewport;
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
	void* GetSwapChain() const override { return mSwapChain; }

	// Resizes the viewport and swap chain
	virtual void Resize(const FUIntVec2& NewDimensions) override;

	// Presents the viewport (swaps the back buffer)
	virtual void Present() override;

	void CreateSwapChain(FVulkanSwapChain::FRecreateInfo& InCreateInfo);

private:
	FVulkanSwapChain* mSwapChain;

	TVector<VkImageView>   swapChainImageViews;	  // Vulkan对象，包括处于交换链，或者管线，都需要绑定一个VkImageView对象来访问它
	TVector<VkFramebuffer> swapChainFramebuffers; // 添加一个集合存储帧缓冲对象

	// TODO
	//TUINT32 currentFrameIndex = 0;
	//TUINT32 maxFramesInFlight = 2; // TODO: use console variable to control it, but modern driver suggest just implement double buffering
};

using FVulkanViewportRef = TRefCountPtr<FVulkanViewport>;
