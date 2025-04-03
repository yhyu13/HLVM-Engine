/**
* Copyright (c) 2025. MIT License. All rights reserved.
*/

#pragma once

#include "RHI/RHIResource.h"
#include "VulkanRHIResourceDeclaration.h"

template <typename TAttachmentReferenceType>
struct FVulkanAttachmentReference
	: public TAttachmentReferenceType
{
	FVulkanAttachmentReference()
	{
		ZeroStruct();
	}

	FVulkanAttachmentReference(const VkAttachmentReference& AttachmentReferenceIn, VkImageAspectFlags AspectMask)
	{
		SetAttachment(AttachmentReferenceIn, AspectMask);
	}

	inline void SetAttachment(const VkAttachmentReference& /*AttachmentReferenceIn*/, VkImageAspectFlags /*AspectMask*/) { HLVM_NOT_IMPLEMENTED(); }
	inline void SetAttachment(const FVulkanAttachmentReference<TAttachmentReferenceType>& AttachmentReferenceIn, VkImageAspectFlags /*AspectMask*/) { *this = AttachmentReferenceIn; }
	inline void SetDepthStencilAttachment(const VkAttachmentReference& /*AttachmentReferenceIn*/, const VkAttachmentReferenceStencilLayout* /*StencilReference*/, VkImageAspectFlags /*AspectMask*/, bool /*bSupportsParallelRendering*/) { HLVM_NOT_IMPLEMENTED(); }
	inline void ZeroStruct() {}
	inline void SetAspect(TUINT32 /*Aspect*/) {}
};

template <>
inline void FVulkanAttachmentReference<VkAttachmentReference>::SetAttachment(const VkAttachmentReference& AttachmentReferenceIn, VkImageAspectFlags /*AspectMask*/)
{
	attachment = AttachmentReferenceIn.attachment;
	layout = AttachmentReferenceIn.layout;
}

template <>
inline void FVulkanAttachmentReference<VkAttachmentReference2>::SetAttachment(const VkAttachmentReference& AttachmentReferenceIn, VkImageAspectFlags AspectMask)
{
	sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
	pNext = nullptr;
	attachment = AttachmentReferenceIn.attachment;
	layout = AttachmentReferenceIn.layout;
	aspectMask = AspectMask;
}

template<>
inline void FVulkanAttachmentReference<VkAttachmentReference2>::SetAttachment(const FVulkanAttachmentReference<VkAttachmentReference2>& AttachmentReferenceIn, VkImageAspectFlags AspectMask)
{
	sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
	pNext = nullptr;
	attachment = AttachmentReferenceIn.attachment;
	layout = AttachmentReferenceIn.layout;
	aspectMask = AspectMask;
}

template <>
inline void FVulkanAttachmentReference<VkAttachmentReference>::SetDepthStencilAttachment(const VkAttachmentReference& AttachmentReferenceIn,
	const VkAttachmentReferenceStencilLayout* StencilReference, VkImageAspectFlags /*AspectMask*/, bool /*bSupportsParallelRendering*/)
{
	attachment = AttachmentReferenceIn.attachment;
	const VkImageLayout StencilLayout = StencilReference ? StencilReference->stencilLayout : VK_IMAGE_LAYOUT_UNDEFINED;
	layout = VulkanRHI::VulkanGetMergedDepthStencilLayout(AttachmentReferenceIn.layout, StencilLayout);
}

template <>
inline void FVulkanAttachmentReference<VkAttachmentReference2>::SetDepthStencilAttachment(const VkAttachmentReference& AttachmentReferenceIn,
	const VkAttachmentReferenceStencilLayout* StencilReference, VkImageAspectFlags AspectMask, bool bSupportsParallelRendering)
{
	sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
	pNext = (bSupportsParallelRendering && StencilReference && StencilReference->stencilLayout != VK_IMAGE_LAYOUT_UNDEFINED) ? StencilReference : nullptr;
	attachment = AttachmentReferenceIn.attachment;
	layout = bSupportsParallelRendering ? AttachmentReferenceIn.layout : VulkanRHI::VulkanGetMergedDepthStencilLayout(AttachmentReferenceIn.layout, StencilReference->stencilLayout);
	aspectMask = AspectMask;
}


template<>
inline void FVulkanAttachmentReference<VkAttachmentReference>::ZeroStruct()
{
	attachment = 0;
	layout = VK_IMAGE_LAYOUT_UNDEFINED;
}

template<>
inline void FVulkanAttachmentReference<VkAttachmentReference2>::ZeroStruct()
{
	sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
	pNext = nullptr;
	attachment = 0;
	layout = VK_IMAGE_LAYOUT_UNDEFINED;
	aspectMask = 0;
}

