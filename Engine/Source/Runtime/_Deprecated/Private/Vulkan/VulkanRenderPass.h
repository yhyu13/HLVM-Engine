/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#pragma once

#include "Core/Container/ContainerDefinition.h"
#include "Renderer/RHI/_Deprecated/RHIResource.h"
#include "VulkanResourcePost.h"

class FVulkanRenderPass : public FRHIRenderPass, public FVulkanResource, public FVulkanMinimalContext
{
public:
	FVulkanRenderPass(const FVulkanRenderTargetLayout& RTLayout);
	~FVulkanRenderPass() override;

	HLVM_INLINE_FUNC const FVulkanRenderTargetLayout& GetLayout() const
	{
		return Layout;
	}

	HLVM_INLINE_FUNC VkRenderPass GetHandle() const
	{
		return RenderPass;
	}

	HLVM_INLINE_FUNC TUINT32 GetNumUsedClearValues() const
	{
		return Layout.GetNumUsedClearValues();
	}

private:
	FVulkanRenderTargetLayout Layout;
	VkRenderPass			  RenderPass;
};
using FVulkanRenderPassRef = TRefCountPtr<FVulkanRenderPass>;

template <typename TAttachmentReferenceType>
struct FVulkanAttachmentReference : public TAttachmentReferenceType
{
	FVulkanAttachmentReference()
	{
		ZeroStruct();
	}

	FVulkanAttachmentReference(const VkAttachmentReference& AttachmentReferenceIn, VkImageAspectFlags AspectMask)
	{
		SetAttachment(AttachmentReferenceIn, AspectMask);
	}

	HLVM_INLINE_FUNC void SetAttachment(const VkAttachmentReference& /*AttachmentReferenceIn*/, VkImageAspectFlags /*AspectMask*/) { HLVM_NOT_IMPLEMENTED(); }
	HLVM_INLINE_FUNC void SetAttachment(const FVulkanAttachmentReference<TAttachmentReferenceType>& AttachmentReferenceIn, VkImageAspectFlags /*AspectMask*/) { *this = AttachmentReferenceIn; }
	HLVM_INLINE_FUNC void SetDepthStencilAttachment(const VkAttachmentReference& /*AttachmentReferenceIn*/, const VkAttachmentReferenceStencilLayout* /*StencilReference*/, VkImageAspectFlags /*AspectMask*/, bool /*bSupportsParallelRendering*/) { HLVM_NOT_IMPLEMENTED(); }
	HLVM_INLINE_FUNC void ZeroStruct() {}
	HLVM_INLINE_FUNC void SetAspect(TUINT32 /*Aspect*/) {}
};

template <>
HLVM_INLINE_FUNC void FVulkanAttachmentReference<VkAttachmentReference>::SetAttachment(const VkAttachmentReference& AttachmentReferenceIn, VkImageAspectFlags /*AspectMask*/)
{
	attachment = AttachmentReferenceIn.attachment;
	layout = AttachmentReferenceIn.layout;
}

template <>
HLVM_INLINE_FUNC void FVulkanAttachmentReference<VkAttachmentReference2>::SetAttachment(const VkAttachmentReference& AttachmentReferenceIn, VkImageAspectFlags AspectMask)
{
	sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
	pNext = nullptr;
	attachment = AttachmentReferenceIn.attachment;
	layout = AttachmentReferenceIn.layout;
	aspectMask = AspectMask;
}

template <>
HLVM_INLINE_FUNC void FVulkanAttachmentReference<VkAttachmentReference2>::SetAttachment(const FVulkanAttachmentReference<VkAttachmentReference2>& AttachmentReferenceIn, VkImageAspectFlags AspectMask)
{
	sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
	pNext = nullptr;
	attachment = AttachmentReferenceIn.attachment;
	layout = AttachmentReferenceIn.layout;
	aspectMask = AspectMask;
}

template <>
HLVM_INLINE_FUNC void FVulkanAttachmentReference<VkAttachmentReference>::SetDepthStencilAttachment(const VkAttachmentReference& AttachmentReferenceIn,
	const VkAttachmentReferenceStencilLayout*																					StencilReference, VkImageAspectFlags /*AspectMask*/, bool /*bSupportsParallelRendering*/)
{
	attachment = AttachmentReferenceIn.attachment;
	const VkImageLayout StencilLayout = StencilReference ? StencilReference->stencilLayout : VK_IMAGE_LAYOUT_UNDEFINED;
	layout = VulkanRHI::VulkanGetMergedDepthStencilLayout(AttachmentReferenceIn.layout, StencilLayout);
}

