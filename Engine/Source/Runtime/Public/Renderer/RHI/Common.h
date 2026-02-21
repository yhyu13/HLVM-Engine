/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once
#include "RHIDefinition.h"

#if USE_VK_BACKEND
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

namespace
{
	/*-----------------------------------------------------------------------------
	   Texture Dimension Types
	-----------------------------------------------------------------------------*/

	enum class ETextureDimension : TUINT8
	{
		Texture2D,
		Texture2DArray,
		Texture3D,
		TextureCube,
		TextureCubeArray,
	};

	/*-----------------------------------------------------------------------------
	   Texture Format
	-----------------------------------------------------------------------------*/

	enum class ETextureFormat : TUINT8
	{
		// Color formats
		R8,
		RG8,
		RGBA8,
		SRGBA8,

		// Depth formats
		D16,
		D24S8,
		D32,
		D32S8,

		// Compressed formats
		BC1,
		BC4,
		BC6H,
		BC7,

		// Float formats
		R16F,
		RG16F,
		RGBA16F,
		R32F,
		RGBA32F,
	};

	/*-----------------------------------------------------------------------------
	   Texture Filter Modes
	-----------------------------------------------------------------------------*/

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

	enum class ETextureAddress : TUINT8
	{
		Wrap,
		Mirror,
		Clamp,
		Border,
		MirrorOnce,
	};

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
} // namespace

namespace RHI
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
} // namespace RHI
