/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#include "VulkanRenderPass.h"

namespace VulkanRHI
{
	VkRenderPass CreateVulkanRenderPass(FVulkanLogicalDeviceRef InDevice, const FVulkanRenderTargetLayout& RTLayout)
	{
		VkRenderPass RenderPass = VK_NULL_HANDLE;
#if VULKAN_RENDERPASS2
		FVulkanRenderPassBuilder<FVulkanSubpassDescription<VkSubpassDescription2>,
			FVulkanSubpassDependency<VkSubpassDependency2>,
			FVulkanAttachmentReference<VkAttachmentReference2>,
			FVulkanAttachmentDescription<VkAttachmentDescription2>,
			FVulkanRenderPassCreateInfo<VkRenderPassCreateInfo2>>
			Creator(InDevice);
#else
		FVulkanRenderPassBuilder<FVulkanSubpassDescription<VkSubpassDescription>,
			FVulkanSubpassDependency<VkSubpassDependency>,
			FVulkanAttachmentReference<VkAttachmentReference>,
			FVulkanAttachmentDescription<VkAttachmentDescription>,
			FVulkanRenderPassCreateInfo<VkRenderPassCreateInfo>>
			Creator(InDevice);
#endif
		RenderPass = Creator.Create(RTLayout);
		return RenderPass;
	}
} // namespace VulkanRHI

FVulkanRenderPass::FVulkanRenderPass(const FVulkanRenderTargetLayout& InRTLayout)
	: Layout(InRTLayout)
{
	RenderPass = VulkanRHI::CreateVulkanRenderPass(LogicalDevice, Layout);
}

FVulkanRenderPass::~FVulkanRenderPass()
{
	VulkanRHI::vkDestroyRenderPass(LogicalDevice->GetHandle(), RenderPass, VulkanRHI::VULKAN_CPU_ALLOCATOR);
}