template <>
HLVM_INLINE_FUNC void FVulkanAttachmentReference<VkAttachmentReference2>::SetDepthStencilAttachment(const VkAttachmentReference& AttachmentReferenceIn,
	const VkAttachmentReferenceStencilLayout* StencilReference, VkImageAspectFlags AspectMask, bool bSupportsParallelRendering)
{
	sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
	pNext = (bSupportsParallelRendering && StencilReference && StencilReference->stencilLayout != VK_IMAGE_LAYOUT_UNDEFINED) ? StencilReference : nullptr;
	attachment = AttachmentReferenceIn.attachment;
	layout = bSupportsParallelRendering ? AttachmentReferenceIn.layout : VulkanRHI::VulkanGetMergedDepthStencilLayout(AttachmentReferenceIn.layout, StencilReference->stencilLayout);
	aspectMask = AspectMask;
}

template <>
HLVM_INLINE_FUNC void FVulkanAttachmentReference<VkAttachmentReference>::ZeroStruct()
{
	attachment = 0;
	layout = VK_IMAGE_LAYOUT_UNDEFINED;
}

template <>
HLVM_INLINE_FUNC void FVulkanAttachmentReference<VkAttachmentReference2>::ZeroStruct()
{
	sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
	pNext = nullptr;
	attachment = 0;
	layout = VK_IMAGE_LAYOUT_UNDEFINED;
	aspectMask = 0;
}

template <>
HLVM_INLINE_FUNC void FVulkanAttachmentReference<VkAttachmentReference2>::SetAspect(TUINT32 Aspect)
{
	aspectMask = Aspect;
}

template <typename TSubpassDescriptionType>
class FVulkanSubpassDescription
{
};

template <>
struct FVulkanSubpassDescription<VkSubpassDescription> : public VkSubpassDescription
{
	FVulkanSubpassDescription()
	{
		FMemory::Memzero(this, sizeof(VkSubpassDescription));
		pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	}

	void SetColorAttachments(const TVectorView<FVulkanAttachmentReference<VkAttachmentReference>>& ColorAttachmentReferences, TUINT32 OverrideCount = 0)
	{
		colorAttachmentCount = (OverrideCount == 0) ? ColorAttachmentReferences.Num() : OverrideCount;
		pColorAttachments = ColorAttachmentReferences.GetDataConst();
	}

	void SetResolveAttachments(const TVectorView<FVulkanAttachmentReference<VkAttachmentReference>>& ResolveAttachmentReferences)
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

template <>
struct FVulkanSubpassDescription<VkSubpassDescription2> : public VkSubpassDescription2
{
	FVulkanSubpassDescription()
	{
		VulkanRHI::ZeroVulkanStruct(this, VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2);
		pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		viewMask = 0;
	}

	void SetColorAttachments(const TVectorView<FVulkanAttachmentReference<VkAttachmentReference2>>& ColorAttachmentReferences, TUINT32 OverrideCount = 0)
	{
		colorAttachmentCount = (OverrideCount == 0) ? ColorAttachmentReferences.Num() : OverrideCount;
		pColorAttachments = ColorAttachmentReferences.GetDataConst();
	}

	void SetResolveAttachments(const TVectorView<FVulkanAttachmentReference<VkAttachmentReference2>>& ResolveAttachmentReferences)
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
};

template <typename TSubpassDependencyType>
struct FVulkanSubpassDependency : public TSubpassDependencyType
{
};

template <>
struct FVulkanSubpassDependency<VkSubpassDependency> : public VkSubpassDependency
{
	FVulkanSubpassDependency()
	{
		FMemory::Memzero(this, sizeof(VkSubpassDependency));
	}
};

template <>
struct FVulkanSubpassDependency<VkSubpassDependency2> : public VkSubpassDependency2
{
	FVulkanSubpassDependency()
	{
		VulkanRHI::ZeroVulkanStruct(this, VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2);
		viewOffset = 0; // According to the Vulkan spec: "If dependencyFlags does not include VK_DEPENDENCY_VIEW_LOCAL_BIT, viewOffset must be 0"
	}
};

template <typename TAttachmentDescriptionType>
struct FVulkanAttachmentDescription
{
};

template <>
struct FVulkanAttachmentDescription<VkAttachmentDescription> : public VkAttachmentDescription
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