template<>
inline void FVulkanAttachmentReference<VkAttachmentReference2>::SetAspect(TUINT32 Aspect)
{
	aspectMask = Aspect;
}

template <typename TSubpassDescriptionType>
class FVulkanSubpassDescription
{
};

template<>
struct FVulkanSubpassDescription<VkSubpassDescription>
	: public VkSubpassDescription
{
	FVulkanSubpassDescription()
	{
		FMemory::Memzero(this, sizeof(VkSubpassDescription));
		pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	}

	void SetColorAttachments(const TVector<FVulkanAttachmentReference<VkAttachmentReference>>& ColorAttachmentReferences, TUINT32 OverrideCount = 0)
	{
		colorAttachmentCount = (OverrideCount == 0) ? ColorAttachmentReferences.Num() : OverrideCount;
		pColorAttachments = ColorAttachmentReferences.GetDataConst();
	}

	void SetResolveAttachments(const TVector<FVulkanAttachmentReference<VkAttachmentReference>>& ResolveAttachmentReferences)
	{
		if (ResolveAttachmentReferences.Num() > 0)
		{
			HLVM_ASSERT(colorAttachmentCount == ResolveAttachmentReferences.Num());
			pResolveAttachments = ResolveAttachmentReferences.GetDataConst();
		}
	}

	void SetDepthStencilAttachment(FVulkanAttachmentReference<VkAttachmentReference>* DepthStencilAttachmentReference)
	{
		pDepthStencilAttachment = static_cast<VkAttachmentReference*>(DepthStencilAttachmentReference);
	}

	void SetInputAttachments(FVulkanAttachmentReference<VkAttachmentReference>* InputAttachmentReferences, TUINT32 NumInputAttachmentReferences)
	{
		pInputAttachments = static_cast<VkAttachmentReference*>(InputAttachmentReferences);
		inputAttachmentCount = NumInputAttachmentReferences;
	}

	void SetDepthStencilResolveAttachment(VkSubpassDescriptionDepthStencilResolveKHR* /*DepthStencilResolveAttachmentDesc*/)
	{
		// No-op without VK_KHR_create_renderpass2
	}

	void SetShadingRateAttachment(VkFragmentShadingRateAttachmentInfoKHR* /* ShadingRateAttachmentInfo */)
	{
		// No-op without VK_KHR_create_renderpass2
	}

	void SetMultiViewMask(TUINT32 /*Mask*/)
	{
		// No-op without VK_KHR_create_renderpass2
	}
};

template<>
struct FVulkanSubpassDescription<VkSubpassDescription2>
	: public VkSubpassDescription2
{
	FVulkanSubpassDescription()
	{
		ZeroVulkanStruct(this, VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2);
		pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		viewMask = 0;
	}

	void SetColorAttachments(const TVector<FVulkanAttachmentReference<VkAttachmentReference2>>& ColorAttachmentReferences, TUINT32 OverrideCount = 0)
	{
		colorAttachmentCount = (OverrideCount == 0) ? ColorAttachmentReferences.Num() : OverrideCount;
		pColorAttachments = ColorAttachmentReferences.GetDataConst();
	}

	void SetResolveAttachments(const TVector<FVulkanAttachmentReference<VkAttachmentReference2>>& ResolveAttachmentReferences)
	{
		if (ResolveAttachmentReferences.Num() > 0)
		{
			HLVM_ASSERT(colorAttachmentCount == ResolveAttachmentReferences.Num());
			pResolveAttachments = ResolveAttachmentReferences.GetDataConst();
		}
	}

	void SetDepthStencilAttachment(FVulkanAttachmentReference<VkAttachmentReference2>* DepthStencilAttachmentReference)
	{
		pDepthStencilAttachment = static_cast<VkAttachmentReference2*>(DepthStencilAttachmentReference);
	}

	void SetInputAttachments(FVulkanAttachmentReference<VkAttachmentReference2>* InputAttachmentReferences, TUINT32 NumInputAttachmentReferences)
	{
		pInputAttachments = static_cast<VkAttachmentReference2*>(InputAttachmentReferences);
		inputAttachmentCount = NumInputAttachmentReferences;
	}

	void SetDepthStencilResolveAttachment(VkSubpassDescriptionDepthStencilResolveKHR* DepthStencilResolveAttachmentDesc)
	{
		const void* Next = pNext;
		pNext = DepthStencilResolveAttachmentDesc;
		DepthStencilResolveAttachmentDesc->pNext = Next;
	}

	void SetShadingRateAttachment(VkFragmentShadingRateAttachmentInfoKHR* ShadingRateAttachmentInfo)
	{
		const void* Next = pNext;
		pNext = ShadingRateAttachmentInfo;
		ShadingRateAttachmentInfo->pNext = Next;
	}

	void SetMultiViewMask(TUINT32 Mask)
	{
		viewMask = Mask;
	}
};

