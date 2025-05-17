/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#include "VulkanRHIResource.h"
#include "VulkanRHIResourcePre.h"
#include "RHI/Vulkan/IVulkanDynamicRHI.h"

FVulkanMinimalContext::FVulkanMinimalContext()
{
	GetDynamicRHI<IVulkanDynamicRHI>()->SetVulkanMinimalContext(this);
}

FVulkanRenderTargetLayout::FVulkanRenderTargetLayout(const FRHIRenderPassInfo& RPInfo, const RenderPassAdditionalInfo& AdditionalInfo)
	: NumAttachmentDescriptions(0)
	, NumColorAttachments(0)
	, bHasDepthStencil(false)
	, bHasResolveAttachments(false)
	, bHasDepthStencilResolve(false)
	, bHasFragmentDensityAttachment(false)
	, NumSamples(0)
	, NumUsedClearValues(0)
{
	ResetAttachments();

	bool bSetExtent = false;
	bool bFoundClearOp = false;

	TUINT32 NumColorRenderTargets = RPInfo.GetNumColorRenderTargets();
	for (TUINT32 Index = 0; Index < NumColorRenderTargets; ++Index)
	{
		const FRHIRenderPassInfo::ColorRTBinding& ColorRTBinding = RPInfo.ColorRenderTargets[Index];
		FVulkanTexture*							  Texture = S_C(FVulkanTexture*, ColorRTBinding.RenderTarget.Get());
		HLVM_ASSERT(Texture);
		const FRHITextureCreateInfo& TextureInfo = Texture->GetCreateInfo();

		if (bSetExtent)
		{
			HLVM_ENSURE(Extent.Extent3D.width == TextureInfo.Extent.x >> ColorRTBinding.MipIndex);
			HLVM_ENSURE(Extent.Extent3D.height == TextureInfo.Extent.y >> ColorRTBinding.MipIndex);
			HLVM_ENSURE(Extent.Extent3D.depth == TextureInfo.Extent.z);
		}
		else
		{
			bSetExtent = true;
			Extent.Extent3D.width = TextureInfo.Extent.x >> ColorRTBinding.MipIndex;
			Extent.Extent3D.height = TextureInfo.Extent.y >> ColorRTBinding.MipIndex;
			Extent.Extent3D.depth = TextureInfo.Extent.z;
		}

		// CustomResolveSubpass can have targets with a different NumSamples
		HLVM_ENSURE(!NumSamples || NumSamples == ColorRTBinding.RenderTarget->GetNumSamples() || RPInfo.SubpassHint == ESubpassHint::CustomResolve);
		NumSamples = ColorRTBinding.RenderTarget->GetNumSamples();

		// With a CustomResolveSubpass last color attachment is a resolve target
		bool bCustomResolveAttachment = (Index == (NumColorRenderTargets - 1)) && RPInfo.SubpassHint == ESubpassHint::CustomResolve;

		VkAttachmentDescription& CurrDesc = Desc[NumAttachmentDescriptions];
		CurrDesc.samples = bCustomResolveAttachment ? VK_SAMPLE_COUNT_1_BIT : static_cast<VkSampleCountFlagBits>(NumSamples);
		CurrDesc.format = VulkanRHI::VulkanFormatFromRHIFormat(ColorRTBinding.RenderTarget->GetFormat(), ColorRTBinding.RenderTarget->IsSRGB());
		CurrDesc.loadOp = VulkanRHI::VulkanAttachmentLoadOpFromRHIAction(RHI::GetLoadAction(ColorRTBinding.Action));
		bFoundClearOp = bFoundClearOp || (CurrDesc.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR);
		CurrDesc.storeOp = VulkanRHI::VulkanAttachmentStoreOpFromRHIAction(RHI::GetStoreAction(ColorRTBinding.Action));
		CurrDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		CurrDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

		if (Texture->GetCreateFlags() & ETextureCreateFlag::MemoryLess)
		{
			HLVM_ENSURE(CurrDesc.storeOp == VK_ATTACHMENT_STORE_OP_DONT_CARE);
		}

		// If the initial != final we need to change the FullHashInfo and use FinalLayout
		CurrDesc.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		CurrDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		ColorReferences[NumColorAttachments].attachment = NumAttachmentDescriptions;
		ColorReferences[NumColorAttachments].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		if (CurrDesc.samples > VK_SAMPLE_COUNT_1_BIT && ColorRTBinding.ResolveTarget)
		{
			Desc[NumAttachmentDescriptions + 1] = Desc[NumAttachmentDescriptions];
			Desc[NumAttachmentDescriptions + 1].samples = VK_SAMPLE_COUNT_1_BIT;
			Desc[NumAttachmentDescriptions + 1].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			Desc[NumAttachmentDescriptions + 1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			ResolveReferences[NumColorAttachments].attachment = NumAttachmentDescriptions + 1;
			ResolveReferences[NumColorAttachments].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			++NumAttachmentDescriptions;
			bHasResolveAttachments = true;
		}

		++NumAttachmentDescriptions;
		++NumColorAttachments;
	}

	if (RPInfo.DepthStencilRenderTarget.DepthStencilTarget)
	{
		VkAttachmentDescription& CurrDesc = Desc[NumAttachmentDescriptions];
		FMemory::Memzero(&CurrDesc);
		FVulkanTextureRef Texture = S_C(FVulkanTextureRef, RPInfo.DepthStencilRenderTarget.DepthStencilTarget);
		HLVM_ASSERT(Texture);
		const FRHITextureCreateInfo& TextureInfo = Texture->GetCreateInfo();
		CurrDesc.samples = static_cast<VkSampleCountFlagBits>(RPInfo.DepthStencilRenderTarget.DepthStencilTarget->GetNumSamples());
		// CustomResolveSubpass can have targets with a different NumSamples
		HLVM_ENSURE(!NumSamples || CurrDesc.samples == NumSamples || RPInfo.SubpassHint == ESubpassHint::CustomResolve);
		NumSamples = S_C(TUINT8, CurrDesc.samples);
		CurrDesc.format = VulkanRHI::VulkanFormatFromRHIFormat(RPInfo.DepthStencilRenderTarget.DepthStencilTarget->GetFormat());
		CurrDesc.loadOp = VulkanRHI::VulkanAttachmentLoadOpFromRHIAction(RHI::GetLoadAction(RHI::GetDepthActions(RPInfo.DepthStencilRenderTarget.Action)));
		CurrDesc.stencilLoadOp = VulkanRHI::VulkanAttachmentLoadOpFromRHIAction(RHI::GetLoadAction(RHI::GetStencilActions(RPInfo.DepthStencilRenderTarget.Action)));
		bFoundClearOp = bFoundClearOp || (CurrDesc.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR || CurrDesc.stencilLoadOp == VK_ATTACHMENT_LOAD_OP_CLEAR);

		CurrDesc.storeOp = VulkanRHI::VulkanAttachmentStoreOpFromRHIAction(RHI::GetStoreAction(RHI::GetDepthActions(RPInfo.DepthStencilRenderTarget.Action)));
		CurrDesc.stencilStoreOp = VulkanRHI::VulkanAttachmentStoreOpFromRHIAction(RHI::GetStoreAction(RHI::GetStencilActions(RPInfo.DepthStencilRenderTarget.Action)));

		if (TextureInfo.Flags & ETextureCreateFlag::MemoryLess)
		{
			HLVM_ENSURE(CurrDesc.storeOp == VK_ATTACHMENT_STORE_OP_DONT_CARE);
			HLVM_ENSURE(CurrDesc.stencilStoreOp == VK_ATTACHMENT_STORE_OP_DONT_CARE);
		}

		VkImageLayout CurrentDepthLayout = AdditionalInfo.CurrentDepthLayout;
		VkImageLayout CurrentStencilLayout = AdditionalInfo.CurrentStencilLayout;

		FExclusiveDepthStencil ExclusiveDepthStencil = RPInfo.DepthStencilRenderTarget.ExclusiveDepthStencil;
		if (RPInfo.DepthStencilRenderTarget.Action == EDepthStencilTargetActions::LoadDepthClearStencil_StoreDepthStencil)
		{
			ExclusiveDepthStencil = FExclusiveDepthStencil::DepthWrite_StencilWrite;
			CurrentDepthLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
			CurrentStencilLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
		}

		// If the initial != final we need to change the FullHashInfo and use FinalLayout
		CurrDesc.initialLayout = CurrentDepthLayout;
		CurrDesc.finalLayout = CurrentDepthLayout;
		StencilDesc.stencilInitialLayout = CurrentStencilLayout;
		StencilDesc.stencilFinalLayout = CurrentStencilLayout;

		// We can't have the final layout be UNDEFINED, but it's possible that we get here from a transient texture
		// where the stencil was never used yet.  We can set the layout to whatever we want, the next transition will
		// happen from UNDEFINED anyhow.
		if (CurrentDepthLayout == VK_IMAGE_LAYOUT_UNDEFINED)
		{
			// Unused image aspects with a LoadOp but undefined layout should just remain untouched
			if (!RPInfo.DepthStencilRenderTarget.ExclusiveDepthStencil.IsUsingDepth() && (CurrDesc.loadOp == VK_ATTACHMENT_LOAD_OP_LOAD))
			{
				CurrDesc.loadOp = VK_ATTACHMENT_LOAD_OP_NONE_KHR;
			}

			HLVM_ASSERT(CurrDesc.storeOp == VK_ATTACHMENT_STORE_OP_DONT_CARE);
			CurrDesc.finalLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
		}
		if (CurrentStencilLayout == VK_IMAGE_LAYOUT_UNDEFINED)
		{
			// Unused image aspects with a LoadOp but undefined layout should just remain untouched
			if (!RPInfo.DepthStencilRenderTarget.ExclusiveDepthStencil.IsUsingStencil() && (CurrDesc.stencilLoadOp == VK_ATTACHMENT_LOAD_OP_LOAD))
			{
				CurrDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_NONE_KHR;
			}

			HLVM_ASSERT(CurrDesc.stencilStoreOp == VK_ATTACHMENT_STORE_OP_DONT_CARE);
			StencilDesc.stencilFinalLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
		}

		DepthReference.attachment = NumAttachmentDescriptions;
		DepthReference.layout = CurrentDepthLayout;
		StencilReference.stencilLayout = CurrentStencilLayout;

		if (CurrDesc.samples > VK_SAMPLE_COUNT_1_BIT && RPInfo.DepthStencilRenderTarget.ResolveTarget)
		{
			Desc[NumAttachmentDescriptions + 1] = Desc[NumAttachmentDescriptions];
			Desc[NumAttachmentDescriptions + 1].samples = VK_SAMPLE_COUNT_1_BIT;
			Desc[NumAttachmentDescriptions + 1].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			Desc[NumAttachmentDescriptions + 1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			Desc[NumAttachmentDescriptions + 1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			Desc[NumAttachmentDescriptions + 1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
			DepthStencilResolveReference.attachment = NumAttachmentDescriptions + 1;
			DepthStencilResolveReference.layout = CurrentDepthLayout;
			// NumColorAttachments was incremented after the last color attachment
			HLVM_ENSURE_F(NumColorAttachments < 16, TXT("Must have room for depth resolve bit"));
			++NumAttachmentDescriptions;
			bHasDepthStencilResolve = true;
		}

		++NumAttachmentDescriptions;

		bHasDepthStencil = true;

		if (bSetExtent)
		{
			// Depth can be greater or equal to color. Clamp to the smaller size.
			Extent.Extent3D.width = FMath::Min<TUINT32>(Extent.Extent3D.width, TextureInfo.Extent.x);
			Extent.Extent3D.height = FMath::Min<TUINT32>(Extent.Extent3D.height, TextureInfo.Extent.y);
		}
		else
		{
			bSetExtent = true;
			Extent.Extent3D.width = TextureInfo.Extent.x;
			Extent.Extent3D.height = TextureInfo.Extent.y;
			Extent.Extent3D.depth = TextureInfo.Extent.z;
		}
	}
	else if (NumColorRenderTargets == 0)
	{
		// No Depth and no color, it's a raster-only pass so make sure the renderArea will be set up properly
		HLVM_ASSERT_F(RPInfo.ResolveRect.IsValid(), TXT("For raster-only passes without render targets, ResolveRect has to contain the render area"));
		bSetExtent = true;
		Offset.Offset3D.x = S_C(TINT32, RPInfo.ResolveRect.X1);
		Offset.Offset3D.y = S_C(TINT32, RPInfo.ResolveRect.Y1);
		Offset.Offset3D.z = 0;
		Extent.Extent3D.width = RPInfo.ResolveRect.X2 - RPInfo.ResolveRect.X1;
		Extent.Extent3D.height = RPInfo.ResolveRect.Y2 - RPInfo.ResolveRect.Y1;
		Extent.Extent3D.depth = 1;
	}

	SubpassHint = RPInfo.SubpassHint;
	NumUsedClearValues = bFoundClearOp ? NumAttachmentDescriptions : 0;
}

FVulkanRenderTargetLayout::FVulkanRenderTargetLayout(const FGraphicsPSOCreateInfo& PSOInfo)
	: NumAttachmentDescriptions(0)
	, NumColorAttachments(0)
	, bHasDepthStencil(false)
	, bHasResolveAttachments(false)
	, bHasDepthStencilResolve(false)
	, bHasFragmentDensityAttachment(false)
	, NumSamples(0)
	, NumUsedClearValues(0)
{
	ResetAttachments();

	bool bFoundClearOp = false;
	NumSamples = PSOInfo.NumSamples;
	for (TUINT32 Index = 0; Index < PSOInfo.RenderTargetsEnabled; ++Index)
	{
		EPixelFormat UEFormat = PSOInfo.RenderTargetFormats[Index];
		if (UEFormat != EPixelFormat::None)
		{
			// With a CustomResolveSubpass last color attachment is a resolve target
			bool bCustomResolveAttachment = (Index == (PSOInfo.RenderTargetsEnabled - 1)) && PSOInfo.SubpassHint == ESubpassHint::CustomResolve;

			VkAttachmentDescription& CurrDesc = Desc[NumAttachmentDescriptions];
			CurrDesc.samples = bCustomResolveAttachment ? VK_SAMPLE_COUNT_1_BIT : static_cast<VkSampleCountFlagBits>(NumSamples);
			CurrDesc.format = VulkanRHI::VulkanFormatFromRHIFormat(UEFormat, PSOInfo.RenderTargetFlags[Index] & ETextureCreateFlag::SRGB);
			CurrDesc.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			CurrDesc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			CurrDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			CurrDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

			// If the initial != final we need to change the FullHashInfo and use FinalLayout
			CurrDesc.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			CurrDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			ColorReferences[NumColorAttachments].attachment = NumAttachmentDescriptions;
			ColorReferences[NumColorAttachments].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			if (CurrDesc.samples > VK_SAMPLE_COUNT_1_BIT)
			{
				Desc[NumAttachmentDescriptions + 1] = Desc[NumAttachmentDescriptions];
				Desc[NumAttachmentDescriptions + 1].samples = VK_SAMPLE_COUNT_1_BIT;
				Desc[NumAttachmentDescriptions + 1].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				Desc[NumAttachmentDescriptions + 1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				ResolveReferences[NumColorAttachments].attachment = NumAttachmentDescriptions + 1;
				ResolveReferences[NumColorAttachments].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				++NumAttachmentDescriptions;
				bHasResolveAttachments = true;
			}


			++NumAttachmentDescriptions;
			++NumColorAttachments;
		}
	}

	if (PSOInfo.DepthStencilTargetFormat != EPixelFormat::None)
	{
		VkAttachmentDescription& CurrDesc = Desc[NumAttachmentDescriptions];
		FMemory::Memzero(&CurrDesc);

		CurrDesc.samples = static_cast<VkSampleCountFlagBits>(NumSamples);
		CurrDesc.format = VulkanRHI::VulkanFormatFromRHIFormat(PSOInfo.DepthStencilTargetFormat, false);
		CurrDesc.loadOp = VulkanRHI::VulkanAttachmentLoadOpFromRHIAction(PSOInfo.DepthTargetLoadAction);
		CurrDesc.stencilLoadOp = VulkanRHI::VulkanAttachmentLoadOpFromRHIAction(PSOInfo.StencilTargetLoadAction);
		if (CurrDesc.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR || CurrDesc.stencilLoadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
		{
			bFoundClearOp = true;
		}
		CurrDesc.storeOp = VulkanRHI::VulkanAttachmentStoreOpFromRHIAction(PSOInfo.DepthTargetStoreAction);
		CurrDesc.stencilStoreOp = VulkanRHI::VulkanAttachmentStoreOpFromRHIAction(PSOInfo.StencilTargetStoreAction);

		const VkImageLayout DepthLayout = PSOInfo.DepthStencilAccess.IsDepthWrite() ? VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
		const VkImageLayout StencilLayout = PSOInfo.DepthStencilAccess.IsStencilWrite() ? VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;

		// If the initial != final we need to change the FullHashInfo and use FinalLayout
		CurrDesc.initialLayout = DepthLayout;
		CurrDesc.finalLayout = DepthLayout;
		StencilDesc.stencilInitialLayout = StencilLayout;
		StencilDesc.stencilFinalLayout = StencilLayout;

		DepthReference.attachment = NumAttachmentDescriptions;
		DepthReference.layout = DepthLayout;
		StencilReference.stencilLayout = StencilLayout;

		const bool bDepthStencilResolve = (PSOInfo.DepthTargetStoreAction == ERenderTargetStoreAction::MultisampleResolve) || (PSOInfo.StencilTargetStoreAction == ERenderTargetStoreAction::MultisampleResolve);
		if (bDepthStencilResolve && CurrDesc.samples > VK_SAMPLE_COUNT_1_BIT)
		{
			Desc[NumAttachmentDescriptions + 1] = Desc[NumAttachmentDescriptions];
			Desc[NumAttachmentDescriptions + 1].samples = VK_SAMPLE_COUNT_1_BIT;
			Desc[NumAttachmentDescriptions + 1].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			Desc[NumAttachmentDescriptions + 1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			Desc[NumAttachmentDescriptions + 1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			Desc[NumAttachmentDescriptions + 1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
			DepthStencilResolveReference.attachment = NumAttachmentDescriptions + 1;
			DepthStencilResolveReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			// NumColorAttachments was incremented after the last color attachment
			HLVM_ENSURE_F(NumColorAttachments < 16, TXT("Must have room for depth resolve bit"));
			++NumAttachmentDescriptions;
			bHasDepthStencilResolve = true;
		}

		++NumAttachmentDescriptions;
		bHasDepthStencil = true;
	}


	SubpassHint = PSOInfo.SubpassHint;
	NumUsedClearValues = bFoundClearOp ? NumAttachmentDescriptions : 0;
}
