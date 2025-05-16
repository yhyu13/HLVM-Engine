/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#include "VulkanDevice.h"

FVulkanPhysicalDevice::FVulkanPhysicalDevice(VkPhysicalDevice InDevice)
	: mDevice(InDevice)
{
	// Init VkPhysicalDeviceProperties and so on
	{
		VkPhysicalDeviceProperties2 properties;
		VulkanRHI::ZeroVulkanStruct(&properties, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR);

		VulkanRHI::ZeroVulkanStruct(&mDeviceIDProperties, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES_KHR);
		VulkanRHI::ZeroVulkanStruct(&mSubgroupProperties, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES);
		properties.pNext = &mDeviceIDProperties;
		mDeviceIDProperties.pNext = &mSubgroupProperties;

		VulkanRHI::vkGetPhysicalDeviceProperties2(mDevice, &properties);
		mProperties = properties.properties;
	}

	mVendorId = RHI::GetVenderId(mProperties.vendorID);

	HLVM_LOG(LogVulkanRHI, debug, TXT("- DeviceName: {}"), TO_TCHAR_CSTR(mProperties.deviceName));
	HLVM_LOG(LogVulkanRHI, debug, TXT("- API={:d}.{:d}.{:d} (0x{:x}) Driver=0x{:x} VendorId=0x{:x}"),
		VK_VERSION_MAJOR(mProperties.apiVersion), VK_VERSION_MINOR(mProperties.apiVersion), VK_VERSION_PATCH(mProperties.apiVersion),
		mProperties.apiVersion, mProperties.driverVersion, mProperties.vendorID);
	HLVM_LOG(LogVulkanRHI, debug, TXT("- DeviceID=0x{:x} Type={:s}"), mProperties.deviceID, *VULKAN_TYPE_TO_FSTRING(VkPhysicalDeviceType, mProperties.deviceType));
	HLVM_LOG(LogVulkanRHI, debug, TXT("- Max Descriptor Sets Bound {:d}, Timestamps {:d}"), mProperties.limits.maxBoundDescriptorSets, mProperties.limits.timestampComputeAndGraphics);
}

FVulkanPhysicalDevice::QueueFamilyIndices FVulkanPhysicalDevice::QueryQueueFamilyIndices(VkSurfaceKHR Surface, bool bFresh)
{
	using namespace VulkanRHI;

	if (auto iter = mSurfaceToQueueFamilyIndices.find(Surface);
		!bFresh && iter != mSurfaceToQueueFamilyIndices.end() && iter->second.IsComplete())
	{
		return iter->second;
	}

	auto  device = this->GetHandle();
	auto& indices = mSurfaceToQueueFamilyIndices[Surface];

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

FVulkanPhysicalDevice::SwapChainSupportDetails FVulkanPhysicalDevice::QuerySwapChainSupport(VkSurfaceKHR Surface, bool bFresh)
{
	using namespace VulkanRHI;

	if (auto iter = mSurfaceToSwapChainSupportDetails.find(Surface);
		!bFresh && iter != mSurfaceToSwapChainSupportDetails.end() && iter->second.IsComplete())
	{
		return iter->second;
	}

	auto  device = this->GetHandle();
	auto& surface = Surface;
	auto& details = mSurfaceToSwapChainSupportDetails[Surface];

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
