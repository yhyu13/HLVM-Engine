/**
* Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "RHI/Vulkan/VulkanLoader.h"

class FVulkanPhysicalDevice : public FRefCountable
{
public:
	// 查询设备的可用队列族，-1代表无效
	struct QueueFamilyIndices
	{
		// 支持图像绘制的队列
		TUINT32 graphicsFamily = TUINT32_MAX;

		// 支持图像计算的队列
		TUINT32 computeFamily = TUINT32_MAX;

		// 支持图像传输的队列
		TUINT32 transferFamily = TUINT32_MAX;

		// 支持图形呈现的队列
		TUINT32 presentFamily = TUINT32_MAX;

		HLVM_INLINE_FUNC bool IsComplete()
		{
			return (graphicsFamily < TUINT32_MAX)
				&& (presentFamily < TUINT32_MAX)
				&& (computeFamily < TUINT32_MAX)
				&& (transferFamily < TUINT32_MAX);
		}
	};

	// 查询并记录交换链支持的细节
	struct SwapChainSupportDetails
	{
		VkSurfaceCapabilitiesKHR	capabilities; // 基础表面特性
		TVector<VkSurfaceFormatKHR> formats;	  // 像素格式、色彩空间
		TVector<VkPresentModeKHR>	presentModes; // 可用的呈现模式

		HLVM_INLINE_FUNC bool IsComplete()
		{
			return !formats.empty() && !presentModes.empty();
		}
	};

public:
	explicit FVulkanPhysicalDevice(VkPhysicalDevice InDevice)
	{
		mDevice = InDevice;
	}

	HLVM_INLINE_FUNC VkPhysicalDevice GetHandle() const
	{
		return mDevice;
	}

	operator VkPhysicalDevice()
	{
		return mDevice;
	}

	// 查询并返回队列族索引
	QueueFamilyIndices QueryQueueFamilyIndices(VkSurfaceKHR Surface, bool bFresh = false);

	// 查询交换链支持的详细信息
	SwapChainSupportDetails QuerySwapChainSupport(VkSurfaceKHR Surface, bool bFresh = false);

private:
	VkPhysicalDevice mDevice;
	TMap<VkSurfaceKHR, QueueFamilyIndices> mSurfaceToQueueFamilyIndices;
	TMap<VkSurfaceKHR, SwapChainSupportDetails> mSurfaceToSwapChainSupportDetails;
};

class FVulkanLogicalDevice : public FRefCountable
{
public:
	explicit FVulkanLogicalDevice(VkDevice InDevice)
	{
		mDevice = InDevice;
	}

	HLVM_INLINE_FUNC VkDevice GetHandle() const
	{
		return mDevice;
	}

	operator VkDevice()
	{
		return mDevice;
	}

	// TODO : 暂时不支持并行渲染
	bool SupportsParallelRendering() const
	{
		return false;
	}

private:
	VkDevice mDevice;
};

using FVulkanPhysicalDeviceRef = TRefCountPtr<FVulkanPhysicalDevice>;
using FVulkanLogicalDeviceRef = TRefCountPtr<FVulkanLogicalDevice>;
