/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#include "VulkanSyncObject.h"

FVulkanFence::FVulkanFence(const FVulkanLogicalDeviceRef& InDevice, bool InSignaled)
	: Device(InDevice)
{
	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = InSignaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;
	VULKAN_ENSURE(VulkanRHI::vkCreateFence(Device->GetHandle(), &fenceCreateInfo, VulkanRHI::VULKAN_CPU_ALLOCATOR, &Fence));
}

FVulkanFence::~FVulkanFence()
{
	VulkanRHI::vkDestroyFence(Device->GetHandle(), Fence, VulkanRHI::VULKAN_CPU_ALLOCATOR);
}

void FVulkanFence::Reset()
{
	VulkanRHI::vkResetFences(Device->GetHandle(), 1, &Fence);
}

void FVulkanFence::Wait(uint64_t Timeout)
{
	VulkanRHI::vkWaitForFences(Device->GetHandle(), 1, &Fence, VK_TRUE, Timeout);
}

FVulkanSemaphore::FVulkanSemaphore(const FVulkanLogicalDeviceRef& InDevice)
	: Device(InDevice)
{
	VkSemaphoreCreateInfo semaphoreCreateInfo = {};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	VULKAN_ENSURE(VulkanRHI::vkCreateSemaphore(Device->GetHandle(), &semaphoreCreateInfo, VulkanRHI::VULKAN_CPU_ALLOCATOR, &Semaphore));
}

FVulkanSemaphore::~FVulkanSemaphore()
{
	VulkanRHI::vkDestroySemaphore(Device->GetHandle(), Semaphore, VulkanRHI::VULKAN_CPU_ALLOCATOR);
}


FVulkanEvent::FVulkanEvent(const FVulkanLogicalDeviceRef& InDevice)
	: Device(InDevice)
{
	VkEventCreateInfo eventCreateInfo = {};
	eventCreateInfo.sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO;
	VULKAN_ENSURE(VulkanRHI::vkCreateEvent(Device->GetHandle(), &eventCreateInfo, VulkanRHI::VULKAN_CPU_ALLOCATOR, &Event));
}

FVulkanEvent::~FVulkanEvent()
{
	VulkanRHI::vkDestroyEvent(Device->GetHandle(), Event, VulkanRHI::VULKAN_CPU_ALLOCATOR);
}

void FVulkanEvent::Set()
{
	VulkanRHI::vkSetEvent(Device->GetHandle(), Event);
}

void FVulkanEvent::Reset()
{
	VulkanRHI::vkResetEvent(Device->GetHandle(), Event);
}