FVulkanFrameBuffer::FVulkanFrameBuffer(const FRHIRenderTargetsInfo& InRTInfo, const FVulkanRenderTargetLayout& RTLayout, const FVulkanRenderPassRef& RenderPass)
	: Framebuffer(VK_NULL_HANDLE)
	, NumColorRenderTargets(InRTInfo.NumColorRenderTargets)
	, NumColorAttachments(0)
	, DepthStencilRenderTargetImage(VK_NULL_HANDLE)
{
	FMemory::MemzeroArray(&ColorRenderTargetImages);
	FMemory::MemzeroArray(&ColorResolveTargetImages);

	AttachmentTextureViews.Empty(RTLayout.GetNumAttachmentDescriptions());

	auto CreateOwnedView = [&]() {
		const VkDescriptorType DescriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		FVulkanViewRef		   View = new FVulkanView(DescriptorType);
		AttachmentTextureViews.Add(View);
		OwnedTextureViews.Add(View);
		return View;
	};

	auto AddExternalView = [&](FVulkanViewRef View) {
		AttachmentTextureViews.Add(View);
	};

	TUINT32 MipIndex = 0;

	const VkExtent3D& RTExtents = RTLayout.GetExtent3D();
	// Adreno does not like zero size RTs
	HLVM_ASSERT(RTExtents.width != 0 && RTExtents.height != 0);
	TUINT32 NumLayers = RTExtents.depth;

	for (TUINT32 Index = 0; Index < InRTInfo.NumColorRenderTargets; ++Index)
	{
		FRHITextureRef RHITexture = InRTInfo.ColorRenderTarget[Index].Texture;
		if (!RHITexture)
		{
			continue;
		}

		FVulkanTextureRef			 Texture = FVulkanTextureRef(RHITexture);
		const FRHITextureCreateInfo& Desc = Texture->GetCreateInfo();

		// this could fire in case one of the textures is FVulkanBackBuffer and it has not acquired an image
		// with EDelayAcquireImageType::LazyAcquire acquire happens when texture transition to Writeable state
		// make sure you call TransitionResource(Writable, Tex) before using this texture as a render-target
		HLVM_ASSERT(Texture->GetImage() != VK_NULL_HANDLE);

		ColorRenderTargetImages[Index] = Texture->GetImage();
		MipIndex = InRTInfo.ColorRenderTarget[Index].MipIndex;

		if (Texture->GetImageViewType() == VK_IMAGE_VIEW_TYPE_2D || Texture->GetImageViewType() == VK_IMAGE_VIEW_TYPE_2D_ARRAY)
		{
			TUINT32 ArraySliceIndex = 0;
			TUINT32 NumArraySlices = 1;
			if (InRTInfo.ColorRenderTarget[Index].ArraySliceIndex == -1)
			{
				ArraySliceIndex = 0;
				NumArraySlices = Texture->GetVulkanArraySize();
			}
			else
			{
				ArraySliceIndex = S_C(TUINT32, InRTInfo.ColorRenderTarget[Index].ArraySliceIndex);
				NumArraySlices = 1;
				HLVM_ASSERT(ArraySliceIndex < Texture->GetVulkanArraySize());
			}

			// About !RTLayout.GetIsMultiView(), from https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkFramebufferCreateInfo.html:
			// If the render pass uses multiview, then layers must be one
			if (Texture->GetImageViewType() == VK_IMAGE_VIEW_TYPE_2D_ARRAY)
			{
				NumLayers = NumArraySlices;
			}

			CreateOwnedView()->InitAsTextureView(
				Texture->GetImage(), Texture->GetImageViewType(), Texture->GetFullAspectFlags(), Desc.Format,
				Texture->GetViewFormat(), MipIndex, 1, ArraySliceIndex, NumArraySlices, true,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | (Texture->GetImageUsageFlags() & VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT));
		}
		else if (Texture->GetImageViewType() == VK_IMAGE_VIEW_TYPE_CUBE || Texture->GetImageViewType() == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY)
		{
			CreateOwnedView()->InitAsTextureView(
				Texture->GetImage(), VK_IMAGE_VIEW_TYPE_2D, Texture->GetFullAspectFlags(), Desc.Format, Texture->GetViewFormat(), MipIndex, 1,
				S_C(TUINT32, InRTInfo.ColorRenderTarget[Index].ArraySliceIndex), 1, true, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | (Texture->GetImageUsageFlags() & VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT));
		}
		else if (Texture->GetImageViewType() == VK_IMAGE_VIEW_TYPE_3D)
		{
			CreateOwnedView()->InitAsTextureView(
				Texture->GetImage(), VK_IMAGE_VIEW_TYPE_2D_ARRAY, Texture->GetFullAspectFlags(), Desc.Format, Texture->GetViewFormat(), MipIndex, 1, 0,
				Desc.Extent.z, true, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | (Texture->GetImageUsageFlags() & VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT));
		}
		else
		{
			HLVM_ENSURE(0);
		}

		++NumColorAttachments;

		// Check the RTLayout as well to make sure the resolve attachment is needed (Vulkan and Feature level specific)
		// See: FVulkanRenderTargetLayout constructor with FRHIRenderPassInfo
		if (InRTInfo.bHasResolveAttachments && RTLayout.GetHasResolveAttachments() && RTLayout.GetResolveAttachmentReferences()[Index].layout != VK_IMAGE_LAYOUT_UNDEFINED)
		{
			FRHITextureRef	  ResolveRHITexture = InRTInfo.ColorResolveRenderTarget[Index].Texture;
			FVulkanTextureRef ResolveTexture = (ResolveRHITexture);
			ColorResolveTargetImages[Index] = ResolveTexture->GetImage();

			// resolve attachments only supported for 2d/2d array textures
			if (ResolveTexture->GetImageViewType() == VK_IMAGE_VIEW_TYPE_2D || ResolveTexture->GetImageViewType() == VK_IMAGE_VIEW_TYPE_2D_ARRAY)
			{
				CreateOwnedView()->InitAsTextureView(
					ResolveTexture->GetImage(), ResolveTexture->GetImageViewType(), ResolveTexture->GetFullAspectFlags(), ResolveTexture->GetCreateInfo().Format, ResolveTexture->GetViewFormat(), MipIndex, 1, FMath::Max<TUINT32>(0, InRTInfo.ColorRenderTarget[Index].ArraySliceIndex), ResolveTexture->GetVulkanArraySize(), true);
			}
		}
	}

	if (RTLayout.GetHasDepthStencil())
	{
		FVulkanTextureRef Texture = (InRTInfo.DepthStencilRenderTarget.Texture);
		// const FRHITextureCreateInfo& Desc = Texture->GetCreateInfo();
		DepthStencilRenderTargetImage = Texture->GetImage();
		HLVM_ENSURE(RHI::HasStencil(Texture->GetFormat()));

		HLVM_ASSERT(Texture->GetPartialAspectFlags());
		PartialDepthTextureView = Texture->GetPartialView();

		HLVM_ENSURE(Texture->GetImageViewType() == VK_IMAGE_VIEW_TYPE_2D || Texture->GetImageViewType() == VK_IMAGE_VIEW_TYPE_2D_ARRAY || Texture->GetImageViewType() == VK_IMAGE_VIEW_TYPE_CUBE);
		if (NumColorAttachments == 0 && Texture->GetImageViewType() == VK_IMAGE_VIEW_TYPE_CUBE)
		{
			CreateOwnedView()->InitAsTextureView(
				Texture->GetImage(), VK_IMAGE_VIEW_TYPE_2D_ARRAY, Texture->GetFullAspectFlags(), Texture->GetCreateInfo().Format, Texture->GetViewFormat(), MipIndex, 1, 0, 6, true);

			NumLayers = 6;
		}
		else if (Texture->GetImageViewType() == VK_IMAGE_VIEW_TYPE_2D || Texture->GetImageViewType() == VK_IMAGE_VIEW_TYPE_2D_ARRAY)
		{
			// depth attachments need a separate view to have no swizzle components, for validation correctness
			CreateOwnedView()->InitAsTextureView(
				Texture->GetImage(), Texture->GetImageViewType(), Texture->GetFullAspectFlags(), Texture->GetCreateInfo().Format, Texture->GetViewFormat(), MipIndex, 1, 0, Texture->GetVulkanArraySize(), true);
		}
		else
		{
			AddExternalView(Texture->GetFullView());
		}

		if (RTLayout.GetHasDepthStencilResolve() && RTLayout.GetDepthStencilResolveAttachmentReference()->layout != VK_IMAGE_LAYOUT_UNDEFINED)
		{
			FRHITextureRef	  ResolveRHITexture = InRTInfo.DepthStencilResolveRenderTarget.Texture;
			FVulkanTextureRef ResolveTexture = (ResolveRHITexture);
			DepthStencilResolveRenderTargetImage = ResolveTexture->GetImage();

			// Resolve attachments only supported for 2d/2d array textures
			if (ResolveTexture->GetImageViewType() == VK_IMAGE_VIEW_TYPE_2D || ResolveTexture->GetImageViewType() == VK_IMAGE_VIEW_TYPE_2D_ARRAY)
			{
				CreateOwnedView()->InitAsTextureView(
					ResolveTexture->GetImage(), ResolveTexture->GetImageViewType(), ResolveTexture->GetFullAspectFlags(), ResolveTexture->GetCreateInfo().Format, ResolveTexture->GetViewFormat(), MipIndex, 1, 0, ResolveTexture->GetVulkanArraySize(), true);
			}
		}
	}

	TVector<VkImageView> AttachmentViews;
	AttachmentViews.AddDefaulted(AttachmentTextureViews.Num());
	for (TUINT32 Index = 0; Index < AttachmentTextureViews.Num(); ++Index)
	{
		AttachmentViews[Index] = AttachmentTextureViews[Index]->GetTextureView().View;
	}

	VkFramebufferCreateInfo CreateInfo;
	VulkanRHI::ZeroVulkanStruct(&CreateInfo, VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO);
	CreateInfo.renderPass = RenderPass->GetHandle();
	CreateInfo.attachmentCount = AttachmentViews.Num();
	CreateInfo.pAttachments = AttachmentViews.GetData();
	CreateInfo.width = RTExtents.width;
	CreateInfo.height = RTExtents.height;
	CreateInfo.layers = NumLayers;
	VULKAN_ENSURE(VulkanRHI::vkCreateFramebuffer(LogicalDevice->GetHandle(), &CreateInfo, VulkanRHI::VULKAN_CPU_ALLOCATOR, &Framebuffer));

	RenderArea.offset.x = 0;
	RenderArea.offset.y = 0;
	RenderArea.extent.width = RTExtents.width;
	RenderArea.extent.height = RTExtents.height;
}

