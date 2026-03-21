/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#pragma once

#include "Renderer/RHI/_Deprecated/RHIResource.h"
#include "VulkanResource.h"

// Vulkan-specific RHI texture
class FVulkanTexture : public FRHITexture, public FVulkanResource
{
public:
	enum class EOwnerShip
	{
		Owner, // Owns the image
		Alias  // Alias of another image
	};

public:
	FVulkanTexture(const FRHITextureCreateInfo& InCreateInfo);
	FVulkanTexture(VkImage Image, const FRHITextureCreateInfo& InCreateInfo);

	~FVulkanTexture() override;

	// Returns the Vulkan image handle
	VkImage GetImage() const { return Image; }

	void SetOwnerShip(EOwnerShip InOwnerShip) { OwnerShip = InOwnerShip; }

	VkImageViewType GetImageViewType() const
	{
		return VulkanRHI::VulkanImageViewTypeFromRHIDimension(CreateInfo.Dimension);
	}

	TUINT32 GetVulkanArraySize() const;

	VkFormat GetViewFormat() const { return ViewFormat; }
	VkFormat GetStorageFormat() const { return StorageFormat; }

	VkImageAspectFlags GetFullAspectFlags() const { return FullAspectFlags; }
	VkImageAspectFlags GetPartialAspectFlags() const { return PartialAspectFlags; }
	FVulkanViewRef	   GetFullView() const { return FullView; }
	FVulkanViewRef	   GetPartialView() const { return PartialView; }

	VkImageUsageFlags GetImageUsageFlags() const { return ImageUsageFlags; }

protected:
	EOwnerShip			OwnerShip;
	IRHIHandle<VkImage> Image;

private:
	void PostInit();

private:
	VkFormat ViewFormat;
	VkFormat StorageFormat;

	VkImageAspectFlags FullAspectFlags;	   // depth + stencil
	VkImageAspectFlags PartialAspectFlags; // only depth
	FVulkanViewRef	   FullView;
	FVulkanViewRef	   PartialView;

	VkImageUsageFlags ImageUsageFlags;
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
using FVulkanRenderTargetViewRef = TRefCountPtr<FVulkanRenderTargetView>;
