/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "Renderer/RHI/_Deprecated/RHIResource.h"
#include "VulkanResourcePre.h"

// Vulkan-specific RHI buffer
class FVulkanBuffer : public FRHIBuffer, public FVulkanResource
{
public:
	FVulkanBuffer(const FRHIBufferCreateInfo& InCreateInfo);
	~FVulkanBuffer() override;

	FVulkanBuffer(const FVulkanBuffer&) = delete;
	FVulkanBuffer& operator=(const FVulkanBuffer&) = delete;

	FVulkanBuffer(FVulkanBuffer&& Other);
	FVulkanBuffer& operator=(FVulkanBuffer&& Other);

	// Returns the Vulkan buffer handle
	VkBuffer GetBuffer() const { return Buffer; }

private:
	void CreateBuffer();
	void DestroyBuffer();

private:
	IRHIHandle<VkBuffer> Buffer;
	VmaAllocation		 Allocation;
};

// Vulkan-specific RHI unordered access view
class FVulkanUnorderedAccessView : public FRHIUnorderedAccessView, public FVulkanResource
{
public:
	FVulkanUnorderedAccessView(VkBufferView InBufferView, const FRHIUnorderedAccessViewCreateInfo& InCreateInfo)
		: FRHIUnorderedAccessView(InCreateInfo), BufferView(InBufferView)
	{
	}

	// Returns the Vulkan buffer view handle
	VkBufferView GetBufferView() const { return BufferView; }

private:
	VkBufferView BufferView;
};

using FVulkanBufferRef = TRefCountPtr<FVulkanBuffer>;
using FVulkanUnorderedAccessViewRef = TRefCountPtr<FVulkanUnorderedAccessView>;
