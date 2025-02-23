/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#include "RHI/Vulkan/VulkanSwapChain.h"
#include "RHI/Vulkan/VulkanRHIResource.h"

FVulkanSwapChain::~FVulkanSwapChain()
{
	auto& device = OwnerViewport->Device;
	for (auto framebuffer : swapChainFramebuffers)
	{
		vkDestroyFramebuffer(device, framebuffer, nullptr);
	}
	for (auto imageView : swapChainImageViews)
	{
		vkDestroyImageView(device, imageView, VkCPUAllocator);
	}
	vkDestroySwapchainKHR(device, swapChain, VkCPUAllocator);
}
