/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "VulkanLoader.h"
#include "VulkanSyncObject.h"

class FVulkanViewport;

class FVulkanSwapChain : public FRefCountable
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
		CreateImageViews();
	}
	~FVulkanSwapChain();

private:
	void			   DestroySwapChain(TNullablePtr<FRecreateInfo> OutCreateInfo);
	void			   CreateSwapChain(TNoNullPtr<FRecreateInfo> InCreateInfo);
	VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const TVector<VkSurfaceFormatKHR>& availableFormats);
	VkPresentModeKHR   ChooseSwapPresentMode(const TVector<VkPresentModeKHR>& availablePresentModes);
	VkExtent2D		   ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
	void			   CreateImageViews();

private:
	friend class FVulkanViewport;
	TNoNullPtr<FVulkanViewport> OwnerViewport;
	VkSurfaceKHR				surface;
	VkSwapchainKHR				swapChain;
	VkFormat					swapChainImageFormat;
	VkExtent2D					swapChainExtent;
	TVector<VkImage>			swapChainImages;	   // 交换链图像句柄
	TVector<VkImageView>		swapChainImageViews;   // Vulkan对象，包括处于交换链，或者管线，都需要绑定一个VkImageView对象来访问它
	TVector<VkFramebuffer>		swapChainFrameBuffers; // 添加一个集合存储帧缓冲对象

#if VULKAN_SWAPCHAIN_USE_IMAGE_FENCE
	TVector<FVulkanFenceRef> imageAcquiredFences; // 用于绘制的同步变量
#endif
	TVector<FVulkanSemaphoreRef> imageAcquiredSemaphores; // 用于绘制的同步变量
};

using FVulkanSwapChainRef = TRefCountPtr<FVulkanSwapChain>;
