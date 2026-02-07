/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "VulkanDevice.h"

class FVulkanFence : public FRefCountable
{
public:
	FVulkanFence(const FVulkanLogicalDeviceRef& InDevice, bool InSignaled);
	~FVulkanFence();

	VkFence GetHandle() const { return Fence; }

	void Reset();
	void Wait(uint64_t Timeout = UINT64_MAX);

private:
	FVulkanLogicalDeviceRef Device;
	VkFence					Fence;
};

class FVulkanSemaphore : public FRefCountable
{
public:
	FVulkanSemaphore(const FVulkanLogicalDeviceRef& InDevice);
	~FVulkanSemaphore();

	VkSemaphore GetHandle() const { return Semaphore; }

private:
	FVulkanLogicalDeviceRef Device;
	VkSemaphore				Semaphore;
};

class FVulkanEvent : public FRefCountable
{
public:
	FVulkanEvent(const FVulkanLogicalDeviceRef& InDevice);
	~FVulkanEvent();

	VkEvent GetHandle() const { return Event; }

	void Set();
	void Reset();

private:
	FVulkanLogicalDeviceRef Device;
	VkEvent Event;
};

using FVulkanFenceRef = TRefCountPtr<FVulkanFence>;
using FVulkanSemaphoreRef = TRefCountPtr<FVulkanSemaphore>;
using FVulkanEventRef = TRefCountPtr<FVulkanEvent>;
