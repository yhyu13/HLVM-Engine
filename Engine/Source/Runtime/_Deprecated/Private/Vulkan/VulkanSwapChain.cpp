/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#include "VulkanSwapChain.h"
#include "VulkanResourcePost.h"

FVulkanSwapChain::FVulkanSwapChain(FVulkanViewport* InOwnerViewport, FRecreateInfo* InCreateInfo)
	: OwnerViewport(InOwnerViewport)
{
	HLVM_ENSURE(OwnerViewport);
	CreateSwapChain(InCreateInfo);
	CreateImageViews();
}

FVulkanSwapChain::~FVulkanSwapChain()
{
	if (OwnerViewport)
	{
		DestroySwapChain(nullptr);
	}
}

void FVulkanSwapChain::DestroySwapChain(TNullablePtr<FRecreateInfo> OutCreateInfo)
{
	VkDevice   device = OwnerViewport->LogicalDevice->GetHandle();
	const bool bRecreate = OutCreateInfo && VULKAN_SWAPCHAIN_KEEP_OLD;
	if (bRecreate)
	{
		OutCreateInfo->OldSwapChain = swapChain;
		OutCreateInfo->Surface = surface;
	}

	// Release fence, release semaphre
#if VULKAN_SWAPCHAIN_USE_IMAGE_FENCE
	imageAcquiredFences.clear();
#endif
	imageAcquiredSemaphores.clear();

	// Destroy framebuffer
	for (auto framebuffer : swapChainFrameBuffers)
	{
		VulkanRHI::vkDestroyFramebuffer(device, framebuffer, VulkanRHI::VULKAN_CPU_ALLOCATOR);
	}
	swapChainFrameBuffers.clear();
	// Destroy image view
	for (auto imageView : swapChainImageViews)
	{
		VulkanRHI::vkDestroyImageView(device, imageView, VulkanRHI::VULKAN_CPU_ALLOCATOR);
	}
	swapChainImageViews.clear();
	swapChainImages.clear();

	// Destroy surface not created by swapchain? The surface is actually created by GLFW3 vulkan window class
	// TODO : create a manger class for surface and remove ref count by 1 here
	if (!bRecreate)
	{
		VulkanRHI::vkDestroySwapchainKHR(device, swapChain, VulkanRHI::VULKAN_CPU_ALLOCATOR);
		VulkanRHI::vkDestroySurfaceKHR(OwnerViewport->Instance, surface, VulkanRHI::VULKAN_CPU_ALLOCATOR);
	}

	// Release ownership to prevent swapchain calling destroy twice
	OwnerViewport = nullptr;
}

void FVulkanSwapChain::CreateSwapChain(TNoNullablePtr<FRecreateInfo> InCreateInfo)
{
	auto&	 physicalDevice = OwnerViewport->PhysicalDevice;
	VkDevice device = OwnerViewport->LogicalDevice->GetHandle();
	surface = InCreateInfo->Surface;

	FVulkanPhysicalDevice::SwapChainSupportDetails swapChainSupport = physicalDevice->QuerySwapChainSupport(surface, true);
	VkSurfaceFormatKHR							   surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.formats);
	VkPresentModeKHR							   presentMode = ChooseSwapPresentMode(swapChainSupport.presentModes);
	VkExtent2D									   extent = ChooseSwapExtent(swapChainSupport.capabilities);

	TUINT32 imageCount = swapChainSupport.capabilities.minImageCount + 1; // 交换链支持的最小图像个数+1数量类实现三倍缓存
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
	TUINT32									  queueFamilyIndices[] = { indices.graphicsFamily, indices.presentFamily };

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

	// TODO : 旋转屏幕 on android can be left for app to rotate the surface instead of leting vulkan driver do it
	// which is an optimization for memory bandwidth on mobile devices, checkout UE5 Vulkan pretransform
	createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // 忽略alpha通道
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;
	if (InCreateInfo->OldSwapChain)
	{
		createInfo.oldSwapchain = InCreateInfo->OldSwapChain;
	}
	VULKAN_ENSURE(VulkanRHI::vkCreateSwapchainKHR(device, &createInfo, VulkanRHI::VULKAN_CPU_ALLOCATOR, &swapChain));

	VulkanRHI::vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
	HLVM_ENSURE(imageCount > 0 && imageCount <= MAX_FRAMES_IN_FLIGHT);
	swapChainImages.resize(imageCount);
	VulkanRHI::vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

	swapChainActualImageCount = imageCount;
	swapChainImageFormat = surfaceFormat.format;
	swapChainExtent = extent;

	// init fence and semaphore
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
	if (capabilities.currentExtent.width != TUINT32_MAX
		&& capabilities.currentExtent.height != TUINT32_MAX)
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

