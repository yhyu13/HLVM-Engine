/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "VulkanMisc.h"
#include "VulkanDevice.h"
#include "VulkanSyncObject.h"

struct FVulkanMinimalContext
{
	explicit FVulkanMinimalContext(VkInstance InInstance,
		FVulkanPhysicalDeviceRef InPhysicalDevice,
		FVulkanLogicalDeviceRef InDevice)
		: Instance(InInstance)
		, PhysicalDevice(InPhysicalDevice)
		, LogicalDevice(InDevice)
	{
	}

	template<typename T>
	void Update(T& InResource)
	{
		InResource.Instance = Instance;
		InResource.PhysicalDevice = PhysicalDevice;
		InResource.LogicalDevice = LogicalDevice;
	}

	VkInstance			  Instance;
	FVulkanPhysicalDeviceRef PhysicalDevice;
	FVulkanLogicalDeviceRef  LogicalDevice;
};

// Base class for all RHI resources
class FVulkanResource : virtual public FRHIResource
{
public:
	FVulkanResource() = default;
	virtual ERHIInterfaceType GetInterfaceType() const override { return ERHIInterfaceType::Vulkan; }
};
