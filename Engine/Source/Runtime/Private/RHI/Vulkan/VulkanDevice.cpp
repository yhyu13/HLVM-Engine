/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#include "RHI/Vulkan/VulkanDevice.h"

FVulkanPhysicalDevice::QueueFamilyIndices FVulkanPhysicalDevice::QueryQueueFamilyIndices(VkSurfaceKHR Surface)
{
	if (mQueueFamilyIndices.IsComplete())
	{
		return mQueueFamilyIndices;
	}

	auto  device = Get();
	auto& indices = mQueueFamilyIndices;

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
	TVector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

	uint32_t index = 0;
	for (const auto& queueFamily : queueFamilies)
	{
		if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			indices.graphicsFamily = index;
		}
		if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)
		{
			indices.computeFamily = index;
		}
		if (queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT)
		{
			indices.transferFamily = index;
		}

		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, index, Surface, &presentSupport);
		if (presentSupport)
		{
			indices.presentFamily = index;
		}

		if (indices.IsComplete())
		{
			break;
		}
		index++;
	}

	return indices;
}

FVulkanPhysicalDevice::SwapChainSupportDetails FVulkanPhysicalDevice::QuerySwapChainSupport(VkSurfaceKHR Surface)
{
	if (mSwapChainSupportDetails.IsComplete())
	{
		return mSwapChainSupportDetails;
	}

	auto  device = Get();
	auto& surface = Surface;

	SwapChainSupportDetails details;

	// 与交换链相关的函数都需要device和surface这两个参数
	// 查询基础表面特性
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

	// 查询表面支持格式
	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
	if (formatCount != 0)
	{
		details.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
	}

	// 查询表面支持呈现模式
	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

	if (presentModeCount != 0)
	{
		details.presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
	}

	return details;
}