template <typename TSubpassDependencyType>
struct FVulkanSubpassDependency
	: public TSubpassDependencyType
{
};

template<>
struct FVulkanSubpassDependency<VkSubpassDependency>
	: public VkSubpassDependency
{
	FVulkanSubpassDependency()
	{
		FMemory::Memzero(this, sizeof(VkSubpassDependency));
	}
};

template<>
struct FVulkanSubpassDependency<VkSubpassDependency2>
	: public VkSubpassDependency2
{
	FVulkanSubpassDependency()
	{
		ZeroVulkanStruct(this, VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2);
		viewOffset = 0;       // According to the Vulkan spec: "If dependencyFlags does not include VK_DEPENDENCY_VIEW_LOCAL_BIT, viewOffset must be 0"
	}
};

template<typename TAttachmentDescriptionType>
struct FVulkanAttachmentDescription
{
};

template<>
struct FVulkanAttachmentDescription<VkAttachmentDescription>
	: public VkAttachmentDescription
{
	FVulkanAttachmentDescription()
	{
		FMemory::Memzero(this, sizeof(VkAttachmentDescription));
	}

	FVulkanAttachmentDescription(const VkAttachmentDescription& InDesc)
	{
		flags = InDesc.flags;
		format = InDesc.format;
		samples = InDesc.samples;
		loadOp = InDesc.loadOp;
		storeOp = InDesc.storeOp;
		stencilLoadOp = InDesc.stencilLoadOp;
		stencilStoreOp = InDesc.stencilStoreOp;
		initialLayout = InDesc.initialLayout;
		finalLayout = InDesc.finalLayout;
	}

	FVulkanAttachmentDescription(const VkAttachmentDescription& InDesc, const VkAttachmentDescriptionStencilLayout* InStencilDesc, bool /*bSupportsParallelRendering*/)
	{
		flags = InDesc.flags;
		format = InDesc.format;
		samples = InDesc.samples;
		loadOp = InDesc.loadOp;
		storeOp = InDesc.storeOp;
		stencilLoadOp = InDesc.stencilLoadOp;
		stencilStoreOp = InDesc.stencilStoreOp;

		const bool bHasStencilLayout = VulkanRHI::VulkanFormatHasStencil(InDesc.format) && (InStencilDesc != nullptr);
		const VkImageLayout StencilInitialLayout = bHasStencilLayout ? InStencilDesc->stencilInitialLayout : VK_IMAGE_LAYOUT_UNDEFINED;
		initialLayout = VulkanRHI::VulkanGetMergedDepthStencilLayout(InDesc.initialLayout, StencilInitialLayout);
		const VkImageLayout StencilFinalLayout = bHasStencilLayout ? InStencilDesc->stencilFinalLayout : VK_IMAGE_LAYOUT_UNDEFINED;
		finalLayout = VulkanRHI::VulkanGetMergedDepthStencilLayout(InDesc.finalLayout, StencilFinalLayout);
	}
};

template<>
struct FVulkanAttachmentDescription<VkAttachmentDescription2>
	: public VkAttachmentDescription2
{
	FVulkanAttachmentDescription()
	{
		ZeroVulkanStruct(this, VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2);
	}

	FVulkanAttachmentDescription(const VkAttachmentDescription& InDesc)
	{
		sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
		pNext = nullptr;
		flags = InDesc.flags;
		format = InDesc.format;
		samples = InDesc.samples;
		loadOp = InDesc.loadOp;
		storeOp = InDesc.storeOp;
		stencilLoadOp = InDesc.stencilLoadOp;
		stencilStoreOp = InDesc.stencilStoreOp;
		initialLayout = InDesc.initialLayout;
		finalLayout = InDesc.finalLayout;
	}

	FVulkanAttachmentDescription(const VkAttachmentDescription& InDesc, const VkAttachmentDescriptionStencilLayout* InStencilDesc, bool bSupportsParallelRendering)
	{
		const bool bHasStencilLayout = bSupportsParallelRendering && VulkanRHI::VulkanFormatHasStencil(InDesc.format) && (InStencilDesc != nullptr);

		sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
		pNext = (bHasStencilLayout && (InStencilDesc->stencilFinalLayout != VK_IMAGE_LAYOUT_UNDEFINED)) ? InStencilDesc : nullptr;
		flags = InDesc.flags;
		format = InDesc.format;
		samples = InDesc.samples;
		loadOp = InDesc.loadOp;
		storeOp = InDesc.storeOp;
		stencilLoadOp = InDesc.stencilLoadOp;
		stencilStoreOp = InDesc.stencilStoreOp;
		initialLayout = bSupportsParallelRendering ? InDesc.initialLayout : VulkanRHI::VulkanGetMergedDepthStencilLayout(InDesc.initialLayout, InStencilDesc->stencilInitialLayout);
		finalLayout = bSupportsParallelRendering ? InDesc.finalLayout : VulkanRHI::VulkanGetMergedDepthStencilLayout(InDesc.finalLayout, InStencilDesc->stencilFinalLayout);
	}
};

