/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#pragma once

#include "VulkanDefinition.h"

namespace hlvm_vk
{
	HLVM_EXTERN_FUNC bool IsVulkanLoaderAPIInitialized();
	HLVM_EXTERN_FUNC void InitVulkanLoaderAPIOnce();

	HLVM_EXTERN_FUNC bool IsVulkanLoaderInstanceAPIInitialized();
	HLVM_EXTERN_FUNC void InitVulkanLoaderInstance(vk::Instance& instance);

	HLVM_EXTERN_FUNC bool IsVulkanLoaderDeviceAPIInitialized();
	HLVM_EXTERN_FUNC void InitVulkanLoaderDevice(vk::Device& device);
} // namespace hlvm_vk
