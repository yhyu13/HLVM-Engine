/**
* Copyright (c) 2025. MIT License. All rights reserved.
*/

#include "VulkanTexture.h"
#include "RHI/Vulkan/IVulkanDynamicRHI.h"

FVulkanTexture::FVulkanTexture(const FRHITextureCreateInfo& InCreateInfo)
{
	CreateInfo = InCreateInfo;
	Image = GetDynamicRHI<IVulkanDynamicRHI>()->CreateVulkanImage(CreateInfo);
	OwnerShip = EOwnerShip::Owner;
}

FVulkanTexture::FVulkanTexture(VkImage InImage, const FRHITextureCreateInfo& InCreateInfo)
{
	CreateInfo = InCreateInfo;
	Image = InImage;
	OwnerShip = EOwnerShip::Owner;
}

FVulkanTexture::~FVulkanTexture()
{
	if (OwnerShip == EOwnerShip::Owner)
	{
		GetDynamicRHI<IVulkanDynamicRHI>()->DestroyVulkanImage(Image);
	}
}
