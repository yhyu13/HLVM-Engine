/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "VulkanMisc.h"
#include "VulkanDevice.h"

struct FVulkanMinimalContext
{
	explicit FVulkanMinimalContext(VkInstance InInstance,
		FVulkanPhysicalDevice* InPhysicalDevice,
		FVulkanLogicalDevice* InDevice)
		: Instance(InInstance)
		, PhysicalDevice(InPhysicalDevice)
		, LogicalDevice(InDevice)
	{
	}

	VkInstance			  Instance;
	TNoNullPointer<FVulkanPhysicalDevice> PhysicalDevice;
	TNoNullPointer<FVulkanLogicalDevice>  LogicalDevice;
};

// Base class for all RHI resources
class FVulkanResource : virtual public FRHIResource
{
public:
	FVulkanResource() = default;
	virtual ERHIInterfaceType GetInterfaceType() const override { return ERHIInterfaceType::Vulkan; }
};
