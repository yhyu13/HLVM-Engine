/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#include "VulkanTexture.h"
#include "Renderer/RHI/_Deprecated/Vulkan/IVulkanDynamicRHI.h"

HLVM_STATIC_FUNC VkImageUsageFlags GetImageUsageFlags(const ETextureCreateFlags& TexFlags)
{
	VkImageUsageFlags UsageFlags = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

	if (EnumHasAnyFlags(TexFlags, ETextureCreateFlag::Present))
	{
		UsageFlags |= VK_IMAGE_USAGE_STORAGE_BIT;
	}
	else if (EnumHasAnyFlags(TexFlags, ETextureCreateFlag::RenderTarget | ETextureCreateFlag::DepthStencil))
	{
		if (EnumHasAllFlags(TexFlags, ETextureCreateFlag::InputAttachment))
		{
			UsageFlags |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
		}
		UsageFlags |= (EnumHasAnyFlags(TexFlags, ETextureCreateFlag::RenderTarget) ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT : VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
		if (EnumHasAllFlags(TexFlags, ETextureCreateFlag::MemoryLess)
			//&& InDevice.GetDeviceMemoryManager().SupportsMemoryless() TODO
		)
		{
			UsageFlags |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
			// Remove the transfer and sampled bits, as they are incompatible with the transient bit.
			UsageFlags &= static_cast<VkImageUsageFlags>(~(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT));
		}
	}
	else if (EnumHasAnyFlags(TexFlags, ETextureCreateFlag::DepthStencilResolve))
	{
		UsageFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	}
	else if (EnumHasAnyFlags(TexFlags, ETextureCreateFlag::RenderTargetResolve))
	{
		UsageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	}

	if (EnumHasAnyFlags(TexFlags, ETextureCreateFlag::UAV))
	{
		// cannot have the storage bit on a memoryless texture
		HLVM_ENSURE(!EnumHasAnyFlags(TexFlags, ETextureCreateFlag::MemoryLess));
		UsageFlags |= VK_IMAGE_USAGE_STORAGE_BIT;
	}

	return UsageFlags;
}

FVulkanTexture::FVulkanTexture(const FRHITextureCreateInfo& InCreateInfo)
	: FRHITexture(InCreateInfo), Image(this)
{
	Image = RHI::GetDynamicRHI<IVulkanDynamicRHI>()->CreateVulkanImage(CreateInfo);
	OwnerShip = EOwnerShip::Owner;

	PostInit();
	// Log
	HLVM_LOG(LogVulkanRHI, trace, TXT("Create Texture {} {}"), *ToString(), HLVM_ENUM_TO_TCHAR(OwnerShip));
}

FVulkanTexture::FVulkanTexture(VkImage InImage, const FRHITextureCreateInfo& InCreateInfo)
	: FRHITexture(InCreateInfo), Image(this)
{
	Image = InImage;
	OwnerShip = EOwnerShip::Owner;

	PostInit();
	HLVM_LOG(LogVulkanRHI, trace, TXT("Create Texture {} {}"), *this->ToString(), HLVM_ENUM_TO_TCHAR(OwnerShip));
}

FVulkanTexture::~FVulkanTexture()
{
	if (OwnerShip == EOwnerShip::Owner)
	{
		RHI::GetDynamicRHI<IVulkanDynamicRHI>()->DestroyVulkanImage(Image);
		HLVM_LOG(LogVulkanRHI, trace, TXT("Destroy Texture {}"), *this->ToString());
	}
	else
	{
		HLVM_LOG(LogVulkanRHI, trace, TXT("No Destroy Texture {} {}"), *this->ToString(), HLVM_ENUM_TO_TCHAR(OwnerShip));
	}
}

void FVulkanTexture::PostInit()
{
	ViewFormat = VulkanRHI::VulkanFormatFromRHIFormat(GetFormat(), GetCreateFlags() & ETextureCreateFlag::SRGB);
	StorageFormat = VulkanRHI::VulkanFormatTryRemoveSRGB(ViewFormat);

	FullAspectFlags = VulkanRHI::GetAspectMaskFromRHIFormat(GetFormat(), true, true);
	PartialAspectFlags = VulkanRHI::GetAspectMaskFromRHIFormat(GetFormat(), true, false);

	ImageUsageFlags = ::GetImageUsageFlags(GetCreateFlags());
}

TUINT32 FVulkanTexture::GetVulkanArraySize() const
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch-enum"
	switch (GetImageViewType())
	{
		case VK_IMAGE_VIEW_TYPE_1D:
		case VK_IMAGE_VIEW_TYPE_2D:
		case VK_IMAGE_VIEW_TYPE_3D:
			return 1;
		case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
			return GetArraySize();
		case VK_IMAGE_VIEW_TYPE_CUBE:
			return 6;
		case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:
			return 6 * GetArraySize();
		default:
			HLVM_ASSERT_F(false, TXT("Unknown image view type"));
			return 1;
	}
#pragma clang diagnostic pop
}
