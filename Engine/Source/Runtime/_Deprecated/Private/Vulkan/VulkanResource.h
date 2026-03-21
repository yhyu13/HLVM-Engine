/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#pragma once

#include "VulkanResourcePre.h"

#include "VulkanBuffer.h"
#include "VulkanShader.h"
#include "VulkanState.h"

class FVulkanView : public FVulkanMinimalContext, public FRefCountable
{
public:
	struct FInvalidatedState
	{
		bool bInitialized = false;
	};

	struct FTypedBufferView
	{
		VkBufferView View = VK_NULL_HANDLE;
		TUINT32		 ViewId = 0;
		bool		 bVolatile = false; // Whether source buffer is volatile
	};

	struct FStructuredBufferView
	{
		VkBuffer Buffer = VK_NULL_HANDLE;
		TUINT32	 HandleId = 0;
		TUINT32	 Offset = 0;
		TUINT32	 Size = 0;
	};

	struct FAccelerationStructureView
	{
		VkAccelerationStructureKHR Handle = VK_NULL_HANDLE;
	};

	struct FTextureView
	{
		VkImageView View = VK_NULL_HANDLE;
		VkImage		Image = VK_NULL_HANDLE;
		TUINT32		ViewId = 0;
	};

	typedef std::variant<
		FInvalidatedState, FTypedBufferView, FTextureView, FStructuredBufferView, FAccelerationStructureView>
		TStorage;

	enum class EType : TUINT8
	{
		Invalid = 0,
		TypedBuffer,
		Texture,
		StructuredBuffer,
		AccelerationStructure,
	};

	FVulkanView(VkDescriptorType InDescriptorType);
	~FVulkanView();

	void Invalidate();

	EType GetViewType() const
	{
		return EType(Storage.index());
	}

	bool IsInitialized() const
	{
		return (GetViewType() != EType::Invalid) || std::get<FInvalidatedState>(Storage).bInitialized;
	}

	const FTypedBufferView&			  GetTypedBufferView() const { return std::get<FTypedBufferView>(Storage); }
	const FTextureView&				  GetTextureView() const { return std::get<FTextureView>(Storage); }
	const FStructuredBufferView&	  GetStructuredBufferView() const { return std::get<FStructuredBufferView>(Storage); }
	const FAccelerationStructureView& GetAccelerationStructureView() const { return std::get<FAccelerationStructureView>(Storage); }

	// NOTE: The InOffset applies to the FVulkanResourceMultiBuffer (it does not include any internal Allocation offsets that may exist)
	FVulkanView* InitAsTypedBufferView(FVulkanBufferRef Buffer, EPixelFormat Format, TUINT32 InOffset, TUINT32 InSize);

	FVulkanView* InitAsTextureView(VkImage InImage, VkImageViewType ViewType, VkImageAspectFlags AspectFlags, EPixelFormat UEFormat, VkFormat Format, TUINT32 FirstMip, TUINT32 NumMips, TUINT32 ArraySliceIndex, TUINT32 NumArraySlices, bool bUseIdentitySwizzle = false, VkImageUsageFlags ImageUsageFlags = 0, VkSamplerYcbcrConversion SamplerYcbcrConversion = nullptr);

	// NOTE: The InOffset applies to the FVulkanResourceMultiBuffer (it does not include any internal Allocation offsets that may exist)
	FVulkanView* InitAsStructuredBufferView(FVulkanBufferRef Buffer, TUINT32 InOffset, TUINT32 InSize);

	FVulkanView* InitAsAccelerationStructureView(FVulkanBufferRef Buffer, TUINT32 Offset, TUINT32 Size);

	// No moving or copying
	FVulkanView(FVulkanView&&) = delete;
	FVulkanView(FVulkanView const&) = delete;
	FVulkanView& operator=(FVulkanView&&) = delete;
	FVulkanView& operator=(FVulkanView const&) = delete;

	FRHIDescriptorHandle GetBindlessHandle() const
	{
		return BindlessHandle;
	}

private:
	FRHIDescriptorHandle BindlessHandle;
	TStorage			 Storage{ FInvalidatedState() };
};
using FVulkanViewRef = TRefCountPtr<FVulkanView>;

class FVulkanRenderTargetLayout
{
public:
	struct FRenderPassAdditionalInfo
	{
		VkImageLayout CurrentDepthLayout{ VK_IMAGE_LAYOUT_UNDEFINED };
		VkImageLayout CurrentStencilLayout{ VK_IMAGE_LAYOUT_UNDEFINED };
	};

private:
	struct FRenderPassCompatibleHashableStruct
	{
		FRenderPassCompatibleHashableStruct()
		{
			FMemory::Memzero(this);
		}

		TUINT8							NumAttachments;
		TUINT8							NumSamples;
		TEnumAsUnderlying<ESubpassHint> SubpassHint;
		// +1 for Depth, +1 for Stencil, +1 for Fragment Density
		VkFormat Formats[RHI::MAX_RT_ATTACHMENTS + 2];
		bool	 AttachmentsToResolve[RHI::MAX_RT_ATTACHMENTS + 2];
	};

	struct FRenderPassFullHashableStruct
	{
		FRenderPassFullHashableStruct()
		{
			FMemory::Memzero(this);
		}

		// +1 for Depth, +1 for Stencil, +1 for Fragment Density
		TEnumAsUnderlying<VkAttachmentLoadOp>  LoadOps[RHI::MAX_RT_ATTACHMENTS + 2];
		TEnumAsUnderlying<VkAttachmentStoreOp> StoreOps[RHI::MAX_RT_ATTACHMENTS + 2];
		// If the initial != final we need to add FinalLayout and potentially RefLayout
		VkImageLayout InitialLayout[RHI::MAX_RT_ATTACHMENTS + 2];
		// VkImageLayout                   FinalLayout[RHI::MAX_RT_ATTACHMENTS  + 2];
		// VkImageLayout                   RefLayout[RHI::MAX_RT_ATTACHMENTS  + 2];
	};

public:
	FVulkanRenderTargetLayout(const FRHIRenderPassInfo& RPInfo, const FRenderPassAdditionalInfo& AdditionalInfo);
	FVulkanRenderTargetLayout(const FGraphicsPSOCreateInfo& PSOInfo);

