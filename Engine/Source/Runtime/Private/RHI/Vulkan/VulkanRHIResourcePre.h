/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "VulkanMisc.h"
#include "VulkanDevice.h"
#include "VulkanSyncObject.h"

struct FVulkanMinimalContext
{
	explicit FVulkanMinimalContext(VkInstance InInstance,
		FVulkanPhysicalDeviceRef			  InPhysicalDevice,
		FVulkanLogicalDeviceRef				  InDevice)
		: Instance(InInstance)
		, PhysicalDevice(InPhysicalDevice)
		, LogicalDevice(InDevice)
	{
	}

	template <typename T>
	void Update(T& InResource)
	{
		InResource.Instance = Instance;
		InResource.PhysicalDevice = PhysicalDevice;
		InResource.LogicalDevice = LogicalDevice;
	}

	VkInstance				 Instance;
	FVulkanPhysicalDeviceRef PhysicalDevice;
	FVulkanLogicalDeviceRef	 LogicalDevice;
};

// Base class for all RHI resources
class FVulkanResource : virtual public IRHIResource
{
public:
	FVulkanResource() = default;
	virtual ERHIInterfaceType GetInterfaceType() const override { return ERHIInterfaceType::Vulkan; }
};

class FVulkanRenderTargetLayout
{
public:
	FVulkanRenderTargetLayout(const FRHIRenderPassInfo& RPInfo);

	inline const VkOffset2D&			  GetOffset2D() const { return Offset.Offset2D; }
	inline const VkOffset3D&			  GetOffset3D() const { return Offset.Offset3D; }
	inline const VkExtent2D&			  GetExtent2D() const { return Extent.Extent2D; }
	inline const VkExtent3D&			  GetExtent3D() const { return Extent.Extent3D; }
	inline const VkAttachmentDescription* GetAttachmentDescriptions() const { return Desc; }
	inline TUINT32						  GetNumColorAttachments() const { return NumColorAttachments; }
	inline bool							  GetHasDepthStencil() const { return bHasDepthStencil != 0; }
	inline bool							  GetHasResolveAttachments() const { return bHasResolveAttachments != 0; }
	inline bool							  GetHasDepthStencilResolve() const { return bHasDepthStencilResolve != 0; }
	inline bool							  GetHasFragmentDensityAttachment() const { return bHasFragmentDensityAttachment != 0; }
	inline TUINT32						  GetNumAttachmentDescriptions() const { return NumAttachmentDescriptions; }
	inline TUINT32						  GetNumSamples() const { return NumSamples; }
	inline TUINT32						  GetNumUsedClearValues() const { return NumUsedClearValues; }

	inline const VkAttachmentReference*				 GetColorAttachmentReferences() const { return NumColorAttachments > 0 ? ColorReferences : nullptr; }
	inline const VkAttachmentReference*				 GetResolveAttachmentReferences() const { return bHasResolveAttachments ? ResolveReferences : nullptr; }
	inline const VkAttachmentReference*				 GetDepthAttachmentReference() const { return bHasDepthStencil ? &DepthReference : nullptr; }
	inline const VkAttachmentReferenceStencilLayout* GetStencilAttachmentReference() const { return bHasDepthStencil ? &StencilReference : nullptr; }
	inline const VkAttachmentReference*				 GetDepthStencilResolveAttachmentReference() const { return bHasDepthStencilResolve ? &DepthStencilResolveReference : nullptr; }
	inline const VkAttachmentReference*				 GetFragmentDensityAttachmentReference() const { return bHasFragmentDensityAttachment ? &FragmentDensityReference : nullptr; }

	inline const VkAttachmentDescriptionStencilLayout* GetStencilDesc() const { return bHasDepthStencil ? &StencilDesc : nullptr; }

	inline ESubpassHint GetSubpassHint() const { return SubpassHint; }

protected:
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

protected:
	VkAttachmentReference			   ColorReferences[RHI::MAX_RT_ATTACHMENTS];
	VkAttachmentReference			   DepthReference;
	VkAttachmentReferenceStencilLayout StencilReference;
	VkAttachmentReference			   FragmentDensityReference;
	VkAttachmentReference			   ResolveReferences[RHI::MAX_RT_ATTACHMENTS];
	VkAttachmentReference			   DepthStencilResolveReference;

	// Depth goes in the "+1" slot, Depth resolve goes in the "+2 slot", and the Shading Rate texture goes in the "+3" slot.
	VkAttachmentDescription				 Desc[RHI::MAX_RT_ATTACHMENTS * 2 + 3];
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

// class FVulkanFramebuffer
//{
// public:
//	FVulkanFramebuffer(FVulkanLogicalDeviceRef Device, const FRHISetRenderTargetsInfo& InRTInfo, const FVulkanRenderTargetLayout& RTLayout, const FVulkanRenderPass& RenderPass);
//	~FVulkanFramebuffer();
//
//	bool Matches(const FRHISetRenderTargetsInfo& RTInfo) const;
//
//	TUINT32 GetNumColorAttachments() const
//	{
//		return NumColorAttachments;
//	}
//
//	void Destroy(FVulkanLogicalDeviceRef Device);
//
//	VkFramebuffer GetHandle()
//	{
//		return Framebuffer;
//	}
//
//	const FVulkanView::FTextureView& GetPartialDepthTextureView() const
//	{
//		HLVM_ASSERT(PartialDepthTextureView);
//		return PartialDepthTextureView->GetTextureView();
//	}
//
//	TIndirectArray<FVulkanView> OwnedTextureViews;
//	TArray<FVulkanView const*> AttachmentTextureViews;
//
//	// Copy from the Depth render target partial view
//	FVulkanView const* PartialDepthTextureView = nullptr;
//
//	bool ContainsRenderTarget(FRHITexture* Texture) const
//	{
//		ensure(Texture);
//		FVulkanTexture* VulkanTexture = ResourceCast(Texture);
//		return ContainsRenderTarget(VulkanTexture->Image);
//	}
//
//	bool ContainsRenderTarget(VkImage Image) const
//	{
//		ensure(Image != VK_NULL_HANDLE);
//		for (TUINT32 Index = 0; Index < NumColorAttachments; ++Index)
//		{
//			if (ColorRenderTargetImages[Index] == Image)
//			{
//				return true;
//			}
//		}
//
//		return (DepthStencilRenderTargetImage == Image);
//	}
//
//	VkRect2D GetRenderArea() const
//	{
//		return RenderArea;
//	}
//
// private:
//	VkFramebuffer Framebuffer;
//	VkRect2D RenderArea;
//
//	// Unadjusted number of color render targets as in FRHISetRenderTargetsInfo
//	TUINT32 NumColorRenderTargets;
//
//	// Save image off for comparison, in case it gets aliased.
//	TUINT32 NumColorAttachments;
//	VkImage ColorRenderTargetImages[RHI::MAX_RT_ATTACHMENTS];
//	VkImage ColorResolveTargetImages[RHI::MAX_RT_ATTACHMENTS];
//	VkImage DepthStencilRenderTargetImage;
//	VkImage DepthStencilResolveRenderTargetImage;
//	VkImage FragmentDensityImage;
//
//	// Predefined set of barriers, when executes ensuring all writes are finished
//	TArray<VkImageMemoryBarrier> WriteBarriers;
//
//	friend class FVulkanCommandListContext;
// };
//