FVulkanFrameBuffer::~FVulkanFrameBuffer()
{
	HLVM_ENSURE(Framebuffer == VK_NULL_HANDLE);
}

void FVulkanFrameBuffer::Destroy()
{
	// TODO : Deferred deletion queue (clean up before RHI destructor)
	//	VulkanRHI::FDeferredDeletionQueue2& Queue = LogicalDevice.GetDeferredDeletionQueue();
	//
	//	// will be deleted in reverse order
	//	Queue.EnqueueResource(VulkanRHI::FDeferredDeletionQueue2::EType::Framebuffer, Framebuffer);
	HLVM_ASSERT(Framebuffer != VK_NULL_HANDLE);
	VulkanRHI::vkDestroyFramebuffer(LogicalDevice->GetHandle(), Framebuffer, VulkanRHI::VULKAN_CPU_ALLOCATOR);
	Framebuffer = VK_NULL_HANDLE;
}

bool FVulkanFrameBuffer::Matches(const FRHIRenderTargetsInfo& InRTInfo) const
{
	if (NumColorRenderTargets != InRTInfo.NumColorRenderTargets)
	{
		return false;
	}

	{
		const FRHIDepthStencilRenderTargetView& B = InRTInfo.DepthStencilRenderTarget;
		if (B.Texture)
		{
			VkImage AImage = DepthStencilRenderTargetImage;
			VkImage BImage = FVulkanTextureRef(B.Texture)->GetImage();
			if (AImage != BImage)
			{
				return false;
			}
		}
	}

	{
		const FRHIDepthStencilRenderTargetView& R = InRTInfo.DepthStencilResolveRenderTarget;
		if (R.Texture)
		{
			VkImage AImage = DepthStencilResolveRenderTargetImage;
			VkImage BImage = FVulkanTextureRef(R.Texture)->GetImage();
			if (AImage != BImage)
			{
				return false;
			}
		}
	}

	TUINT32 AttachementIndex = 0;
	for (TUINT32 Index = 0; Index < InRTInfo.NumColorRenderTargets; ++Index)
	{
		if (InRTInfo.bHasResolveAttachments)
		{
			const FRHIRenderTargetView& R = InRTInfo.ColorResolveRenderTarget[Index];
			if (R.Texture)
			{
				VkImage AImage = ColorResolveTargetImages[AttachementIndex];
				VkImage BImage = FVulkanTextureRef(R.Texture)->GetImage();
				if (AImage != BImage)
				{
					return false;
				}
			}
		}

		const FRHIRenderTargetView& B = InRTInfo.ColorRenderTarget[Index];
		if (B.Texture)
		{
			VkImage AImage = ColorRenderTargetImages[AttachementIndex];
			VkImage BImage = FVulkanTextureRef(B.Texture)->GetImage();
			if (AImage != BImage)
			{
				return false;
			}
			AttachementIndex++;
		}
	}

	return true;
}

