/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "RHI/Vulkan/VulkanLoader.h"

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
	FVulkanSwapChain(FVulkanViewport* InOwnerViewport, FRecreateInfo& InCreateInfo)
		: OwnerViewport(InOwnerViewport)
		//,Surface(InCreateInfo.Surface)
	{
		CreateSwapChain(InCreateInfo);
	}

	~FVulkanSwapChain();

private:
	void			   CreateSwapChain(FRecreateInfo& InCreateInfo);
	VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const TVector<VkSurfaceFormatKHR>& availableFormats);
	VkPresentModeKHR   ChooseSwapPresentMode(const TVector<VkPresentModeKHR>& availablePresentModes);
	VkExtent2D		   ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

private:
	friend FVulkanViewport;
	FVulkanViewport*	   OwnerViewport;
	//VkSurfaceKHR&		   Surface;
	VkSwapchainKHR		   swapChain;
	VkFormat			   swapChainImageFormat;
	VkExtent2D			   swapChainExtent;
	TVector<VkImage>	   swapChainImages;		  // 交换链图像句柄
	TVector<VkImageView>   swapChainImageViews;	  // Vulkan对象，包括处于交换链，或者管线，都需要绑定一个VkImageView对象来访问它
	TVector<VkFramebuffer> swapChainFramebuffers; // 添加一个集合存储帧缓冲对象
};