void FVulkanSwapChain::CreateImageViews()
{
	VkDevice device = OwnerViewport->LogicalDevice->GetHandle();
	swapChainImageViews.resize(swapChainImages.size());
	// 遍历创建ImageView---图像视图，该图像可以作为纹理使用，但是作为渲染目标，还需要帧缓冲对象
	for (size_t i = 0; i < swapChainImages.size(); i++)
	{
		VkImageViewCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = swapChainImages[i];
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;			 // viewType和fromat成员变量用于指定图像数据的解释方式
		createInfo.format = swapChainImageFormat;				 // 一维纹理、二维纹理、三维纹理或者立方体贴图
		createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY; // 用于图像颜色通道映射，保持默认即可
		createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; // 用于指定图像的用途和图像哪一部分可以被访问
		createInfo.subresourceRange.baseMipLevel = 0;						// 此处图像用作渲染，没有细分级别，只存在一个图层
		createInfo.subresourceRange.levelCount = 1;							// 不是VR应用，可以保持默认
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;

		VULKAN_ENSURE(VulkanRHI::vkCreateImageView(device, &createInfo, VulkanRHI::VULKAN_CPU_ALLOCATOR, &swapChainImageViews[i]));
	}
}

FVulkanSwapChain::ESurfaceStatus FVulkanSwapChain::AcquireNextImageIndex(TUINT32& OutImageIndex, VkSemaphore& OutImageAvailableSemaphore)
{
	// acquired image index need to be reset to TUINT32_MAX
	HLVM_ASSERT(currentAcquiredImageIndex == TUINT32_MAX);
	// Reset outputs
	OutImageIndex = TUINT32_MAX;
	OutImageAvailableSemaphore = VK_NULL_HANDLE;

	VkDevice device = OwnerViewport->LogicalDevice->GetHandle();

	TUINT32 prevSyncObjectIndex = currentSyncObjectIndex;
	currentSyncObjectIndex += 1;
	currentSyncObjectIndex %= swapChainActualImageCount;
	VkSemaphore imageAcquiredSemaphore = imageAcquiredSemaphores[currentSyncObjectIndex]->GetHandle();
#if VULKAN_SWAPCHAIN_USE_IMAGE_FENCE
	VkFence imageAcquiredFence = imageAcquiredFences[currentSyncObjectIndex]->GetHandle();
	if (imageAcquiredFence != VK_NULL_HANDLE)
	{
		imageAcquiredFences[currentSyncObjectIndex]->Reset();
	}
#else
	VkFence imageAcquiredFence = VK_NULL_HANDLE;
#endif
	// Acquire image
	TUINT32	 ImageIndex = swapChainActualImageCount;
	VkResult Result = VK_SUCCESS;
	{
		// 循环等待获取图像
		while ((Result == VK_SUCCESS || Result == VK_SUBOPTIMAL_KHR)
			&& ImageIndex >= swapChainActualImageCount)
		{
			Result = VulkanRHI::vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAcquiredSemaphore, imageAcquiredFence, &ImageIndex);
		}
	}

	if (Result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		currentSyncObjectIndex = prevSyncObjectIndex;
		return ESurfaceStatus::OutOfDate;
	}
	else if (Result == VK_ERROR_SURFACE_LOST_KHR)
	{
		currentSyncObjectIndex = prevSyncObjectIndex;
		return ESurfaceStatus::SurfaceLost;
	}
	HLVM_ENSURE(Result == VK_SUCCESS || Result == VK_SUBOPTIMAL_KHR);

	currentAcquiredImageIndex = ImageIndex;
	OutImageIndex = currentAcquiredImageIndex;

	OutImageAvailableSemaphore = imageAcquiredSemaphore;

	// Wait for fence
	if (imageAcquiredFence != VK_NULL_HANDLE)
	{
		imageAcquiredFences[currentSyncObjectIndex]->Wait();
	}

	return ESurfaceStatus::OK;
}

FVulkanSwapChain::ESurfaceStatus FVulkanSwapChain::Present(VkQueue PresentQueue, VkSemaphore RenderingDoneSemaphore)
{
	HLVM_ASSERT(currentAcquiredImageIndex != TUINT32_MAX);

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	if (RenderingDoneSemaphore != VK_NULL_HANDLE)
	{
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &RenderingDoneSemaphore;
	}
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapChain;
	presentInfo.pImageIndices = &currentAcquiredImageIndex;

	VkResult Result = VulkanRHI::vkQueuePresentKHR(PresentQueue, &presentInfo);
	// Reset acquired image index
	currentAcquiredImageIndex = TUINT32_MAX;

	if (Result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		return ESurfaceStatus::OutOfDate;
	}
	else if (Result == VK_ERROR_SURFACE_LOST_KHR)
	{
		return ESurfaceStatus::SurfaceLost;
	}
	HLVM_ENSURE(Result == VK_SUCCESS || Result == VK_SUBOPTIMAL_KHR);

	return FVulkanSwapChain::ESurfaceStatus::OK;
}
