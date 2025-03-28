/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#include "RHI/Vulkan/VulkanBuffer.h"
#include "RHI/Vulkan/IVulkanDynamicRHI.h"

FVulkanBuffer::FVulkanBuffer(const FRHIBufferCreateDesc& InCreateDesc)
	: Buffer(VK_NULL_HANDLE)
{
	CreateDesc = InCreateDesc;
	CreateBuffer();

	HLVM_LOG(LogRHI, trace, TXT("Create buffer: {}"), *CreateDesc.DebugName);
}

FVulkanBuffer::~FVulkanBuffer()
{
	DestroyBuffer();

	HLVM_LOG(LogRHI, trace, TXT("Destroy buffer: {}"), *CreateDesc.DebugName);
}

FVulkanBuffer::FVulkanBuffer(FVulkanBuffer&& Other)
	: Buffer(Other.Buffer), Allocation(Other.Allocation)
{
	CreateDesc = Other.CreateDesc;

	Other.Buffer = VK_NULL_HANDLE;
	Other.Allocation = VK_NULL_HANDLE;
}

FVulkanBuffer& FVulkanBuffer::operator=(FVulkanBuffer&& Other)
{
	if (this != &Other)
	{
		DestroyBuffer();
		Buffer = Other.Buffer;
		Allocation = Other.Allocation;
		CreateDesc = Other.CreateDesc;

		Other.Buffer = VK_NULL_HANDLE;
		Other.Allocation = VK_NULL_HANDLE;
	}
	return *this;
}

void FVulkanBuffer::CreateBuffer()
{
	VkBufferUsageFlags	  usage = VulkanBufferUsageFlagsFromRHIUsageFlags(CreateDesc.UsageFlags);
	VkMemoryPropertyFlags memoryProperties = VulkanMemoryPropertyFlagsFromRHIMemoryPropertyFlags(CreateDesc.MemoryPropertyFlags);
	VkDeviceSize		  size = CreateDesc.Size;

	GetDynamicRHI<IVulkanDynamicRHI>()->CreateVulkanBuffer(CreateDesc, R_C(void**, &Allocation));
}

void FVulkanBuffer::DestroyBuffer()
{
	GetDynamicRHI<IVulkanDynamicRHI>()->DestoryVulkanBuffer(Buffer, R_C(void**, &Allocation));
}
