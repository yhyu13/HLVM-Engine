/**
* Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "RHI/DynamicRHI.h"
#include "VulkanLoader.h"

class IVulkanDynamicRHI : public FDynamicRHI
{
public:
	HLVM_STATIC_FUNC IVulkanDynamicRHI* Get()
	{
		return RHI::GetDynamicRHI<IVulkanDynamicRHI>();
	}

	// RHI Interface Type
	virtual ERHIInterfaceType GetInterfaceType() const override { return ERHIInterfaceType::Vulkan; }

	// Vulkan-specific resource creation
	HLVM_NODISCARD virtual VkImage CreateVulkanImage(const FRHITextureCreateInfo& CreateInfo) = 0;
	virtual void DestroyVulkanImage(VkImage Image) = 0;
	HLVM_NODISCARD virtual VkSampler CreateVulkanSampler(const FRHISamplerStateCreateInfo& CreateInfo) = 0;

	HLVM_NODISCARD virtual VkBuffer CreateVulkanBuffer(const FRHIBufferCreateInfo& CreateInfo, void** OutAllocation) = 0;
	virtual void DestroyVulkanBuffer(VkBuffer Buffer, void** InAllocation) = 0;
	HLVM_NODISCARD virtual VkImageView CreateVulkanImageView(VkImage Image, const FRHIShaderResourceViewCreateInfo& CreateInfo) = 0;
	HLVM_NODISCARD virtual VkBufferView CreateVulkanBufferView(VkBuffer Buffer, const FRHIUnorderedAccessViewCreateInfo& CreateInfo) = 0;

	HLVM_NODISCARD virtual VkShaderModule CreateVulkanShaderModule(const FShaderCreateInfo& CreateInfo) = 0;
	virtual void DestroyVulkanShaderModule(VkShaderModule ShaderModule) = 0;
	HLVM_NODISCARD virtual VkPipelineShaderStageCreateInfo GenerateVkPipelineShaderStageCreateInfo(const FShaderCreateInfo& CreateInfo, VkShaderModule ShaderModule) = 0;

	// Vulkan-specific command list management
	HLVM_NODISCARD virtual VkCommandBuffer BeginVulkanCommandBuffer() = 0;
	virtual void EndVulkanCommandBuffer(VkCommandBuffer CommandBuffer) = 0;

	// Vulkan-specific synchronization
	virtual void SubmitVulkanCommandsAndFlushGPU() = 0;
	virtual void FlushVulkanResources() = 0;

	// Vulkan-specific viewport and swap chain management
	virtual void CreateVulkanSwapChain(void* WindowHandle, TUINT32 Width, TUINT32 Height, bool bIsFullscreen, EPixelFormat PreferredPixelFormat, FRHIViewportRef& OutViewport) = 0;
	virtual void ResizeVulkanSwapChain(FRHIViewportRef& Viewport, TUINT32 Width, TUINT32 Height, bool bIsFullscreen, EPixelFormat PreferredPixelFormat) = 0;
	virtual void PresentVulkanSwapChain(FRHIViewportRef& Viewport) = 0;

	// Vulkan-specific render pass management
	virtual void BeginVulkanRenderPass(const FRHIRenderPassInfo& RenderPassInfo) = 0;
	virtual void EndVulkanRenderPass() = 0;

	// Vulkan-specific query and timestamp management
	HLVM_NODISCARD virtual VkQueryPool CreateVulkanQueryPool(ERHIQueryType QueryType) = 0;
	virtual void BeginVulkanQuery(VkQueryPool QueryPool, TUINT32 QueryIndex) = 0;
	virtual void EndVulkanQuery(VkQueryPool QueryPool, TUINT32 QueryIndex) = 0;
	virtual void GetVulkanQueryResults(VkQueryPool QueryPool, TUINT32 QueryIndex, TUINT64& OutResult, bool bWait) = 0;

	// Vulkan-specific debugging and profiling
	virtual void PushVulkanEvent(const TCHAR* Name) = 0;
	virtual void PopVulkanEvent() = 0;

	// Vulkan-specific memory management
	virtual void FlushVulkanPendingDeletes() = 0;

	virtual void SetVulkanMinimalContext(void* InContext) const = 0;
};
