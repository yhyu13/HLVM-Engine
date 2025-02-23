/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "VulkanMisc.h"
#include "VulkanSwapChain.h"

struct FVulkanMinimalContext
{
	FVulkanMinimalContext(VkInstance InInstance, VkDevice InDevice)
		: Instance(InInstance)
		, Device(InDevice)
	{
	}

	VkInstance Instance;
	VkDevice   Device;
};
