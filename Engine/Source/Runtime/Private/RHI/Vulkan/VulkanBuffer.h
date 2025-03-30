/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "RHI/RHIResource.h"
#include "VulkanRHIResourceDeclaration.h"

// Vulkan-specific RHI buffer
class FVulkanBuffer : public FRHIBuffer, public FVulkanResource
{
public:
	FVulkanBuffer(const FRHIBufferCreateDesc& InCreateDesc);
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
	VkBuffer	  Buffer;
	VmaAllocation Allocation;
};

// Vulkan-specific RHI unordered access view
class FVulkanUnorderedAccessView : public FRHIUnorderedAccessView, public FVulkanResource
{
public:
	FVulkanUnorderedAccessView(VkBufferView InBufferView, const FRHIUnorderedAccessViewCreateInfo& InCreateDesc)
		: BufferView(InBufferView)
	{
		CreateDesc = InCreateDesc;
	}

	// Returns the Vulkan buffer view handle
	VkBufferView GetBufferView() const { return BufferView; }

private:
	VkBufferView BufferView;
};

using FVulkanBufferRef = TRefCountPtr<FVulkanBuffer>;
using FVulkanUnorderedAccessViewRef = TRefCountPtr<FVulkanUnorderedAccessView>;
