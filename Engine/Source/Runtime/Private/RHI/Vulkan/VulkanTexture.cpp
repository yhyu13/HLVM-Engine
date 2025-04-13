/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#include "VulkanTexture.h"
#include "RHI/Vulkan/IVulkanDynamicRHI.h"

FVulkanTexture::FVulkanTexture(const FRHITextureCreateInfo& InCreateInfo)
	: FRHITexture(InCreateInfo)
{
	Image = GetDynamicRHI<IVulkanDynamicRHI>()->CreateVulkanImage(CreateInfo);
	OwnerShip = EOwnerShip::Owner;
	// Log
	HLVM_LOG(LogVulkanRHI, trace, TXT("Create Texture {} {}"), *GetName(), HLVM_ENUM_TO_TCHAR(OwnerShip));
}

FVulkanTexture::FVulkanTexture(VkImage InImage, const FRHITextureCreateInfo& InCreateInfo)
	: FRHITexture(InCreateInfo)
{
	Image = InImage;
	OwnerShip = EOwnerShip::Owner;
	HLVM_LOG(LogVulkanRHI, trace, TXT("Create Texture {} {}"), *GetName(), HLVM_ENUM_TO_TCHAR(OwnerShip));
}

FVulkanTexture::~FVulkanTexture()
{
	if (OwnerShip == EOwnerShip::Owner)
	{
		GetDynamicRHI<IVulkanDynamicRHI>()->DestroyVulkanImage(Image);
		HLVM_LOG(LogVulkanRHI, trace, TXT("Destroy Texture %s"), *GetName());
	}
	else
	{
		HLVM_LOG(LogVulkanRHI, trace, TXT("No Destroy Texture %s since not owned"), *GetName());
	}
}
