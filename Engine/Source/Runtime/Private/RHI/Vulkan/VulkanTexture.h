/**
* Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "RHI/RHIResource.h"
#include "VulkanRHIResourcePre.h"

// Vulkan-specific RHI texture
class FVulkanTexture : public FRHITexture, public FVulkanResource
{
public:
	enum class EOwnerShip
	{
		None,
		LocalOwner,
		ExternalOwner,
		Alias
	};

public:
	FVulkanTexture(VkImage InImage, const FRHITextureCreateInfo& InCreateInfo)
		: Image(InImage)
	{
		CreateInfo = InCreateInfo;
	}

	// Returns the Vulkan image handle
	VkImage GetImage() const { return Image; }

protected:
	VkImage Image;
	EOwnerShip OwnerShip;
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
