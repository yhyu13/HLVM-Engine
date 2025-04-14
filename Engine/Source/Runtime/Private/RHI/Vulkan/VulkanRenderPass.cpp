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
		FVulkanRenderPassBuilder<FVulkanSubpassDescription<VkSubpassDescription2>,
			FVulkanSubpassDependency<VkSubpassDependency2>,
			FVulkanAttachmentReference<VkAttachmentReference2>,
			FVulkanAttachmentDescription<VkAttachmentDescription2>,
			FVulkanRenderPassCreateInfo<VkRenderPassCreateInfo2>>
			Creator(InDevice);
#else
		FVulkanRenderPassBuilder<FVulkanSubpassDescription<VkSubpassDescription>,
			FVulkanSubpassDependency<VkSubpassDependency>,
			FVulkanAttachmentReference<VkAttachmentReference>,
			FVulkanAttachmentDescription<VkAttachmentDescription>,
			FVulkanRenderPassCreateInfo<VkRenderPassCreateInfo>>
			Creator(InDevice);
#endif
		RenderPass = Creator.Create(RTLayout);
		return RenderPass;
	}
} // namespace VulkanRHI

FVulkanRenderPass::FVulkanRenderPass(FVulkanLogicalDeviceRef InDevice, const FVulkanRenderTargetLayout& InRTLayout)
	: Device(InDevice), Layout(InRTLayout)
{
	RenderPass = VulkanRHI::CreateVulkanRenderPass(Device, Layout);
}

FVulkanRenderPass::~FVulkanRenderPass()
{
	VulkanRHI::vkDestroyRenderPass(Device->GetHandle(), RenderPass, VulkanRHI::VULKAN_CPU_ALLOCATOR);
}
