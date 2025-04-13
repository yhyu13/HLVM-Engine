/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#include "VulkanBuffer.h"
#include "RHI/Vulkan/IVulkanDynamicRHI.h"

FVulkanBuffer::FVulkanBuffer(const FRHIBufferCreateInfo& InCreateInfo)
	: FRHIBuffer(InCreateInfo), Buffer(VK_NULL_HANDLE)
{
	CreateBuffer();
	HLVM_LOG(LogVulkanRHI, trace, TXT("Create buffer: {}"), *GetName());
}

FVulkanBuffer::~FVulkanBuffer()
{
	DestroyBuffer();
	HLVM_LOG(LogVulkanRHI, trace, TXT("Destroy buffer: {}"), *GetName());
}

FVulkanBuffer::FVulkanBuffer(FVulkanBuffer&& Other)
	: FRHIBuffer(Other.CreateInfo), Buffer(Other.Buffer), Allocation(Other.Allocation)
{
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
		UpdateCreateInfo(Other.CreateInfo);

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
