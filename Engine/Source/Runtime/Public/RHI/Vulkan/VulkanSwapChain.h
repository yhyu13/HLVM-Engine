/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "RHI/Vulkan/VulkanLoader.h"

class FVulkanViewport;

class FVulkanSwapChain
{
public:
	~FVulkanSwapChain();

private:
	friend FVulkanViewport;

	VkSwapchainKHR		   swapChain;
	TVector<VkImage>	   swapChainImages; // 交换链图像句柄
	//VkFormat			   swapChainImageFormat;
	//VkExtent2D			   swapChainExtent;
	TVector<VkImageView>   swapChainImageViews;	  // Vulkan对象，包括处于交换链，或者管线，都需要绑定一个VkImageView对象来访问它
	TVector<VkFramebuffer> swapChainFramebuffers; // 添加一个集合存储帧缓冲对象

	FVulkanViewport* OwnerViewport;
};
