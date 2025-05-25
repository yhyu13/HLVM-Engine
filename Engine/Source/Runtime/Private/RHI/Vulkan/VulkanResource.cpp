/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#include "VulkanResourcePost.h"
#include "RHI/Vulkan/IVulkanDynamicRHI.h"

FVulkanMinimalContext::FVulkanMinimalContext()
{
	RHI::GetDynamicRHI<IVulkanDynamicRHI>()->SetVulkanMinimalContext(this);
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
		FVulkanTextureRef						  Texture = ColorRTBinding.RenderTarget;
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
		FVulkanTextureRef Texture = RPInfo.DepthStencilRenderTarget.DepthStencilTarget;
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
		EPixelFormat RHIFormat = PSOInfo.RenderTargetFormats[Index];
		if (RHIFormat != EPixelFormat::None)
		{
			// With a CustomResolveSubpass last color attachment is a resolve target
			bool bCustomResolveAttachment = (Index == (PSOInfo.RenderTargetsEnabled - 1)) && PSOInfo.SubpassHint == ESubpassHint::CustomResolve;

			VkAttachmentDescription& CurrDesc = Desc[NumAttachmentDescriptions];
			CurrDesc.samples = bCustomResolveAttachment ? VK_SAMPLE_COUNT_1_BIT : static_cast<VkSampleCountFlagBits>(NumSamples);
			CurrDesc.format = VulkanRHI::VulkanFormatFromRHIFormat(RHIFormat, PSOInfo.RenderTargetFlags[Index] & ETextureCreateFlag::SRGB);
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

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"

FVulkanView::FVulkanView(VkDescriptorType InDescriptorType)
{
	// BindlessHandle = LogicalDevice.GetBindlessDescriptorManager()->ReserveDescriptor(InDescriptorType);
}

FVulkanView::~FVulkanView()
{
	Invalidate();

	if (BindlessHandle.IsValid())
	{
		// LogicalDevice.GetDeferredDeletionQueue().EnqueueBindlessHandle(BindlessHandle);
		BindlessHandle = FRHIDescriptorHandle();
	}
}

void FVulkanView::Invalidate()
{
	// Carry forward its initialized state
	const bool bIsInitialized = IsInitialized();

	switch (GetViewType())
	{
		default:
		case EType::Invalid:
			HLVM_ASSERT(false);
			break;

		case EType::TypedBuffer:
			// LogicalDevice.GetDeferredDeletionQueue().EnqueueResource(VulkanRHI::FDeferredDeletionQueue2::EType::BufferView, Storage.Get<FTypedBufferView>().View);
			break;

		case EType::Texture:
			// LogicalDevice.GetDeferredDeletionQueue().EnqueueResource(VulkanRHI::FDeferredDeletionQueue2::EType::ImageView, Storage.Get<FTextureView>().View);
			break;

		case EType::StructuredBuffer:
			// Nothing to do
			break;

		case EType::AccelerationStructure:
			// LogicalDevice.GetDeferredDeletionQueue().EnqueueResource(VulkanRHI::FDeferredDeletionQueue2::EType::AccelerationStructure, Storage.Get<FAccelerationStructureView>().Handle);
			break;
	}

	Storage.emplace<FInvalidatedState>();
	std::get<FInvalidatedState>(Storage).bInitialized = bIsInitialized;
}

FVulkanView* FVulkanView::InitAsTypedBufferView(FVulkanBufferRef Buffer, EPixelFormat RHIFormat, TUINT32 InOffset, TUINT32 InSize)
{
	//	// We will need a deferred update if the descriptor was already in use
	//	const bool bImmediateUpdate = !IsInitialized();
	//
	//	HLVM_ASSERT(GetViewType() == EType::Invalid);
	//	Storage.emplace<FTypedBufferView>();
	//	FTypedBufferView& TBV = std::get<FTypedBufferView>(Storage);
	//
	//	const TUINT32 TotalOffset = Buffer->GetOffset() + InOffset;
	//
	//	HLVM_ASSERT(RHIFormat != PF_Unknown);
	//	VkFormat VKFormat = GVulkanBufferFormat[RHIFormat];
	//	HLVM_ASSERT(Format != VK_FORMAT_UNDEFINED);
	//
	//	VkBufferViewCreateInfo ViewInfo;
	//	ZeroVulkanStruct(ViewInfo, VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO);
	//	ViewInfo.buffer = Buffer->GetHandle();
	//	ViewInfo.offset = TotalOffset;
	//	ViewInfo.format = Format;
	//
	//	const TUINT32 TypeSize =  VulkanRHI::GetNumBitsPerPixel(Format) / 8u;
	//	// View size has to be a multiple of element size
	//	// Commented out because there are multiple places in the high level rendering code which re-purpose buffers for a new format while there are still
	//	// views with the old format lying around, and then lock them with a size computed based on the new stride, triggering this assert when the old views
	//	// are re-created. These places need to be fixed before re-enabling this HLVM_ASSERT (UE-211785).
	//	//HLVM_ASSERT(IsAligned(InSize, TypeSize));
	//
	//	//#todo-rco: Revisit this if buffer views become VK_BUFFER_USAGE_STORAGE_BUFFER_BIT instead of VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT
	//	const VkPhysicalDeviceLimits& Limits = LogicalDevice.GetLimits();
	//	const uint64 MaxSize = (uint64)Limits.maxTexelBufferElements * TypeSize;
	//	ViewInfo.range = FMath::Min<uint64>(InSize, MaxSize);
	//	// TODO: add a HLVM_ASSERT() for exceeding MaxSize, to catch code which blindly makes views without checking the platform limits.
	//
	//	HLVM_ASSERT(Buffer->GetBufferUsageFlags() & (VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT));
	//	HLVM_ASSERT(IsAligned(InOffset, Limits.minTexelBufferOffsetAlignment));
	//
	//	VERIFYVULKANRESULT(VulkanRHI::vkCreateBufferView(LogicalDevice->GetHandle(), &ViewInfo, VulkanRHI::VULKAN_CPU_ALLOCATOR, &TBV.View));
	//
	//	TBV.bVolatile = Buffer->IsVolatile();
	//	if (!TBV.bVolatile && UseVulkanDescriptorCache())
	//	{
	//		TBV.ViewId = ++GVulkanBufferViewHandleIdCounter;
	//	}
	//
	//	INC_DWORD_STAT(STAT_VulkanNumBufferViews);
	//	// :todo-jn: the buffer view is actually not needed in bindless anymore
	//
	//	LogicalDevice.GetBindlessDescriptorManager()->UpdateTexelBuffer(BindlessHandle, ViewInfo, bImmediateUpdate);

	return this;
}

FVulkanView* FVulkanView::InitAsTextureView(
	VkImage InImage, VkImageViewType ViewType, VkImageAspectFlags AspectFlags, EPixelFormat RHIFormat, VkFormat VKFormat, TUINT32 FirstMip, TUINT32 NumMips, TUINT32 ArraySliceIndex, TUINT32 NumArraySlices, bool bUseIdentitySwizzle, VkImageUsageFlags ImageUsageFlags, VkSamplerYcbcrConversion SamplerYcbcrConversion)
{
	// We will need a deferred update if the descriptor was already in use
	const bool bImmediateUpdate = !IsInitialized();

	HLVM_ASSERT(GetViewType() == EType::Invalid);
	Storage.emplace<FTextureView>();
	FTextureView& TV = std::get<FTextureView>(Storage);

	VkImageViewCreateInfo ViewInfo;
	VulkanRHI::ZeroVulkanStruct(&ViewInfo, VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);
	ViewInfo.image = InImage;
	ViewInfo.viewType = ViewType;
	ViewInfo.format = VKFormat;

	VkSamplerYcbcrConversionInfo SamplerYcbcrConversionInfo;
	if (SamplerYcbcrConversion)
	{
		VulkanRHI::ZeroVulkanStruct(&SamplerYcbcrConversionInfo, VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO);
		SamplerYcbcrConversionInfo.conversion = SamplerYcbcrConversion;
		SamplerYcbcrConversionInfo.pNext = ViewInfo.pNext;
		ViewInfo.pNext = &SamplerYcbcrConversionInfo;
	}

	if (bUseIdentitySwizzle)
	{
		ViewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		ViewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		ViewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		ViewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	}
	else
	{
		ViewInfo.components = VulkanRHI::VulkanFormatComponentMappingFromRHIFormat(RHIFormat);
	}

	ViewInfo.subresourceRange.aspectMask = AspectFlags;
	ViewInfo.subresourceRange.baseMipLevel = FirstMip;
	HLVM_ENSURE(NumMips != 0xFFFFFFFF);
	ViewInfo.subresourceRange.levelCount = NumMips;

	HLVM_ENSURE(ArraySliceIndex != 0xFFFFFFFF);
	HLVM_ENSURE(NumArraySlices != 0xFFFFFFFF);
	ViewInfo.subresourceRange.baseArrayLayer = ArraySliceIndex;
	ViewInfo.subresourceRange.layerCount = NumArraySlices;

	// HACK.  DX11 on PC currently uses a D24S8 depthbuffer and so needs an X24_G8 SRV to visualize stencil.
	// So take that as our cue to visualize stencil.  In the future, the platform independent code will have a real format
	// instead of PF_DepthStencil, so the cross-platform code could figure out the proper format to pass in for this.
	if (RHI::HasStencil(RHIFormat))
	{
		HLVM_ENSURE(VulkanRHI::VulkanFormatHasStencil(ViewInfo.format));
		ViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
	}

	// Inform the driver the view will only be used with a subset of usage flags (to help performance and/or compatibility)
	VkImageViewUsageCreateInfo ImageViewUsageCreateInfo;
	if (ImageUsageFlags != 0)
	{
		VulkanRHI::ZeroVulkanStruct(&ImageViewUsageCreateInfo, VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO);
		ImageViewUsageCreateInfo.usage = ImageUsageFlags;

		ImageViewUsageCreateInfo.pNext = C_C(void*, ViewInfo.pNext);
		ViewInfo.pNext = &ImageViewUsageCreateInfo;
	}

	VULKAN_ENSURE(VulkanRHI::vkCreateImageView(LogicalDevice->GetHandle(), &ViewInfo, VulkanRHI::VULKAN_CPU_ALLOCATOR, &TV.View));

	TV.Image = InImage;

	const bool bDepthOrStencilAspect = (AspectFlags & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) != 0;

	// TODO
	//LogicalDevice.GetBindlessDescriptorManager()->UpdateImage(BindlessHandle, TV.View, bDepthOrStencilAspect, bImmediateUpdate);

	return this;
}

FVulkanView* FVulkanView::InitAsStructuredBufferView(FVulkanBufferRef Buffer, TUINT32 InOffset, TUINT32 InSize)
{
	//	// We will need a deferred update if the descriptor was already in use
	//	const bool bImmediateUpdate = !IsInitialized();
	//
	//	HLVM_ASSERT(GetViewType() == EType::Invalid);
	//	Storage.Emplace<FStructuredBufferView>();
	//	FStructuredBufferView& SBV = Storage.Get<FStructuredBufferView>();
	//
	//	const TUINT32 TotalOffset = Buffer->GetOffset() + InOffset;
	//
	//	SBV.Buffer = Buffer->GetHandle();
	//	SBV.HandleId = Buffer->GetCurrentAllocation().HandleId;
	//	SBV.Offset = TotalOffset;
	//
	//	// :todo-jn: Volatile buffers use temporary allocations that can be smaller than the buffer creation size.  Check if the savings are still worth it.
	//	if (Buffer->IsVolatile())
	//	{
	//		InSize = FMath::Min<uint64>(InSize, Buffer->GetCurrentSize());
	//	}
	//
	//	SBV.Size = InSize;
	//
	//	LogicalDevice.GetBindlessDescriptorManager()->UpdateBuffer(BindlessHandle, Buffer->GetHandle(), TotalOffset, InSize, bImmediateUpdate);

	return this;
}

FVulkanView* FVulkanView::InitAsAccelerationStructureView(FVulkanBufferRef Buffer, TUINT32 Offset, TUINT32 Size)
{
	//	HLVM_ASSERT(GetViewType() == EType::Invalid);
	//	Storage.Emplace<FAccelerationStructureView>();
	//	FAccelerationStructureView& ASV = Storage.Get<FAccelerationStructureView>();
	//
	//	VkAccelerationStructureCreateInfoKHR CreateInfo;
	//	ZeroVulkanStruct(CreateInfo, VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR);
	//	CreateInfo.buffer = Buffer->GetHandle();
	//	CreateInfo.offset = Buffer->GetOffset() + Offset;
	//	CreateInfo.size = Size;
	//	CreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	//
	//	VERIFYVULKANRESULT(VulkanDynamicAPI::vkCreateAccelerationStructureKHR(LogicalDevice->GetHandle(), &CreateInfo, VulkanRHI::VULKAN_CPU_ALLOCATOR, &ASV.Handle));
	//
	//	LogicalDevice.GetBindlessDescriptorManager()->UpdateAccelerationStructure(BindlessHandle, ASV.Handle);

	return this;
}
#pragma clang diagnostic pop
