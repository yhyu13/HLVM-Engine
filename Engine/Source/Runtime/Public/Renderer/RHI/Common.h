/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once
#include "RHIDefinition.h"

DECLARE_LOG_CATEGORY(LogRHI)

namespace RHI
{
	enum class EGpuVendorId : TUINT32
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

	// Get venderid from TUINT32

	HLVM_INLINE_FUNC EGpuVendorId VenderId2Enum(TUINT32 VenderId)
	{
		switch (S_C(EGpuVendorId, VenderId))
		{
			case EGpuVendorId::NotQueried:
				return EGpuVendorId::NotQueried;
			case EGpuVendorId::Amd:
			case EGpuVendorId::ImgTec:
			case EGpuVendorId::Nvidia:
			case EGpuVendorId::Arm:
			case EGpuVendorId::Broadcom:
			case EGpuVendorId::Qualcomm:
			case EGpuVendorId::Intel:
			case EGpuVendorId::Apple:
			case EGpuVendorId::Vivante:
			case EGpuVendorId::VeriSilicon:
			case EGpuVendorId::SamsungAMD:
			case EGpuVendorId::Microsoft:
			case EGpuVendorId::Kazan:
			case EGpuVendorId::Codeplay:
			case EGpuVendorId::Mesa:
				return S_C(EGpuVendorId, VenderId);
			case EGpuVendorId::Unknown:
			default:
				return EGpuVendorId::Unknown;
		}
	}
} // namespace RHI
