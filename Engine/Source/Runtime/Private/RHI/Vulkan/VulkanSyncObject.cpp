/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#include "RHI/Vulkan/VulkanSyncObject.h"

FVulkanFence::FVulkanFence(FVulkanLogicalDeviceRef InDevice, bool InSignaled)
	: Device(InDevice)
{
	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = InSignaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;
	VULKAN_ENSURE(vkCreateFence(Device->Get(), &fenceCreateInfo, VULKAN_CPU_ALLOCATOR, &Fence));
}

FVulkanFence::~FVulkanFence()
{
	vkDestroyFence(Device->Get(), Fence, VULKAN_CPU_ALLOCATOR);
}

void FVulkanFence::Reset()
{
	vkResetFences(Device->Get(), 1, &Fence);
}

void FVulkanFence::Wait(uint64_t Timeout)
{
	vkWaitForFences(Device->Get(), 1, &Fence, VK_TRUE, Timeout);
}

FVulkanSemaphore::FVulkanSemaphore(FVulkanLogicalDeviceRef InDevice)
	: Device(InDevice)
{
	VkSemaphoreCreateInfo semaphoreCreateInfo = {};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	VULKAN_ENSURE(vkCreateSemaphore(Device->Get(), &semaphoreCreateInfo, VULKAN_CPU_ALLOCATOR, &Semaphore));
}

FVulkanSemaphore::~FVulkanSemaphore()
{
	vkDestroySemaphore(Device->Get(), Semaphore, VULKAN_CPU_ALLOCATOR);
}


FVulkanEvent::FVulkanEvent(FVulkanLogicalDeviceRef InDevice)
	: Device(InDevice)
{
	VkEventCreateInfo eventCreateInfo = {};
	eventCreateInfo.sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO;
	VULKAN_ENSURE(vkCreateEvent(Device->Get(), &eventCreateInfo, VULKAN_CPU_ALLOCATOR, &Event));
}

FVulkanEvent::~FVulkanEvent()
{
	vkDestroyEvent(Device->Get(), Event, VULKAN_CPU_ALLOCATOR);
}

void FVulkanEvent::Set()
{
	vkSetEvent(Device->Get(), Event);
}

void FVulkanEvent::Reset()
{
	vkResetEvent(Device->Get(), Event);
}
