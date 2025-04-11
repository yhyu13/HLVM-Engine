/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#include "VulkanBuffer.h"
#include "RHI/Vulkan/IVulkanDynamicRHI.h"

FVulkanBuffer::FVulkanBuffer(const FRHIBufferCreateInfo& InCreateInfo)
	: Buffer(VK_NULL_HANDLE)
{
	CreateInfo = InCreateInfo;
	CreateBuffer();
	HLVM_LOG(LogRHI, trace, TXT("Create buffer: {}"), *CreateInfo.DebugName);
}

FVulkanBuffer::~FVulkanBuffer()
{
	DestroyBuffer();
	HLVM_LOG(LogRHI, trace, TXT("Destroy buffer: {}"), *CreateInfo.DebugName);
}

FVulkanBuffer::FVulkanBuffer(FVulkanBuffer&& Other)
	: Buffer(Other.Buffer), Allocation(Other.Allocation)
{
	CreateInfo = Other.CreateInfo;

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
		CreateInfo = Other.CreateInfo;

		Other.Buffer = VK_NULL_HANDLE;
		Other.Allocation = VK_NULL_HANDLE;
	}
	return *this;
}

void FVulkanBuffer::CreateBuffer()
{
	Buffer = GetDynamicRHI<IVulkanDynamicRHI>()->CreateVulkanBuffer(CreateInfo, R_C(void**, &Allocation));
}

void FVulkanBuffer::DestroyBuffer()
{
	if (Buffer == VK_NULL_HANDLE)
	{
		return;
	}
	GetDynamicRHI<IVulkanDynamicRHI>()->DestroyVulkanBuffer(Buffer, R_C(void**, &Allocation));
}