	const FVulkanHash& GetRenderPassCompatibleHash() const { return RenderPassCompatibleHash; }
	const FVulkanHash& GetRenderPassFullHash() const { return RenderPassFullHash; }

	const VkOffset2D&			   GetOffset2D() const { return Offset.Offset2D; }
	const VkOffset3D&			   GetOffset3D() const { return Offset.Offset3D; }
	const VkExtent2D&			   GetExtent2D() const { return Extent.Extent2D; }
	const VkExtent3D&			   GetExtent3D() const { return Extent.Extent3D; }
	const VkAttachmentDescription* GetAttachmentDescriptions() const { return Desc; }
	TUINT32						   GetNumColorAttachments() const { return NumColorAttachments; }
	bool						   GetHasDepthStencil() const { return bHasDepthStencil != 0; }
	bool						   GetHasResolveAttachments() const { return bHasResolveAttachments != 0; }
	bool						   GetHasDepthStencilResolve() const { return bHasDepthStencilResolve != 0; }
	bool						   GetHasFragmentDensityAttachment() const { return bHasFragmentDensityAttachment != 0; }
	TUINT32						   GetNumAttachmentDescriptions() const { return NumAttachmentDescriptions; }
	TUINT32						   GetNumSamples() const { return NumSamples; }
	TUINT32						   GetNumUsedClearValues() const { return NumUsedClearValues; }

	const VkAttachmentReference*			  GetColorAttachmentReferences() const { return NumColorAttachments > 0 ? ColorReferences : nullptr; }
	const VkAttachmentReference*			  GetResolveAttachmentReferences() const { return bHasResolveAttachments ? ResolveReferences : nullptr; }
	const VkAttachmentReference*			  GetDepthAttachmentReference() const { return bHasDepthStencil ? &DepthReference : nullptr; }
	const VkAttachmentReferenceStencilLayout* GetStencilAttachmentReference() const { return bHasDepthStencil ? &StencilReference : nullptr; }
	const VkAttachmentReference*			  GetDepthStencilResolveAttachmentReference() const { return bHasDepthStencilResolve ? &DepthStencilResolveReference : nullptr; }
	const VkAttachmentReference*			  GetFragmentDensityAttachmentReference() const { return bHasFragmentDensityAttachment ? &FragmentDensityReference : nullptr; }

	const VkAttachmentDescriptionStencilLayout* GetStencilDesc() const { return bHasDepthStencil ? &StencilDesc : nullptr; }

	ESubpassHint GetSubpassHint() const { return SubpassHint; }

private:
	FVulkanRenderTargetLayout()
	{
		NumAttachmentDescriptions = 0;
		NumColorAttachments = 0;
		bHasDepthStencil = 0;
		bHasResolveAttachments = 0;
		bHasDepthStencilResolve = 0;
		bHasFragmentDensityAttachment = 0;
		NumSamples = 0;
		NumUsedClearValues = 0;

		ResetAttachments();
	}

	void ResetAttachments()
	{
		FMemory::MemzeroArray(&ColorReferences);
		FMemory::Memzero(&DepthReference);
		FMemory::Memzero(&FragmentDensityReference);
		FMemory::MemzeroArray(&ResolveReferences);
		FMemory::Memzero(&DepthStencilResolveReference);
		FMemory::MemzeroArray(&Desc);
		FMemory::Memzero(&Offset);
		FMemory::Memzero(&Extent);

		VulkanRHI::ZeroVulkanStruct(&StencilReference, VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_STENCIL_LAYOUT);
		VulkanRHI::ZeroVulkanStruct(&StencilDesc, VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_STENCIL_LAYOUT);
	}

	VkImageLayout GetVRSImageLayout() const;

private:
	friend struct FVulkanGraphicsPSODescription;

	VkAttachmentReference			   ColorReferences[RHI::MAX_RT_ATTACHMENTS];
	VkAttachmentReference			   DepthReference;
	VkAttachmentReferenceStencilLayout StencilReference;
	VkAttachmentReference			   FragmentDensityReference;
	VkAttachmentReference			   ResolveReferences[RHI::MAX_RT_ATTACHMENTS];
	VkAttachmentReference			   DepthStencilResolveReference;

	// Depth goes in the "+1" slot, Depth resolve goes in the "+2 slot", and the Shading Rate texture goes in the "+3" slot.
	VkAttachmentDescription				 Desc[RHI::MAX_RT_ATTACHMENTS * 2 + 2];
	VkAttachmentDescriptionStencilLayout StencilDesc;

	TUINT8		 NumAttachmentDescriptions;
	TUINT8		 NumColorAttachments;
	TUINT8		 bHasDepthStencil;
	TUINT8		 bHasResolveAttachments;
	TUINT8		 bHasDepthStencilResolve;
	TUINT8		 bHasFragmentDensityAttachment;
	TUINT8		 NumSamples;
	TUINT8		 NumUsedClearValues;
	ESubpassHint SubpassHint = ESubpassHint::Default;

	FVulkanHash RenderPassCompatibleHash;
	FVulkanHash RenderPassFullHash;

	union
	{
		VkOffset3D Offset3D;
		VkOffset2D Offset2D;
	} Offset;

	union
	{
		VkExtent3D Extent3D;
		VkExtent2D Extent2D;
	} Extent;
};
