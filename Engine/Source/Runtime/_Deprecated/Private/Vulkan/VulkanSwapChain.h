/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "Renderer/RHI/_Deprecated/Vulkan/VulkanLoader.h"
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

	constexpr inline static int MAX_FRAMES_IN_FLIGHT = 3; // 定义GPU同时渲染帧数

	enum class ESurfaceStatus
	{
		OK,
		OutOfDate,
		SurfaceLost
	};

	friend bool operator!(ESurfaceStatus Status)
	{
		return Status != ESurfaceStatus::OK;
	}

public:
	FVulkanSwapChain(FVulkanViewport* InOwnerViewport, FRecreateInfo* InCreateInfo);
	~FVulkanSwapChain();

private:
	void			   DestroySwapChain(TNullablePtr<FRecreateInfo> OutCreateInfo);
	void			   CreateSwapChain(TNoNullablePtr<FRecreateInfo> InCreateInfo);
	VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const TVector<VkSurfaceFormatKHR>& availableFormats);
	VkPresentModeKHR   ChooseSwapPresentMode(const TVector<VkPresentModeKHR>& availablePresentModes);
	VkExtent2D		   ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
	void			   CreateImageViews();

	ESurfaceStatus AcquireNextImageIndex(TUINT32& OutImageIndex, VkSemaphore& OutImageAvailableSemaphore);
	ESurfaceStatus Present(VkQueue PresentQueue, VkSemaphore RenderingDoneSemaphore);

private:
	friend class FVulkanViewport;
	TNullablePtr<FVulkanViewport>						  OwnerViewport;
	VkSurfaceKHR										  surface;
	VkSwapchainKHR										  swapChain;
	VkFormat											  swapChainImageFormat;
	VkExtent2D											  swapChainExtent;
	TStaticVector<VkImage, MAX_FRAMES_IN_FLIGHT>		  swapChainImages;		 // 交换链图像句柄
	TStaticVector<VkImageView, MAX_FRAMES_IN_FLIGHT>	  swapChainImageViews;	 // Vulkan对象，包括处于交换链，或者管线，都需要绑定一个VkImageView对象来访问它
	TStaticVector<VkFramebuffer, MAX_FRAMES_IN_FLIGHT> swapChainFrameBuffers; // 添加一个集合存储帧缓冲对象

#if VULKAN_SWAPCHAIN_USE_IMAGE_FENCE
	TStaticVector<FVulkanFenceRef, MAX_FRAMES_IN_FLIGHT> imageAcquiredFences; // 用于绘制的同步变量
#endif
	TStaticVector<FVulkanSemaphoreRef, MAX_FRAMES_IN_FLIGHT> imageAcquiredSemaphores; // 用于绘制的同步变量

	TUINT32 swapChainActualImageCount;
	TUINT32 currentSyncObjectIndex = 0;
	TUINT32	currentAcquiredImageIndex = TUINT32_MAX;
};

using FVulkanSwapChainRef = TRefCountPtr<FVulkanSwapChain>;
