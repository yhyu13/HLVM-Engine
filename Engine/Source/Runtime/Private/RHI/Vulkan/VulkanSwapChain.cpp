/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#include "RHI/Vulkan/VulkanSwapChain.h"
#include "RHI/Vulkan/VulkanRHIResource.h"

FVulkanSwapChain::~FVulkanSwapChain()
{
	if (OwnerViewport)
	{
		DestroySwapChain(nullptr);
	}
}

void FVulkanSwapChain::DestroySwapChain(TNullablePtr<FRecreateInfo> OutCreateInfo)
{
	VkDevice   device = OwnerViewport->LogicalDevice->Get();
	const bool bRecreate = OutCreateInfo && VULKAN_SWAPCHAIN_KEEP_OLD;
	if (bRecreate)
	{
		OutCreateInfo->OldSwapChain = swapChain;
		OutCreateInfo->Surface = surface;
	}
	else
	{
		vkDestroySwapchainKHR(device, swapChain, VULKAN_CPU_ALLOCATOR);
	}

	// Release fence, release semaphre
#if VULKAN_SWAPCHAIN_USE_IMAGE_FENCE
	imageAcquiredFences.clear();
#endif
	imageAcquiredSemaphores.clear();

	// Destory surface? TODO : create a manger class for surface and remove ref count by 1 here
	if (!bRecreate)
	{
		vkDestroySurfaceKHR(OwnerViewport->Instance, surface, VULKAN_CPU_ALLOCATOR);
	}
}

void FVulkanSwapChain::CreateSwapChain(TNoNullPtr<FRecreateInfo> InCreateInfo)
{
	auto&	 physicalDevice = OwnerViewport->PhysicalDevice;
	VkDevice device = OwnerViewport->LogicalDevice->Get();
	surface = InCreateInfo->Surface;

	FVulkanPhysicalDevice::SwapChainSupportDetails swapChainSupport = physicalDevice->QuerySwapChainSupport(surface);
	VkSurfaceFormatKHR							   surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.formats);
	VkPresentModeKHR							   presentMode = ChooseSwapPresentMode(swapChainSupport.presentModes);
	VkExtent2D									   extent = ChooseSwapExtent(swapChainSupport.capabilities);

	uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1; // 交换链支持的最小图像个数+1数量类实现三倍缓存
	if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
	{
		imageCount = swapChainSupport.capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = surface;

	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;							 // 用于指定每个图像所包含的层次。除了VR场景外，一般都是1.
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // 指定我们在图像上的操作，此处我们将图像作为颜色来使用

	FVulkanPhysicalDevice::QueueFamilyIndices indices = physicalDevice->QueryQueueFamilyIndices(surface);
	uint32_t								  queueFamilyIndices[] = { indices.graphicsFamily, indices.presentFamily };

	// 判断图形绘制队列和呈现队列是不是同一个队列
	if (indices.graphicsFamily != indices.presentFamily)
	{
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT; // 图像在同一时间可以被多个队列使用，协同模式
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else
	{
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // 如果相同，图像在同一时间只能被一个队列拥有，性能最佳
	}

	createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // 忽略alpha通道
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;
	if (InCreateInfo->OldSwapChain)
	{
		createInfo.oldSwapchain = InCreateInfo->OldSwapChain;
	}

	if (vkCreateSwapchainKHR(device, &createInfo, VULKAN_CPU_ALLOCATOR, &swapChain) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create swap chain!");
	}

	vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
	swapChainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

	swapChainImageFormat = surfaceFormat.format;
	swapChainExtent = extent;

	// init fense and semaphore
	// Release fence, release semaphre
#if VULKAN_SWAPCHAIN_USE_IMAGE_FENCE
	HLVM_ASSERT(imageAcquiredFences.size() == 0);
	imageAcquiredFences.resize(imageCount);
	for (auto& fence : imageAcquiredFences)
	{
		fence = new FVulkanFence(OwnerViewport->LogicalDevice, true);
	}
#endif
	HLVM_ASSERT(imageAcquiredSemaphores.size() == 0);
	imageAcquiredSemaphores.resize(imageCount);
	for (auto& semaphore : imageAcquiredSemaphores)
	{
		semaphore = new FVulkanSemaphore(OwnerViewport->LogicalDevice);
	}
}

// 选择合适的表面格式
// VkSurfaceFormatKHR包含了format和colorSpace，format指定颜色通道和存储类型，colorSpcae用来表示是否支持SRGB
VkSurfaceFormatKHR FVulkanSwapChain::ChooseSwapSurfaceFormat(const TVector<VkSurfaceFormatKHR>& availableFormats)
{
	for (const auto& availableFormat : availableFormats)
	{
		if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB
			&& availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			return availableFormat;
		}
	}
	return availableFormats[0];
}

// 选择合适的呈现模式
VkPresentModeKHR FVulkanSwapChain::ChooseSwapPresentMode(const TVector<VkPresentModeKHR>& availablePresentModes)
{
	VkPresentModeKHR bestMode = VK_PRESENT_MODE_FIFO_KHR;
	for (const auto& availablePresentMode : availablePresentModes)
	{
		if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
		{ // 该模式是三倍缓冲，效果最好，但是显卡可能不支持
			return availablePresentMode;
		}
		else if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR)
		{
			bestMode = availablePresentMode;
		}
	}
	return bestMode;
}

// 选择交换范围：交换链中的图像分辨率
VkExtent2D FVulkanSwapChain::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
{
	if (capabilities.currentExtent.width != TUINT32_MAX)
	{
		return capabilities.currentExtent;
	}
	else
	{
		VkExtent2D actualExtent;
		actualExtent.width = OwnerViewport->GetSize().x;
		actualExtent.height = OwnerViewport->GetSize().y;
		actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
		actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));
		return actualExtent;
	}
}
