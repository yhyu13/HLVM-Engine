/**
* Copyright (c) 2025. MIT License. All rights reserved.
*/

#include "VulkanRHIResource.h"


FVulkanRenderTargetLayout::FVulkanRenderTargetLayout(const FRHIRenderPassInfo& RPInfo)
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
		FVulkanTexture* Texture = S_C(FVulkanTexture*, ColorRTBinding.RenderTarget.Get());
		HLVM_ASSERT(Texture);
		const FRHITextureCreateInfo& TextureInfo = Texture->GetCreateInfo();

		if (bSetExtent)
		{
			HLVM_ENSURE(Extent.Extent3D.width == TextureInfo.Dimensions.x >> ColorRTBinding.MipIndex);
			HLVM_ENSURE(Extent.Extent3D.height == TextureInfo.Dimensions.y >> ColorRTBinding.MipIndex);
			HLVM_ENSURE(Extent.Extent3D.depth == TextureInfo.Dimensions.z);
		}
		else
		{
			bSetExtent = true;
			Extent.Extent3D.width = TextureInfo.Dimensions.x >> ColorRTBinding.MipIndex;
			Extent.Extent3D.height = TextureInfo.Dimensions.y >> ColorRTBinding.MipIndex;
			Extent.Extent3D.depth = TextureInfo.Dimensions.z;
		}

		// CustomResolveSubpass can have targets with a different NumSamples
		HLVM_ENSURE(!NumSamples || NumSamples == ColorRTBinding.RenderTarget->GetNumSamples() || RPInfo.SubpassType == ESubpassType::CustomResolve);
		NumSamples = ColorRTBinding.RenderTarget->GetNumSamples();

		// With a CustomResolveSubpass last color attachment is a resolve target
		bool bCustomResolveAttachment = (Index == (NumColorRenderTargets - 1)) && RPInfo.SubpassType == ESubpassType::CustomResolve;

		VkAttachmentDescription& CurrDesc = Desc[NumAttachmentDescriptions];
		CurrDesc.samples = bCustomResolveAttachment ? VK_SAMPLE_COUNT_1_BIT : static_cast<VkSampleCountFlagBits>(NumSamples);
		CurrDesc.format = VulkanRHI::VulkanFormatFromRHIFormat(ColorRTBinding.RenderTarget->GetFormat());
		CurrDesc.loadOp = VulkanRHI::VulkanAttachmentLoadOpFromRHI(RHI::GetLoadAction(ColorRTBinding.Action));
		bFoundClearOp = bFoundClearOp || (CurrDesc.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR);
		CurrDesc.storeOp = VulkanRHI::VulkanAttachmentStoreOpFromRHI(RHI::GetStoreAction(ColorRTBinding.Action));
		CurrDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		CurrDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

		if (Texture->GetCreateInfo().Flags & ETextureCreateFlag::MemoryLess)
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
//	bool bMultiViewDepthStencil = false;
//	if (RPInfo.DepthStencilRenderTarget.DepthStencilTarget)
//	{
//		VkAttachmentDescription& CurrDesc = Desc[NumAttachmentDescriptions];
//		FMemory::Memzero(CurrDesc);
//		FVulkanTexture* Texture = ResourceCast(RPInfo.DepthStencilRenderTarget.DepthStencilTarget);
//		check(Texture);
//		const FRHITextureDesc& TextureInfo = Texture->GetDesc();
//		bMultiViewDepthStencil = (Texture->GetNumberOfArrayLevels() > 1) && !Texture->GetDesc().IsTextureCube();
//		CurrDesc.samples = static_cast<VkSampleCountFlagBits>(RPInfo.DepthStencilRenderTarget.DepthStencilTarget->GetNumSamples());
//		// CustomResolveSubpass can have targets with a different NumSamples
//		HLVM_ENSURE(!NumSamples || CurrDesc.samples == NumSamples || RPInfo.SubpassHint == ESubpassHint::CustomResolveSubpass);
//		NumSamples = CurrDesc.samples;
//		CurrDesc.format = UEToVkTextureFormat(RPInfo.DepthStencilRenderTarget.DepthStencilTarget->GetFormat(), false);
//		CurrDesc.loadOp = RenderTargetLoadActionToVulkan(GetLoadAction(GetDepthActions(RPInfo.DepthStencilRenderTarget.Action)));
//		CurrDesc.stencilLoadOp = RenderTargetLoadActionToVulkan(GetLoadAction(GetStencilActions(RPInfo.DepthStencilRenderTarget.Action)));
//		bFoundClearOp = bFoundClearOp || (CurrDesc.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR || CurrDesc.stencilLoadOp == VK_ATTACHMENT_LOAD_OP_CLEAR);
//
//		CurrDesc.storeOp = RenderTargetStoreActionToVulkan(GetStoreAction(GetDepthActions(RPInfo.DepthStencilRenderTarget.Action)));
//		CurrDesc.stencilStoreOp = RenderTargetStoreActionToVulkan(GetStoreAction(GetStencilActions(RPInfo.DepthStencilRenderTarget.Action)));
//
//		if (EnumHasAnyFlags(TextureInfo.Flags, TexCreate_Memoryless))
//		{
//			HLVM_ENSURE(CurrDesc.storeOp == VK_ATTACHMENT_STORE_OP_DONT_CARE);
//			HLVM_ENSURE(CurrDesc.stencilStoreOp == VK_ATTACHMENT_STORE_OP_DONT_CARE);
//		}
//
//		FExclusiveDepthStencil ExclusiveDepthStencil = RPInfo.DepthStencilRenderTarget.ExclusiveDepthStencil;
//		if (FVulkanPlatform::RequiresDepthWriteOnStencilClear() &&
//			RPInfo.DepthStencilRenderTarget.Action == EDepthStencilTargetActions::LoadDepthClearStencil_StoreDepthStencil)
//		{
//			ExclusiveDepthStencil = FExclusiveDepthStencil::DepthWrite_StencilWrite;
//			CurrentDepthLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
//			CurrentStencilLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
//		}
//
//		// If the initial != final we need to change the FullHashInfo and use FinalLayout
//		CurrDesc.initialLayout = CurrentDepthLayout;
//		CurrDesc.finalLayout = CurrentDepthLayout;
//		StencilDesc.stencilInitialLayout = CurrentStencilLayout;
//		StencilDesc.stencilFinalLayout = CurrentStencilLayout;
//
//		// We can't have the final layout be UNDEFINED, but it's possible that we get here from a transient texture
//		// where the stencil was never used yet.  We can set the layout to whatever we want, the next transition will
//		// happen from UNDEFINED anyhow.
//		if (CurrentDepthLayout == VK_IMAGE_LAYOUT_UNDEFINED)
//		{
//			// Unused image aspects with a LoadOp but undefined layout should just remain untouched
//			if (!RPInfo.DepthStencilRenderTarget.ExclusiveDepthStencil.IsUsingDepth() &&
//				InDevice.GetOptionalExtensions().HasEXTLoadStoreOpNone &&
//				(CurrDesc.loadOp == VK_ATTACHMENT_LOAD_OP_LOAD))
//			{
//				CurrDesc.loadOp = VK_ATTACHMENT_LOAD_OP_NONE_KHR;
//			}
//
//			check(CurrDesc.storeOp == VK_ATTACHMENT_STORE_OP_DONT_CARE);
//			CurrDesc.finalLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
//		}
//		if (CurrentStencilLayout == VK_IMAGE_LAYOUT_UNDEFINED)
//		{
//			// Unused image aspects with a LoadOp but undefined layout should just remain untouched
//			if (!RPInfo.DepthStencilRenderTarget.ExclusiveDepthStencil.IsUsingStencil() &&
//				InDevice.GetOptionalExtensions().HasEXTLoadStoreOpNone &&
//				(CurrDesc.stencilLoadOp == VK_ATTACHMENT_LOAD_OP_LOAD))
//			{
//				CurrDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_NONE_KHR;
//			}
//
//			check(CurrDesc.stencilStoreOp == VK_ATTACHMENT_STORE_OP_DONT_CARE);
//			StencilDesc.stencilFinalLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
//		}
//
//		DepthReference.attachment = NumAttachmentDescriptions;
//		DepthReference.layout = CurrentDepthLayout;
//		StencilReference.stencilLayout = CurrentStencilLayout;
//
//		if (GRHISupportsDepthStencilResolve && CurrDesc.samples > VK_SAMPLE_COUNT_1_BIT && RPInfo.DepthStencilRenderTarget.ResolveTarget)
//		{
//			Desc[NumAttachmentDescriptions + 1] = Desc[NumAttachmentDescriptions];
//			Desc[NumAttachmentDescriptions + 1].samples = VK_SAMPLE_COUNT_1_BIT;
//			Desc[NumAttachmentDescriptions + 1].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
//			Desc[NumAttachmentDescriptions + 1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
//			Desc[NumAttachmentDescriptions + 1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
//			Desc[NumAttachmentDescriptions + 1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
//			DepthStencilResolveReference.attachment = NumAttachmentDescriptions + 1;
//			DepthStencilResolveReference.layout = CurrentDepthLayout;
//			// NumColorAttachments was incremented after the last color attachment
//			ensureMsgf(NumColorAttachments < 16, TEXT("Must have room for depth resolve bit"));
//			CompatibleHashInfo.AttachmentsToResolve |= (uint16)(1 << NumColorAttachments);
//			++NumAttachmentDescriptions;
//			bHasDepthStencilResolve = true;
//		}
//
//		FullHashInfo.LoadOps[MaxSimultaneousRenderTargets] = CurrDesc.loadOp;
//		FullHashInfo.LoadOps[MaxSimultaneousRenderTargets + 1] = CurrDesc.stencilLoadOp;
//		FullHashInfo.StoreOps[MaxSimultaneousRenderTargets] = CurrDesc.storeOp;
//		FullHashInfo.StoreOps[MaxSimultaneousRenderTargets + 1] = CurrDesc.stencilStoreOp;
//		FullHashInfo.InitialLayout[MaxSimultaneousRenderTargets] = CurrentDepthLayout;
//		FullHashInfo.InitialLayout[MaxSimultaneousRenderTargets + 1] = CurrentStencilLayout;
//		CompatibleHashInfo.Formats[MaxSimultaneousRenderTargets] = CurrDesc.format;
//
//		++NumAttachmentDescriptions;
//
//		bHasDepthStencil = true;
//
//		if (bSetExtent)
//		{
//			// Depth can be greater or equal to color. Clamp to the smaller size.
//			Extent.Extent3D.width = FMath::Min<uint32>(Extent.Extent3D.width, TextureInfo.Extent.X);
//			Extent.Extent3D.height = FMath::Min<uint32>(Extent.Extent3D.height, TextureInfo.Extent.Y);
//		}
//		else
//		{
//			bSetExtent = true;
//			Extent.Extent3D.width = TextureInfo.Extent.X;
//			Extent.Extent3D.height = TextureInfo.Extent.Y;
//			Extent.Extent3D.depth = TextureInfo.Depth;
//		}
//	}
//	else if (NumColorRenderTargets == 0)
//	{
//		// No Depth and no color, it's a raster-only pass so make sure the renderArea will be set up properly
//		checkf(RPInfo.ResolveRect.IsValid(), TEXT("For raster-only passes without render targets, ResolveRect has to contain the render area"));
//		bSetExtent = true;
//		Offset.Offset3D.x = RPInfo.ResolveRect.X1;
//		Offset.Offset3D.y = RPInfo.ResolveRect.Y1;
//		Offset.Offset3D.z = 0;
//		Extent.Extent3D.width = RPInfo.ResolveRect.X2 - RPInfo.ResolveRect.X1;
//		Extent.Extent3D.height = RPInfo.ResolveRect.Y2 - RPInfo.ResolveRect.Y1;
//		Extent.Extent3D.depth = 1;
//	}
//
//	if (GRHISupportsAttachmentVariableRateShading && RPInfo.ShadingRateTexture)
//	{
//		FVulkanTexture* Texture = ResourceCast(RPInfo.ShadingRateTexture);
//		check(Texture->GetFormat() == GRHIVariableRateShadingImageFormat);
//
//		VkAttachmentDescription& CurrDesc = Desc[NumAttachmentDescriptions];
//		FMemory::Memzero(CurrDesc);
//
//		const VkImageLayout VRSLayout = GetVRSImageLayout();
//
//		CurrDesc.flags = 0;
//		CurrDesc.format = UEToVkTextureFormat(RPInfo.ShadingRateTexture->GetFormat(), false);
//		CurrDesc.samples = static_cast<VkSampleCountFlagBits>(RPInfo.ShadingRateTexture->GetNumSamples());
//		CurrDesc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
//		CurrDesc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
//		CurrDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
//		CurrDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
//		CurrDesc.initialLayout = VRSLayout;
//		CurrDesc.finalLayout = VRSLayout;
//
//		FragmentDensityReference.attachment = NumAttachmentDescriptions;
//		FragmentDensityReference.layout = VRSLayout;
//
//		FullHashInfo.LoadOps[MaxSimultaneousRenderTargets + 2] = CurrDesc.stencilLoadOp;
//		FullHashInfo.StoreOps[MaxSimultaneousRenderTargets + 2] = CurrDesc.stencilStoreOp;
//		FullHashInfo.InitialLayout[MaxSimultaneousRenderTargets + 2] = VRSLayout;
//		CompatibleHashInfo.Formats[MaxSimultaneousRenderTargets + 1] = CurrDesc.format;
//
//		++NumAttachmentDescriptions;
//		bHasFragmentDensityAttachment = true;
//	}
//
//	SubpassHint = RPInfo.SubpassHint;
//	CompatibleHashInfo.SubpassHint = (uint8)RPInfo.SubpassHint;
//
//	CompatibleHashInfo.NumSamples = NumSamples;
//	CompatibleHashInfo.MultiViewCount = MultiViewCount;
//	// Depth prepass has no color RTs but has a depth attachment that must be multiview
//	if (MultiViewCount > 1 && !bMultiviewRenderTargets && !(NumColorRenderTargets == 0 && bMultiViewDepthStencil))
//	{
//		UE_LOG(LogVulkan, Error, TEXT("Non multiview textures on a multiview layout!"));
//	}
//
//	RenderPassCompatibleHash = FCrc::MemCrc32(&CompatibleHashInfo, sizeof(CompatibleHashInfo));
//	RenderPassFullHash = FCrc::MemCrc32(&FullHashInfo, sizeof(FullHashInfo), RenderPassCompatibleHash);
//	NumUsedClearValues = bFoundClearOp ? NumAttachmentDescriptions : 0;
//	bCalculatedHash = true;
}