template <typename T>
struct FVulkanRenderPassCreateInfo
{};

template<>
struct FVulkanRenderPassCreateInfo<VkRenderPassCreateInfo>
	: public VkRenderPassCreateInfo
{
	FVulkanRenderPassCreateInfo()
	{
		ZeroVulkanStruct(this, VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO);
	}

	void SetCorrelationMask(const TUINT32* /*MaskPtr*/)
	{
		// No-op without VK_KHR_create_renderpass2
	}

	VkRenderPass Create(const FVulkanLogicalDeviceRef& Device)
	{
		VkRenderPass Handle = VK_NULL_HANDLE;
		VULKAN_ENSURE(VulkanRHI::vkCreateRenderPass(Device->GetHandle(), this, VulkanRHI::VULKAN_CPU_ALLOCATOR, &Handle));
		return Handle;
	}
};

struct FVulkanRenderPassFragmentDensityMapCreateInfoEXT
	: public VkRenderPassFragmentDensityMapCreateInfoEXT
{
	FVulkanRenderPassFragmentDensityMapCreateInfoEXT()
	{
		ZeroVulkanStruct(this, VK_STRUCTURE_TYPE_RENDER_PASS_FRAGMENT_DENSITY_MAP_CREATE_INFO_EXT);
	}
};

struct FVulkanRenderPassMultiviewCreateInfo
	: public VkRenderPassMultiviewCreateInfo
{
	FVulkanRenderPassMultiviewCreateInfo()
	{
		ZeroVulkanStruct(this, VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO);
	}
};

template<>
struct FVulkanRenderPassCreateInfo<VkRenderPassCreateInfo2>
	: public VkRenderPassCreateInfo2
{
	FVulkanRenderPassCreateInfo()
	{
		ZeroVulkanStruct(this, VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2);
	}

	void SetCorrelationMask(const TUINT32* MaskPtr)
	{
		correlatedViewMaskCount = 1;
		pCorrelatedViewMasks = MaskPtr;
	}

	// Require extension to logical device creation
//	VkRenderPass Create(FVulkanLogicalDeviceRef Device)
//	{
//		VkRenderPass Handle = VK_NULL_HANDLE;
//		VULKAN_ENSURE(VulkanRHI::vkCreateRenderPass2KHR(Device->GetHandle(), this, VulkanRHI::VULKAN_CPU_ALLOCATOR, &Handle));
//		return Handle;
//	}
};

struct FVulkanFragmentShadingRateAttachmentInfo
	: public VkFragmentShadingRateAttachmentInfoKHR
{
	FVulkanFragmentShadingRateAttachmentInfo()
	{
		ZeroVulkanStruct(this, VK_STRUCTURE_TYPE_FRAGMENT_SHADING_RATE_ATTACHMENT_INFO_KHR);
		// For now, just use the smallest tile-Num available. TODO: Add a setting to allow prioritizing either higher resolution/larger shading rate attachment targets
		// or lower-resolution/smaller attachments.
		// TODO
		//shadingRateAttachmentTexelSize = { (TUINT32)GRHIVariableRateShadingImageTileMinWidth, (TUINT32)GRHIVariableRateShadingImageTileMinHeight };
	}

	void SetReference(FVulkanAttachmentReference<VkAttachmentReference2>* AttachmentReference)
	{
		pFragmentShadingRateAttachment = AttachmentReference;
	}
};

struct FVulkanDepthStencilResolveSubpassDesc
	: public VkSubpassDescriptionDepthStencilResolveKHR
{
	FVulkanDepthStencilResolveSubpassDesc()
	{
		ZeroVulkanStruct(this, VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE);
	}

	void SetResolveModes(VkResolveModeFlagBits DepthMode, VkResolveModeFlagBits StencilMode)
	{
		depthResolveMode = DepthMode;
		stencilResolveMode = StencilMode;
	}

	void SetReference(FVulkanAttachmentReference<VkAttachmentReference2>* AttachmentReference)
	{
		pDepthStencilResolveAttachment = AttachmentReference;
	}
};
