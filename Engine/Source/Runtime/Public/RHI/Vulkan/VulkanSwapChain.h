/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "VulkanLoader.h"
#include "VulkanSyncObject.h"

class FVulkanViewport;

class FVulkanSwapChain
{
public:
	struct FRecreateInfo
	{
		VkSwapchainKHR OldSwapChain;
		VkSurfaceKHR   Surface;
	};

public:
	FVulkanSwapChain(FVulkanViewport* InOwnerViewport, FRecreateInfo* InCreateInfo)
		: OwnerViewport(InOwnerViewport)
	{
		CreateSwapChain(InCreateInfo);
	}
	~FVulkanSwapChain();

private:
	void			   DestroySwapChain(TNullablePtr<FRecreateInfo> OutCreateInfo);
	void			   CreateSwapChain(TNoNullPtr<FRecreateInfo> InCreateInfo);
	VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const TVector<VkSurfaceFormatKHR>& availableFormats);
	VkPresentModeKHR   ChooseSwapPresentMode(const TVector<VkPresentModeKHR>& availablePresentModes);
	VkExtent2D		   ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

private:
	friend class FVulkanViewport;
	TNoNullPtr<FVulkanViewport> OwnerViewport;
	VkSurfaceKHR				surface;
	VkSwapchainKHR				swapChain;
	VkFormat					swapChainImageFormat;
	VkExtent2D					swapChainExtent;
	TVector<VkImage>			swapChainImages;		 // 交换链图像句柄
#if VULKAN_SWAPCHAIN_USE_IMAGE_FENCE
	TVector<FVulkanFenceRef>			imageAcquiredFences;	 // 用于绘制的同步变量
#endif
	TVector<FVulkanSemaphoreRef>		imageAcquiredSemaphores; // 用于绘制的同步变量
};
