/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "RHI/RHIResource.h"
#include "VulkanResourcePre.h"

// Vulkan ParameterMap:
// Buffer Index = EBufferIndex
// Base Offset = Index into the subtype
// Size = Ignored for non-globals
struct FVulkanShaderHeader
{
	// Includes all bindings, the index in this array is the binding slot
	struct FBindingInfo
	{
		TUINT32 DescriptorType; // VkDescriptorType
	};
	TVector<FBindingInfo> Bindings;

	// FBindingInfo with type VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER have a corresponding entry in this table (at the same index)
	struct FUniformBufferInfo
	{
		TUINT32 LayoutHash;
		TUINT8	bHasResources;
		TUINT8	BindlessCBIndex;
	};
	TVector<FUniformBufferInfo> UniformBufferInfos;

	// The order of this enum should always match the strings in VulkanBackend.cpp (VULKAN_SUBPASS_FETCH)
	enum class EAttachmentType : TUINT8
	{
		Depth,
		Color0,
		Color1,
		Color2,
		Color3,
		Color4,
		Color5,
		Color6,
		Color7,

		Count,
	};

	// Used to determine the EAttachmentType of a FBindingInfo with type VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT
	struct FInputAttachmentInfo
	{
		TUINT8			BindingIndex;
		EAttachmentType Type;
	};
	TVector<FInputAttachmentInfo> InputAttachmentInfos;

	// The number of uniform buffers containing constants and requiring bindings
	// Uniform buffers beyond this index do not have bindings (resource only UB)
	TUINT32 NumBoundUniformBuffers = 0;

	// Size of the uniform buffer containing packed globals
	// If present (not zero), it will always be at binding 0 of the stage
	TUINT32 PackedGlobalsSize = 0;

	// Mask of input attachments being used (the index of the bit corresponds to EAttachmentType value)
	TUINT32 InputAttachmentsMask = 0;

	// Mostly relevant for Vertex Shaders
	TUINT32 InOutMask;

	// Relevant for Ray Tracing Shaders
	TUINT32 RayTracingPayloadType = 0;
	TUINT32 RayTracingPayloadSize = 0;

	FVulkanHash SourceCodeHash;
	FVulkanHash SpirvCodeHash;
	TUINT8		WaveSize = 0;

	// For RayHitGroup shaders
	enum class ERayHitGroupEntrypoint : TUINT8
	{
		NotPresent = 0,

		// Hit group types are all stored in a single spirv blob
		// and each have different entry point names
		// NOTE: Not used yet because of compiler issues
		CommonBlob,

		// Hit group types are each stored in a different spirv blob
		// to circumvent DXC compilation issues
		SeparateBlob
	};
	ERayHitGroupEntrypoint RayGroupAnyHit = ERayHitGroupEntrypoint::NotPresent;
	ERayHitGroupEntrypoint RayGroupIntersection = ERayHitGroupEntrypoint::NotPresent;

	FString DebugName;

	FVulkanShaderHeader() = default;
	enum EInit
	{
		EZero
	};
	FVulkanShaderHeader(EInit)
		: InOutMask(0)
	{
	}
};

struct FGraphicsShaderGatherInfo
{
	TNullablePtr<const FVulkanShaderHeader> ShaderHeaders[RHI::MAX_SHADER_STAGES];
};

// Vulkan-specific RHI shader
class FVulkanShader : public FRHIShader, public FVulkanResource
{
public:
	FVulkanShader(const FShaderCreateInfo& InCreateInfo);
	~FVulkanShader() override;

	// Returns the Vulkan shader module handle
	VkShaderModule GetShaderModule() const { return ShaderModule; }

	const VkPipelineShaderStageCreateInfo& GetPipelineShaderStageCreateInfo() const { return PipelineShaderStageCreateInfo; }

	void					   SetShaderHeader(const FVulkanShaderHeader& InShaderHeader) { ShaderHeader = InShaderHeader; }
	const FVulkanShaderHeader* GetCodeHeader() const { return &ShaderHeader; }

	bool UsesBindless() const { return false; }

private:
	VkShaderModule					ShaderModule;
	VkPipelineShaderStageCreateInfo PipelineShaderStageCreateInfo;
	FVulkanShaderHeader				ShaderHeader;
};

// Vulkan-specific RHI shader resource view
class FVulkanShaderResourceView : public FRHIShaderResourceView, public FVulkanResource
{
public:
	FVulkanShaderResourceView(VkImageView InImageView, const FRHIShaderResourceViewCreateInfo& InCreateInfo)
		: FRHIShaderResourceView(InCreateInfo), ImageView(InImageView)
	{
	}

	// Returns the Vulkan image view handle
	VkImageView GetImageView() const { return ImageView; }

private:
	VkImageView ImageView;
};

using FVulkanShaderRef = TRefCountPtr<FVulkanShader>;
using FVulkanShaderResourceViewRef = TRefCountPtr<FVulkanShaderResourceView>;
