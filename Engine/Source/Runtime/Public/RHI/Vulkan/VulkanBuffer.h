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
	FVulkanBuffer(VkBuffer InBuffer, VkDeviceMemory InMemory, const FRHIBufferCreateDesc& InCreateDesc)
		: Buffer(InBuffer), Memory(InMemory)
	{
		CreateDesc = InCreateDesc;
	}

	// Returns the size of the buffer in bytes
	virtual TUINT32 GetSize() const override { return CreateDesc.SizeInBytes; }

	// Returns the usage flags of the buffer
	virtual EBufferUsageFlags GetUsageFlags() const override { return CreateDesc.UsageFlags; }

	// Returns the Vulkan buffer handle
	VkBuffer GetBuffer() const { return Buffer; }

	// Returns the Vulkan device memory handle
	VkDeviceMemory GetMemory() const { return Memory; }

private:
	VkBuffer	   Buffer;
	VkDeviceMemory Memory;
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
