/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "VulkanMisc.h"
#include "VulkanDevice.h"
#include "VulkanSyncObject.h"

struct FVulkanMinimalContext
{
	FVulkanMinimalContext();
	explicit FVulkanMinimalContext(VkInstance InInstance,
		FVulkanPhysicalDeviceRef			  InPhysicalDevice,
		FVulkanLogicalDeviceRef				  InDevice)
		: Instance(InInstance)
		, PhysicalDevice(InPhysicalDevice)
		, LogicalDevice(InDevice)
	{
	}

	VkInstance				 Instance;
	FVulkanPhysicalDeviceRef PhysicalDevice;
	FVulkanLogicalDeviceRef	 LogicalDevice;
};

// Base class for all RHI resources
class FVulkanResource : virtual public IRHIResource
{
public:
	FVulkanResource() = default;
	virtual ERHIInterfaceType GetInterfaceType() const override { return ERHIInterfaceType::Vulkan; }
};
