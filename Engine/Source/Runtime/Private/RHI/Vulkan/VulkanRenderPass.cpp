/**
* Copyright (c) 2025. MIT License. All rights reserved.
*/

#include "VulkanRenderPass.h"

namespace VulkanRHI
{
	VkRenderPass CreateVulkanRenderPass(FVulkanLogicalDeviceRef InDevice, const FVulkanRenderTargetLayout& RTLayout)
	{
		VkRenderPass RenderPass = VK_NULL_HANDLE;
#if VULKAN_RENDERPASS2
		// TODO
#else
		FVulkanRenderPassBuilder<FVulkanSubpassDescription<VkSubpassDescription>, FVulkanSubpassDependency<VkSubpassDependency>, FVulkanAttachmentReference<VkAttachmentReference>, FVulkanAttachmentDescription<VkAttachmentDescription>, FVulkanRenderPassCreateInfo<VkRenderPassCreateInfo>> Creator(InDevice);
		RenderPass = Creator.Create(RTLayout);
#endif
		return RenderPass;
	}
}
