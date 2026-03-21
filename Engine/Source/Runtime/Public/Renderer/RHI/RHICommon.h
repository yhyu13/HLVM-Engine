/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#pragma once
#include "RHIDefinition.h"

// Backend Must include before NVRHI
#if HLVM_RHI_VK_BACKEND
	#include "Renderer/RHI/Vulkan/VulkanDefinition.h"
#endif

#include <nvrhi/nvrhi.h>

#include "Core/Assert.h"
#include "Core/Log.h"

DECLARE_LOG_CATEGORY(LogRHI)

/*-----------------------------------------------------------------------------
   Forward Declarations
-----------------------------------------------------------------------------*/

class FDeviceManager;

/*-----------------------------------------------------------------------------
   Texture Dimension Types
-----------------------------------------------------------------------------*/

using ETextureDimension = nvrhi::TextureDimension;

/*-----------------------------------------------------------------------------
   Texture Format
-----------------------------------------------------------------------------*/

using ETextureFormat = nvrhi::Format;

/*-----------------------------------------------------------------------------
   Texture Filter Modes
-----------------------------------------------------------------------------*/

// NVRHI does not have an enum for texture filter modes
enum class ETextureFilter : TUINT8
{
	Nearest,
	Linear,
	NearestMipmapNearest,
	NearestMipmapLinear,
	LinearMipmapNearest,
	LinearMipmapLinear,
	Anisotropic,
};

/*-----------------------------------------------------------------------------
   Texture Address Modes
-----------------------------------------------------------------------------*/
using ETextureAddress = nvrhi::SamplerAddressMode;

enum class EGpuVendorID : TUINT32
{
	Unknown = 0xffffffff,
	NotQueried = 0,

	Amd = 0x1002,
	ImgTec = 0x1010,
	Nvidia = 0x10DE,
	Arm = 0x13B5,
	Broadcom = 0x14E4,
	Qualcomm = 0x5143,
	Intel = 0x8086,
	Apple = 0x106B,
	Vivante = 0x7a05,
	VeriSilicon = 0x1EB1,
	SamsungAMD = 0x144D,
	Microsoft = 0x1414,

	Kazan = 0x10003,	// VkVendorId
	Codeplay = 0x10004, // VkVendorId
	Mesa = 0x10005,		// VkVendorId
};

namespace hlvm_rhi
{
	// Triple buffering
	static constexpr TUINT32 MAX_FRAMES_IN_FLIGHT = 3;

	// Get venderid from TUINT32

	HLVM_INLINE_FUNC EGpuVendorID VenderId2Enum(TUINT32 VenderId)
	{
		switch (S_C(EGpuVendorID, VenderId))
		{
			case EGpuVendorID::NotQueried:
				return EGpuVendorID::NotQueried;
			case EGpuVendorID::Amd:
			case EGpuVendorID::ImgTec:
			case EGpuVendorID::Nvidia:
			case EGpuVendorID::Arm:
			case EGpuVendorID::Broadcom:
			case EGpuVendorID::Qualcomm:
			case EGpuVendorID::Intel:
			case EGpuVendorID::Apple:
			case EGpuVendorID::Vivante:
			case EGpuVendorID::VeriSilicon:
			case EGpuVendorID::SamsungAMD:
			case EGpuVendorID::Microsoft:
			case EGpuVendorID::Kazan:
			case EGpuVendorID::Codeplay:
			case EGpuVendorID::Mesa:
				return S_C(EGpuVendorID, VenderId);
			case EGpuVendorID::Unknown:
			default:
				return EGpuVendorID::Unknown;
		}
	}


	/*-----------------------------------------------------------------------------
	   Helper Functions
	-----------------------------------------------------------------------------*/

	HLVM_INLINE_FUNC nvrhi::Format ConvertTextureFormat(ETextureFormat Format)
	{
		return S_C(nvrhi::Format, Format);
	}

	HLVM_INLINE_FUNC nvrhi::TextureDimension ConvertTextureDimension(ETextureDimension Dimension)
	{
		return S_C(nvrhi::TextureDimension, Dimension);
	}
} // namespace RHI
