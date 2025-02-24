/**
* Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "RHI/RHIResource.h"
#include "VulkanRHIResourceDeclaration.h"

// Vulkan-specific RHI texture
class FVulkanTexture : public FRHITexture, public FVulkanResource
{
public:
	FVulkanTexture(VkImage InImage, const FRHITextureCreateDesc& InCreateDesc)
		: Image(InImage)
	{
		CreateDesc = InCreateDesc;
	}

	// Returns the dimensions of the texture
	virtual FIntVec3 GetSize() const override { return CreateDesc.Dimensions; }

	// Returns the pixel format of the texture
	virtual EPixelFormat GetFormat() const override { return CreateDesc.Format; }

	// Returns the texture flags
	virtual ETextureCreateFlags GetFlags() const override { return CreateDesc.Flags; }

	// Returns the Vulkan image handle
	VkImage GetImage() const { return Image; }

private:
	VkImage Image;
};



// Vulkan-specific RHI sampler state
class FVulkanSamplerState : public FRHISamplerState, public FVulkanResource
{
public:
	FVulkanSamplerState(VkSampler InSampler)
		: Sampler(InSampler) {}

	// Returns the Vulkan sampler handle
	VkSampler GetSampler() const { return Sampler; }

private:
	VkSampler Sampler;
};

// Vulkan-specific RHI render target view
class FVulkanRenderTargetView : public FRHIRenderTargetView, public FVulkanResource
{
public:
	FVulkanRenderTargetView(VkImageView InImageView)
		: ImageView(InImageView) {}

	// Returns the Vulkan image view handle
	VkImageView GetImageView() const { return ImageView; }

private:
	VkImageView ImageView;
};

using FVulkanTextureRef = TRefCountPtr<FVulkanTexture>;
using FVulkanSamplerStateRef = TRefCountPtr<FVulkanSamplerState>;
using FVulkanRenderTargetViewRef = TRefCountPtr<FVulkanRenderTargetView>;
