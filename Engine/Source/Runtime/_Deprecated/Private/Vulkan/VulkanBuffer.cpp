/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#include "VulkanBuffer.h"
#include "Renderer/RHI/_Deprecated/Vulkan/IVulkanDynamicRHI.h"

FVulkanBuffer::FVulkanBuffer(const FRHIBufferCreateInfo& InCreateInfo)
	: FRHIBuffer(InCreateInfo), Buffer(this)
{
	CreateBuffer();
}

FVulkanBuffer::~FVulkanBuffer()
{
	DestroyBuffer();
}

FVulkanBuffer::FVulkanBuffer(FVulkanBuffer&& Other)
	: FRHIBuffer(Other.CreateInfo), Buffer(this, Other.Buffer), Allocation(Other.Allocation)
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
	Buffer = RHI::GetDynamicRHI<IVulkanDynamicRHI>()->CreateVulkanBuffer(CreateInfo, R_C(void**, &Allocation));
	HLVM_LOG(LogVulkanRHI, trace, TXT("Create buffer: {}"), *ToString());
}

void FVulkanBuffer::DestroyBuffer()
{
	// Buffer could be moved and already null
	if (Buffer == VK_NULL_HANDLE)
	{
		return;
	}
	RHI::GetDynamicRHI<IVulkanDynamicRHI>()->DestroyVulkanBuffer(Buffer, R_C(void**, &Allocation));
	HLVM_LOG(LogVulkanRHI, trace, TXT("Destroy buffer: {}"), *ToString());
}