FVulkanRenderPassRef FVulkanRenderPassManager::GetOrCreateRenderPass(const FVulkanRenderTargetLayout& RTLayout)
{
	const FVulkanHash& RenderPassHash = RTLayout.GetRenderPassFullHash();
	{
		LOCK_GUARD_RW(RenderPassesLock, FRWLock::Group::Read);
		FVulkanRenderPassRef* FoundRenderPass = RenderPasses.Find(RenderPassHash);
		if (FoundRenderPass)
		{
			return *FoundRenderPass;
		}
	}

	FVulkanRenderPassRef RenderPass = new FVulkanRenderPass(RTLayout);
	{
		LOCK_GUARD_RW(RenderPassesLock, FRWLock::Group::Write);
		FVulkanRenderPassRef* FoundRenderPass = RenderPasses.Find(RenderPassHash);
		if (FoundRenderPass)
		{
			return *FoundRenderPass;
		}
		else
		{
			RenderPasses.Add(RenderPassHash, RenderPass);
		}
	}
	return RenderPass;
}

FVulkanFrameBufferRef FVulkanRenderPassManager::GetOrCreateFramebuffer(const FRHIRenderTargetsInfo& RenderTargetsInfo, const FVulkanRenderTargetLayout& RTLayout, FVulkanRenderPassRef RenderPass)
{
	FVulkanHash RTLayoutHash = RTLayout.GetRenderPassCompatibleHash();
	TUINT64		MipsAndSlicesValues[RHI::MAX_RT_ATTACHMENTS];
	for (TUINT32 Index = 0; Index < HLVM_ARRAY_SIZE(MipsAndSlicesValues); ++Index)
	{
		MipsAndSlicesValues[Index] = (S_C(TUINT64, RenderTargetsInfo.ColorRenderTarget[Index].ArraySliceIndex) << 32) | S_C(TUINT64, RenderTargetsInfo.ColorRenderTarget[Index].MipIndex);
	}
	RTLayoutHash.Update(MipsAndSlicesValues, sizeof(MipsAndSlicesValues));

	auto FindFramebufferInList = [&](const TSharePtr<FFramebufferList>& InFramebufferList) {
		FVulkanFrameBufferRef OutFramebuffer = nullptr;

		for (TUINT32 Index = 0; Index < InFramebufferList->Framebuffer.Num(); ++Index)
		{
			const VkRect2D RenderArea = InFramebufferList->Framebuffer[Index]->GetRenderArea();

			if (InFramebufferList->Framebuffer[Index]->Matches(RenderTargetsInfo) && ((RTLayout.GetExtent2D().width == RenderArea.extent.width) && (RTLayout.GetExtent2D().height == RenderArea.extent.height) && (RTLayout.GetOffset2D().x == RenderArea.offset.x) && (RTLayout.GetOffset2D().y == RenderArea.offset.y)))
			{
				OutFramebuffer = InFramebufferList->Framebuffer[Index];
				break;
			}
		}

		return OutFramebuffer;
	};

	TSharePtr<FFramebufferList>* FoundFramebufferList = nullptr;
	TSharePtr<FFramebufferList>  FramebufferList = nullptr;

	{
		LOCK_GUARD_RW(FramebuffersLock, FRWLock::Group::Read);
		FoundFramebufferList = Framebuffers.Find(RTLayoutHash);
		if (FoundFramebufferList)
		{
			FramebufferList = *FoundFramebufferList;

			FVulkanFrameBufferRef ExistingFramebuffer = FindFramebufferInList(FramebufferList);
			if (ExistingFramebuffer)
			{
				return ExistingFramebuffer;
			}
		}
	}

	FVulkanFrameBufferRef Framebuffer = new FVulkanFrameBuffer(RenderTargetsInfo, RTLayout, RenderPass);
	{
		LOCK_GUARD_RW(FramebuffersLock, FRWLock::Group::Write);
		FoundFramebufferList = Framebuffers.Find(RTLayoutHash);
		if (!FoundFramebufferList)
		{
			FramebufferList = MAKE_SHARED(FFramebufferList);
			Framebuffers.Add(RTLayoutHash, FramebufferList);
		}
		else
		{
			FramebufferList = *FoundFramebufferList;
			FVulkanFrameBufferRef ExistingFramebuffer = FindFramebufferInList(FramebufferList);
			if (ExistingFramebuffer)
			{
				return ExistingFramebuffer;
			}
		}

		FramebufferList->Framebuffer.Add(Framebuffer);
	}
	return Framebuffer;
}