		const bool			bHasStencilLayout = VulkanRHI::VulkanFormatHasStencil(InDesc.format) && (InStencilDesc != nullptr);
		const VkImageLayout StencilInitialLayout = bHasStencilLayout ? InStencilDesc->stencilInitialLayout : VK_IMAGE_LAYOUT_UNDEFINED;
		initialLayout = VulkanRHI::VulkanGetMergedDepthStencilLayout(InDesc.initialLayout, StencilInitialLayout);
		const VkImageLayout StencilFinalLayout = bHasStencilLayout ? InStencilDesc->stencilFinalLayout : VK_IMAGE_LAYOUT_UNDEFINED;
		finalLayout = VulkanRHI::VulkanGetMergedDepthStencilLayout(InDesc.finalLayout, StencilFinalLayout);
	}
};

template <>
struct FVulkanAttachmentDescription<VkAttachmentDescription2> : public VkAttachmentDescription2
{
	FVulkanAttachmentDescription()
	{
		VulkanRHI::ZeroVulkanStruct(this, VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2);
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
{
};

template <>
struct FVulkanRenderPassCreateInfo<VkRenderPassCreateInfo> : public VkRenderPassCreateInfo
{
	FVulkanRenderPassCreateInfo()
	{
		VulkanRHI::ZeroVulkanStruct(this, VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO);
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

struct FVulkanRenderPassFragmentDensityMapCreateInfoEXT : public VkRenderPassFragmentDensityMapCreateInfoEXT
{
	FVulkanRenderPassFragmentDensityMapCreateInfoEXT()
	{
		VulkanRHI::ZeroVulkanStruct(this, VK_STRUCTURE_TYPE_RENDER_PASS_FRAGMENT_DENSITY_MAP_CREATE_INFO_EXT);
	}
};

struct FVulkanRenderPassMultiviewCreateInfo : public VkRenderPassMultiviewCreateInfo
{
	FVulkanRenderPassMultiviewCreateInfo()
	{
		VulkanRHI::ZeroVulkanStruct(this, VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO);
	}
};

#if VULKAN_RENDERPASS2
template <>
struct FVulkanRenderPassCreateInfo<VkRenderPassCreateInfo2> : public VkRenderPassCreateInfo2
{
	FVulkanRenderPassCreateInfo()
	{
		VulkanRHI::ZeroVulkanStruct(this, VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2);
	}

	void SetCorrelationMask(const TUINT32* MaskPtr)
	{
		correlatedViewMaskCount = 1;
		pCorrelatedViewMasks = MaskPtr;
	}

	VkRenderPass Create(FVulkanLogicalDeviceRef Device)
	{
		VkRenderPass Handle;
		VULKAN_ENSURE(VulkanRHI::vkCreateRenderPass2KHR(Device->GetHandle(), this, VulkanRHI::VULKAN_CPU_ALLOCATOR, &Handle));
		return Handle;
	}
};
#endif

struct FVulkanFragmentShadingRateAttachmentInfo : public VkFragmentShadingRateAttachmentInfoKHR
{
	FVulkanFragmentShadingRateAttachmentInfo()
	{
		VulkanRHI::ZeroVulkanStruct(this, VK_STRUCTURE_TYPE_FRAGMENT_SHADING_RATE_ATTACHMENT_INFO_KHR);
	}

	void SetReference(FVulkanAttachmentReference<VkAttachmentReference2>* AttachmentReference)
	{
		pFragmentShadingRateAttachment = AttachmentReference;
	}
};

struct FVulkanDepthStencilResolveSubpassDesc : public VkSubpassDescriptionDepthStencilResolveKHR
{
	FVulkanDepthStencilResolveSubpassDesc()
	{
		VulkanRHI::ZeroVulkanStruct(this, VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE);
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

template <typename TSubpassDescriptionClass, typename TSubpassDependencyClass, typename TAttachmentReferenceClass, typename TAttachmentDescriptionClass, typename TRenderPassCreateInfoClass>
class FVulkanRenderPassBuilder
{
public:
	FVulkanRenderPassBuilder(FVulkanLogicalDeviceRef InDevice)
		: Device(InDevice)
		, CorrelationMask(0)
	{
	}

	void BuildCreateInfo(const FVulkanRenderTargetLayout& RTLayout)
	{
		TUINT32 NumSubpasses = 0;
		TUINT32 NumDependencies = 0;

		// 0b11 for 2, 0b1111 for 4, and so on
		TUINT32 MultiviewMask = 0; // TODO : make render pass support multiview?

		const bool bDeferredShadingSubpass = RTLayout.GetSubpassHint() == ESubpassHint::DeferredShading;
		//		const bool bApplyFragmentShadingRate = GRHISupportsAttachmentVariableRateShading
		//			&& RTLayout.GetFragmentDensityAttachmentReference() != nullptr
		//			&& Device.GetOptionalExtensions().HasKHRFragmentShadingRate
		//			&& Device.GetOptionalExtensionProperties().FragmentShadingRateFeatures.attachmentFragmentShadingRate == VK_TRUE;
		//		const bool bResolveDepth = GRHISupportsDepthStencilResolve && Device.GetOptionalExtensions().HasKHRDepthStencilResolve && RTLayout.GetHasDepthStencilResolve();
		const bool bApplyFragmentShadingRate = false;
		const bool bResolveDepth = false;
		const bool bCustomResolveSubpass = RTLayout.GetSubpassHint() == ESubpassHint::CustomResolve;
		const bool bDepthReadSubpass = bCustomResolveSubpass || (RTLayout.GetSubpassHint() == ESubpassHint::DepthReading);
		const bool bHasDepthStencilAttachmentReference = (RTLayout.GetDepthAttachmentReference() != nullptr);

		if (bApplyFragmentShadingRate)
		{
			ShadingRateAttachmentReference.SetAttachment(*RTLayout.GetFragmentDensityAttachmentReference(), VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT);
			FragmentShadingRateAttachmentInfo.SetReference(&ShadingRateAttachmentReference);
		}

		if (bResolveDepth)
		{
			DepthStencilResolveAttachmentReference.SetAttachment(*RTLayout.GetDepthStencilResolveAttachmentReference(), VkImageAspectFlagBits::VK_IMAGE_ASPECT_NONE);
			// Using zero bit because it is always supported if the extension is supported, from spec: "The VK_RESOLVE_MODE_SAMPLE_ZERO_BIT mode
			// is the only mode that is required of all implementations (that support the extension or support Vulkan 1.2 or higher)."
			DepthStencilResolveSubpassDesc.SetResolveModes(VK_RESOLVE_MODE_SAMPLE_ZERO_BIT, VK_RESOLVE_MODE_SAMPLE_ZERO_BIT);
			DepthStencilResolveSubpassDesc.SetReference(&DepthStencilResolveAttachmentReference);
		}

		// Grab (and optionally convert) attachment references.
		TUINT32 NumColorAttachments = RTLayout.GetNumColorAttachments();
		for (TUINT32 ColorAttachment = 0; ColorAttachment < NumColorAttachments; ++ColorAttachment)
		{
			ColorAttachmentReferences.Add(TAttachmentReferenceClass(RTLayout.GetColorAttachmentReferences()[ColorAttachment], 0));
			if (RTLayout.GetResolveAttachmentReferences() != nullptr)
			{
				ResolveAttachmentReferences.Add(TAttachmentReferenceClass(RTLayout.GetResolveAttachmentReferences()[ColorAttachment], 0));
			}
		}

		// CustomResolveSubpass has an additional color attachment that should not be used by main and depth subpasses
		if (bCustomResolveSubpass && (NumColorAttachments > 1))
		{
			NumColorAttachments--;
		}

		TUINT32			   DepthInputAttachment = VK_ATTACHMENT_UNUSED;
		VkImageLayout	   DepthInputAttachmentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		VkImageAspectFlags DepthInputAspectMask = 0;
		if (bHasDepthStencilAttachmentReference)
		{
			DepthStencilAttachmentReference.SetDepthStencilAttachment(*RTLayout.GetDepthAttachmentReference(), RTLayout.GetStencilAttachmentReference(), 0, Device->SupportsParallelRendering());

			if (bDepthReadSubpass || bDeferredShadingSubpass)
			{
				DepthStencilAttachment.attachment = RTLayout.GetDepthAttachmentReference()->attachment;
				DepthStencilAttachment.SetAspect(VK_IMAGE_ASPECT_DEPTH_BIT); // @todo?

				// FIXME: checking a Depth layout is not correct in all cases
				// PSO cache can create a PSO for subpass 1 or 2 first, where depth is read-only but that does not mean depth pre-pass is enabled
				if (false && RTLayout.GetDepthAttachmentReference()->layout == VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL)
				{
					// Depth is read only and is expected to be sampled as a regular texture
					DepthStencilAttachment.layout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
				}
				else
				{
					// lights write to stencil for culling, so stencil is expected to be writebale while depth is read-only
					DepthStencilAttachment.layout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL;
					DepthInputAttachment = DepthStencilAttachment.attachment;
					DepthInputAttachmentLayout = DepthStencilAttachment.layout;
					DepthInputAspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
				}
			}
		}

		// main sub-pass
		{
			TSubpassDescriptionClass& SubpassDesc = SubpassDescriptions[NumSubpasses++];

			SubpassDesc.SetColorAttachments(ColorAttachmentReferences, NumColorAttachments);

			if (bHasDepthStencilAttachmentReference)
			{
				SubpassDesc.SetDepthStencilAttachment(&DepthStencilAttachmentReference);
			}

			if (!bDepthReadSubpass && bResolveDepth)
			{
				SubpassDesc.SetDepthStencilResolveAttachment(&DepthStencilResolveSubpassDesc);
			}

			if (bApplyFragmentShadingRate)
			{
				SubpassDesc.SetShadingRateAttachment(&FragmentShadingRateAttachmentInfo);
			}
			SubpassDesc.SetMultiViewMask(MultiviewMask);
		}

		// Color write and depth read sub-pass
		if (bDepthReadSubpass)
		{
			TSubpassDescriptionClass& SubpassDesc = SubpassDescriptions[NumSubpasses++];

			SubpassDesc.SetColorAttachments(ColorAttachmentReferences, 1);

			HLVM_ASSERT(RTLayout.GetDepthAttachmentReference());

			// Depth as Input0
			InputAttachments1[0].attachment = DepthInputAttachment;
			InputAttachments1[0].layout = DepthInputAttachmentLayout;
			InputAttachments1[0].SetAspect(DepthInputAspectMask);
			SubpassDesc.SetInputAttachments(InputAttachments1, InputAttachment1Count);
			// depth attachment is same as input attachment
			SubpassDesc.SetDepthStencilAttachment(&DepthStencilAttachment);

			if (bResolveDepth && !bCustomResolveSubpass)
			{
				SubpassDesc.SetDepthStencilResolveAttachment(&DepthStencilResolveSubpassDesc);
			}

			if (bApplyFragmentShadingRate)
			{
				SubpassDesc.SetShadingRateAttachment(&FragmentShadingRateAttachmentInfo);
			}
			SubpassDesc.SetMultiViewMask(MultiviewMask);

			TSubpassDependencyClass& SubpassDep = SubpassDependencies[NumDependencies++];
			SubpassDep.srcSubpass = 0;
			SubpassDep.dstSubpass = 1;
			SubpassDep.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			SubpassDep.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			SubpassDep.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			SubpassDep.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
			SubpassDep.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
		}

		// Two subpasses for deferred shading
		if (bDeferredShadingSubpass)
		{
			// const VkAttachmentReference* ColorRef = RTLayout.GetColorAttachmentReferences();
			// TUINT32 NumColorAttachments = RTLayout.GetNumColorAttachments();
			// HLVM_ASSERT(RTLayout.GetNumColorAttachments() == 5); //current layout is SceneColor, GBufferA/B/C/D

			// 1. Write to SceneColor and GBuffer, input DepthStencil
			{
				TSubpassDescriptionClass& SubpassDesc = SubpassDescriptions[NumSubpasses++];
				SubpassDesc.SetColorAttachments(ColorAttachmentReferences);
				SubpassDesc.SetDepthStencilAttachment(&DepthStencilAttachment);
				InputAttachments1[0].attachment = DepthInputAttachment;
				InputAttachments1[0].layout = DepthInputAttachmentLayout;
				InputAttachments1[0].SetAspect(DepthInputAspectMask);
				SubpassDesc.SetInputAttachments(InputAttachments1, InputAttachment1Count);

				if (bApplyFragmentShadingRate)
				{
					SubpassDesc.SetShadingRateAttachment(&FragmentShadingRateAttachmentInfo);
				}
				SubpassDesc.SetMultiViewMask(MultiviewMask);

				// Depth as Input0
				TSubpassDependencyClass& SubpassDep = SubpassDependencies[NumDependencies++];
				SubpassDep.srcSubpass = 0;
				SubpassDep.dstSubpass = 1;
				SubpassDep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
				SubpassDep.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
				SubpassDep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
				SubpassDep.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
				SubpassDep.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
			}

			// 2. Write to SceneColor, input GBuffer and DepthStencil
			{
				TSubpassDescriptionClass& SubpassDesc = SubpassDescriptions[NumSubpasses++];
				SubpassDesc.SetColorAttachments(ColorAttachmentReferences, 1); // SceneColor only
				SubpassDesc.SetDepthStencilAttachment(&DepthStencilAttachment);

				// Depth as Input0
				InputAttachments2[0].attachment = DepthInputAttachment;
				InputAttachments2[0].layout = DepthInputAttachmentLayout;
				InputAttachments2[0].SetAspect(DepthInputAspectMask);

				// SceneColor write only
				InputAttachments2[1].attachment = VK_ATTACHMENT_UNUSED;
				InputAttachments2[1].layout = VK_IMAGE_LAYOUT_UNDEFINED;
				InputAttachments2[1].SetAspect(0);

				// GBufferA/B/C/D as Input2/3/4/5
				TUINT32 NumColorInputs = ColorAttachmentReferences.Num() - 1;
				for (TUINT32 i = 2; i < (NumColorInputs + 2); ++i)
				{
					InputAttachments2[i].attachment = ColorAttachmentReferences[i - 1].attachment;
					InputAttachments2[i].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
					InputAttachments2[i].SetAspect(VK_IMAGE_ASPECT_COLOR_BIT);
				}

				SubpassDesc.SetInputAttachments(InputAttachments2, NumColorInputs + 2);
				if (bApplyFragmentShadingRate)
				{
					SubpassDesc.SetShadingRateAttachment(&FragmentShadingRateAttachmentInfo);
				}
				SubpassDesc.SetMultiViewMask(MultiviewMask);

				TSubpassDependencyClass& SubpassDep = SubpassDependencies[NumDependencies++];
				SubpassDep.srcSubpass = 1;
				SubpassDep.dstSubpass = 2;
				SubpassDep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
				SubpassDep.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
				SubpassDep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				SubpassDep.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
#if VULKAN_INPUT_ATTACHMENT_SHADER_READ
				{
					// this is not required, but might flicker on some devices without
					SubpassDep.dstAccessMask |= VK_ACCESS_SHADER_READ_BIT;
				}
#endif
				SubpassDep.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
			}
		}

		// Custom resolve subpass
		if (bCustomResolveSubpass)
		{
			TSubpassDescriptionClass& SubpassDesc = SubpassDescriptions[NumSubpasses++];
			ColorAttachments3[0].attachment = ColorAttachmentReferences[1].attachment;
			ColorAttachments3[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			ColorAttachments3[0].SetAspect(VK_IMAGE_ASPECT_COLOR_BIT);

			InputAttachments3[0].attachment = VK_ATTACHMENT_UNUSED; // The subpass fetch logic expects depth in first attachment.
			InputAttachments3[0].layout = VK_IMAGE_LAYOUT_UNDEFINED;
			InputAttachments3[0].SetAspect(0);

			InputAttachments3[1].attachment = ColorAttachmentReferences[0].attachment; // SceneColor as input
			InputAttachments3[1].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			InputAttachments3[1].SetAspect(VK_IMAGE_ASPECT_COLOR_BIT);

			SubpassDesc.SetDepthStencilAttachment(&DepthStencilAttachment);

			if (bResolveDepth)
			{
				SubpassDesc.SetDepthStencilResolveAttachment(&DepthStencilResolveSubpassDesc);
			}

			SubpassDesc.SetInputAttachments(InputAttachments3, 2);
			SubpassDesc.colorAttachmentCount = 1;
			SubpassDesc.pColorAttachments = ColorAttachments3;

			SubpassDesc.SetMultiViewMask(MultiviewMask);

			TSubpassDependencyClass& SubpassDep = SubpassDependencies[NumDependencies++];
			SubpassDep.srcSubpass = 1;
			SubpassDep.dstSubpass = 2;
			SubpassDep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			SubpassDep.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			SubpassDep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			SubpassDep.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
			SubpassDep.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
		}

		// CustomResolveSubpass does a custom resolve into second color target and does not need resolve attachments
		if (!bCustomResolveSubpass)
		{
			if (bDepthReadSubpass && ResolveAttachmentReferences.Num() > 1)
			{
				// Handle SceneDepthAux resolve:
				// The depth read subpass has only a single color attachment (using more would not be compatible dual source blending), so make SceneDepthAux a resolve attachment of the first subpass instead
				ResolveAttachmentReferences.Add(TAttachmentReferenceClass{});
				ResolveAttachmentReferences.LastData()->attachment = VK_ATTACHMENT_UNUSED;
				ResolveAttachmentReferences.Swap(0, ResolveAttachmentReferences.Num() - 1);

				SubpassDescriptions[0].SetResolveAttachments(TVectorView<TAttachmentReferenceClass>(ResolveAttachmentReferences.GetData(), ResolveAttachmentReferences.Num() - 1));
				SubpassDescriptions[NumSubpasses - 1].SetResolveAttachments(TVectorView<TAttachmentReferenceClass>(ResolveAttachmentReferences.LastData(), 1));
			}
			else
			{
				// Only set resolve attachment on the last subpass
				SubpassDescriptions[NumSubpasses - 1].SetResolveAttachments(ResolveAttachmentReferences);
			}
		}

		for (TUINT32 Attachment = 0; Attachment < RTLayout.GetNumAttachmentDescriptions(); ++Attachment)
		{
			if (bHasDepthStencilAttachmentReference && (Attachment == DepthStencilAttachmentReference.attachment))
			{
				AttachmentDescriptions.Add(TAttachmentDescriptionClass(RTLayout.GetAttachmentDescriptions()[Attachment], RTLayout.GetStencilDesc(), Device->SupportsParallelRendering()));
			}
			else
			{
				AttachmentDescriptions.Add(TAttachmentDescriptionClass(RTLayout.GetAttachmentDescriptions()[Attachment]));
			}
		}

		CreateInfo.attachmentCount = AttachmentDescriptions.Num();
		CreateInfo.pAttachments = AttachmentDescriptions.GetData();
		CreateInfo.subpassCount = NumSubpasses;
		CreateInfo.pSubpasses = SubpassDescriptions;
		CreateInfo.dependencyCount = NumDependencies;
		CreateInfo.pDependencies = SubpassDependencies;

		/*
		Bit mask that specifies which view rendering is broadcast to
		0011 = Broadcast to first and second view (layer)
		*/
		ViewMask[0] = MultiviewMask;
		ViewMask[1] = MultiviewMask;

		/*
		Bit mask that specifices correlation between views
		An implementation may use this for optimizations (concurrent render)
		*/
		CorrelationMask = MultiviewMask;

		// if (RTLayout.GetIsMultiView())
		if (0)
		{
			// TODO
			// if (Device.GetOptionalExtensions().HasKHRRenderPass2)
			if (0)
			{
				CreateInfo.SetCorrelationMask(&CorrelationMask);
			}
			else
			{
				// TODO
				// checkf(Device.GetOptionalExtensions().HasKHRMultiview, TEXT("Layout is multiview but extension is not supported!"));
				MultiviewInfo.subpassCount = NumSubpasses;
				MultiviewInfo.pViewMasks = ViewMask;
				MultiviewInfo.dependencyCount = 0;
				MultiviewInfo.pViewOffsets = nullptr;
				MultiviewInfo.correlationMaskCount = 1;
				MultiviewInfo.pCorrelationMasks = &CorrelationMask;

				MultiviewInfo.pNext = CreateInfo.pNext;
				CreateInfo.pNext = &MultiviewInfo;
			}
		}

		// TODO
		// if (Device.GetOptionalExtensions().HasEXTFragmentDensityMap && RTLayout.GetHasFragmentDensityAttachment())
		if (0)
		{
			FragDensityCreateInfo.fragmentDensityMapAttachment = *RTLayout.GetFragmentDensityAttachmentReference();

			// Chain fragment density info onto create info and the rest of the pNexts
			// onto the fragment density info
			FragDensityCreateInfo.pNext = CreateInfo.pNext;
			CreateInfo.pNext = &FragDensityCreateInfo;
		}
	}

	VkRenderPass Create(const FVulkanRenderTargetLayout& RTLayout)
	{
		BuildCreateInfo(RTLayout);
		return CreateInfo.Create(Device);
	}

	TRenderPassCreateInfoClass& GetCreateInfo()
	{
		return CreateInfo;
	}

private:
	TSubpassDescriptionClass SubpassDescriptions[8];
	TSubpassDependencyClass	 SubpassDependencies[8];

	TVector<TAttachmentReferenceClass> ColorAttachmentReferences;
	TVector<TAttachmentReferenceClass> ResolveAttachmentReferences;

	// Color write and depth read sub-pass
	static const TUINT32	  InputAttachment1Count = 1;
	TAttachmentReferenceClass InputAttachments1[InputAttachment1Count];

	// Two subpasses for deferred shading
	TAttachmentReferenceClass InputAttachments2[RHI::MAX_RT_ATTACHMENTS + 1];
	TAttachmentReferenceClass DepthStencilAttachment;

	TAttachmentReferenceClass			 DepthStencilAttachmentReference;
	TVector<TAttachmentDescriptionClass> AttachmentDescriptions;

	// Tonemap subpass
	TAttachmentReferenceClass InputAttachments3[RHI::MAX_RT_ATTACHMENTS + 1];
	TAttachmentReferenceClass ColorAttachments3[RHI::MAX_RT_ATTACHMENTS + 1];

	FVulkanAttachmentReference<VkAttachmentReference2> ShadingRateAttachmentReference;
	FVulkanFragmentShadingRateAttachmentInfo		   FragmentShadingRateAttachmentInfo;

	// Depth stencil resolve
	FVulkanAttachmentReference<VkAttachmentReference2> DepthStencilResolveAttachmentReference;
	FVulkanDepthStencilResolveSubpassDesc			   DepthStencilResolveSubpassDesc;

	FVulkanRenderPassFragmentDensityMapCreateInfoEXT FragDensityCreateInfo;
	FVulkanRenderPassMultiviewCreateInfo			 MultiviewInfo;

	TRenderPassCreateInfoClass CreateInfo;
	FVulkanLogicalDeviceRef	   Device;

	TUINT32 ViewMask[2];
	TUINT32 CorrelationMask;
};

namespace VulkanRHI
{
	HLVM_EXTERN_FUNC VkRenderPass CreateVulkanRenderPass(FVulkanLogicalDeviceRef Device, const FVulkanRenderTargetLayout& RTLayout);
}

class FVulkanFrameBuffer : public FVulkanMinimalContext, public FRefCountable
{
public:
	FVulkanFrameBuffer(const FRHIRenderTargetsInfo& InRTInfo, const FVulkanRenderTargetLayout& RTLayout, const FVulkanRenderPassRef& RenderPass);
	~FVulkanFrameBuffer();

	bool Matches(const FRHIRenderTargetsInfo& RTInfo) const;

	TUINT32 GetNumColorAttachments() const
	{
		return NumColorAttachments;
	}

	void Destroy();

	VkFramebuffer GetHandle()
	{
		return Framebuffer;
	}

	const FVulkanView::FTextureView& GetPartialDepthTextureView() const
	{
		HLVM_ASSERT(PartialDepthTextureView);
		return PartialDepthTextureView->GetTextureView();
	}

	TVector<FVulkanViewRef> OwnedTextureViews;
	TVector<FVulkanViewRef> AttachmentTextureViews;

	// Copy from the Depth render target partial view
	FVulkanViewRef PartialDepthTextureView = nullptr;

	bool ContainsRenderTarget(FRHITextureRef Texture) const
	{
		FVulkanTextureRef VulkanTexture = Texture;
		return ContainsRenderTarget(VulkanTexture->GetImage());
	}

	bool ContainsRenderTarget(VkImage Image) const
	{
		HLVM_ENSURE(Image != VK_NULL_HANDLE);
		for (TUINT32 Index = 0; Index < NumColorAttachments; ++Index)
		{
			if (ColorRenderTargetImages[Index] == Image)
			{
				return true;
			}
		}

		return (DepthStencilRenderTargetImage == Image);
	}

	VkRect2D GetRenderArea() const
	{
		return RenderArea;
	}

private:
	VkFramebuffer Framebuffer;
	VkRect2D	  RenderArea;

	// Unadjusted number of color render targets as in FRHIRenderTargetsInfo
	TUINT32 NumColorRenderTargets;

	// Save image off for comparison, in case it gets aliased.
	TUINT32 NumColorAttachments;
	VkImage ColorRenderTargetImages[RHI::MAX_RT_ATTACHMENTS];
	VkImage ColorResolveTargetImages[RHI::MAX_RT_ATTACHMENTS];
	VkImage DepthStencilRenderTargetImage;
	VkImage DepthStencilResolveRenderTargetImage;

	// Predefined set of barriers, when executes ensuring all writes are finished
	TVector<VkImageMemoryBarrier> WriteBarriers;
};
using FVulkanFrameBufferRef = TRefCountPtr<FVulkanFrameBuffer>;

class FVulkanRenderPassManager : public FVulkanMinimalContext
{
public:
	FVulkanRenderPassRef  GetOrCreateRenderPass(const FVulkanRenderTargetLayout& RTLayout);
	FVulkanFrameBufferRef GetOrCreateFramebuffer(const FRHIRenderTargetsInfo& RenderTargetsInfo, const FVulkanRenderTargetLayout& RTLayout, FVulkanRenderPassRef RenderPass);

	// TODO
	//	void BeginRenderPass(FVulkanCommandListContext& Context, FVulkanLogicalDeviceRef InDevice, FVulkanCmdBuffer* CmdBuffer, const FRHIRenderPassInfo& RPInfo, const FVulkanRenderTargetLayout& RTLayout, FVulkanRenderPass* RenderPass, FVulkanFrameBuffer* Framebuffer);
	//	void EndRenderPass(FVulkanCmdBuffer* CmdBuffer);

	// TODO
	//void NotifyDeletedRenderTarget(VkImage Image);

private:
	FRWLock RenderPassesLock;
	FRWLock FramebuffersLock;

	TMap<FVulkanHash, FVulkanRenderPassRef> RenderPasses;

	struct FFramebufferList
	{
		TVector<FVulkanFrameBufferRef> Framebuffer;
	};
	TMap<FVulkanHash, TSharePtr<FFramebufferList>> Framebuffers;
};
