/**
* Copyright (c) 2025. MIT License. All rights reserved.
 */

#include "RHI/DynamicRHI.h"

// Extern
TNoNullablePtr<FDynamicRHI> GDynamicRHI;

void FRHIRenderPassInfo::Validate() const
{
#if !HLVM_BUILD_RELEASE
	TUINT32 NumSamples = 0;
	TUINT32 ColorIndex = 0;
	for (; ColorIndex < RHI::MAX_RT_ATTACHMENTS; ++ColorIndex)
	{
		const ColorRTBinding& Entry = ColorRenderTargets[ColorIndex];
		if (Entry.RenderTarget)
		{
			// Ensure NumSamples matches amongst all color RTs
			if (NumSamples == 0)
			{
				NumSamples = Entry.RenderTarget->GetNumSamples();
			}
			else
			{
				// CustomResolveSubpass can have targets with a different NumSamples
				HLVM_ENSURE_F(Entry.RenderTarget->GetNumSamples() == NumSamples || SubpassHint == ESubpassHint::CustomResolve,
					TXT("RenderTarget have inconsistent NumSamples: first {}, then {}"), NumSamples, Entry.RenderTarget->GetNumSamples());
			}

			ERenderTargetStoreAction Store = RHI::GetStoreAction(Entry.Action);
			// Don't try to resolve a non-msaa
			HLVM_ENSURE(Store != ERenderTargetStoreAction::MultisampleResolve || Entry.RenderTarget->GetNumSamples() > 1);
			// Don't resolve to null
			HLVM_ENSURE(Store != ERenderTargetStoreAction::MultisampleResolve || Entry.ResolveTarget);

			if (Entry.ResolveTarget)
			{
				//HLVM_ENSURE(Store == ERenderTargetStoreAction::EMultisampleResolve);
			}
		}
		else
		{
			break;
		}
	}

	TUINT32 NumColorRenderTargets = ColorIndex;
	for (; ColorIndex < RHI::MAX_RT_ATTACHMENTS; ++ColorIndex)
	{
		// Gap in the sequence of valid render targets (ie RT0, null, RT2, ...)
		HLVM_ENSURE_F(!ColorRenderTargets[ColorIndex].RenderTarget, TXT("Missing color render target on slot %d"), ColorIndex - 1);
	}

	if (DepthStencilRenderTarget.DepthStencilTarget)
	{
		// Ensure NumSamples matches with color RT
		if (NumSamples != 0)
		{
			HLVM_ENSURE(DepthStencilRenderTarget.DepthStencilTarget->GetNumSamples() == NumSamples);
		}
		ERenderTargetStoreAction DepthStore = RHI::GetStoreAction(RHI::GetDepthActions(DepthStencilRenderTarget.Action));
		ERenderTargetStoreAction StencilStore = RHI::GetStoreAction(RHI::GetStencilActions(DepthStencilRenderTarget.Action));
		bool bIsMSAAResolve = (DepthStore == ERenderTargetStoreAction::MultisampleResolve) || (StencilStore == ERenderTargetStoreAction::MultisampleResolve);
		// Don't try to resolve a non-msaa
		HLVM_ENSURE(!bIsMSAAResolve || DepthStencilRenderTarget.DepthStencilTarget->GetNumSamples() > 1);
		// Don't resolve to null
		//HLVM_ENSURE(DepthStencilRenderTarget.ResolveTarget || DepthStore != ERenderTargetStoreAction::EStore);

		// Don't write to depth if read-only
		//HLVM_ENSURE(DepthStencilRenderTarget.ExclusiveDepthStencil.IsDepthWrite() || DepthStore != ERenderTargetStoreAction::EStore);
		// This is not true for stencil. VK and Metal specify that the DontCare store action MAY leave the attachment in an undefined state.
		/*HLVM_ENSURE(DepthStencilRenderTarget.ExclusiveDepthStencil.IsStencilWrite() || StencilStore != ERenderTargetStoreAction::EStore);*/

		// If we have a depthstencil target we MUST Store it or it will be undefined after rendering.
		if (DepthStencilRenderTarget.DepthStencilTarget->GetFormat() != EPixelFormat::D24_UNorm_S8_UInt)
		{
			// If this is DepthStencil we must store it out unless we are absolutely sure it will never be used again.
			// it is valid to use a depthbuffer for performance and not need the results later.
			//HLVM_ENSURE(StencilStore == ERenderTargetStoreAction::EStore);
		}

		if (DepthStencilRenderTarget.ExclusiveDepthStencil.IsDepthWrite())
		{
			// this check is incorrect for mobile, depth/stencil is intermediate and we don't want to store it to main memory
			//HLVM_ENSURE(DepthStore == ERenderTargetStoreAction::EStore);
		}

		if (DepthStencilRenderTarget.ExclusiveDepthStencil.IsStencilWrite())
		{
			// this check is incorrect for mobile, depth/stencil is intermediate and we don't want to store it to main memory
			//HLVM_ENSURE(StencilStore == ERenderTargetStoreAction::EStore);
		}

		if (SubpassHint == ESubpassHint::DepthReading || SubpassHint == ESubpassHint::CustomResolve)
		{
			// for depth read sub-pass
			// 1. render pass must have depth target
			// 2. depth target must support InputAttachement
			HLVM_ENSURE(DepthStencilRenderTarget.DepthStencilTarget->GetCreateFlags() & ETextureCreateFlag::InputAttachment);
		}

		if (DepthStencilRenderTarget.ResolveTarget && DepthStencilRenderTarget.ResolveTarget != DepthStencilRenderTarget.DepthStencilTarget)
		{
			// For depth resolve
			// 1. RHI must support depth stencil resolve
			// 2. Must be using MSAA resolve
			// 3. Resolve target sample count must be 1
			// 4. Resolve target format must be the same as the MSAA target format
			//HLVM_ENSURE_F(GRHISupportsDepthStencilResolve, TXT("Attempted to resolve depth/stencil target but feature is not supported."));
			HLVM_ENSURE_F(bIsMSAAResolve, TXT("Depth/stencil resolve target is bound but resolve was not requested."));
			HLVM_ENSURE_F(DepthStencilRenderTarget.ResolveTarget->GetNumSamples() == 1, TXT("Depth/stencil resolve targets must have a sample count of 1."));
			HLVM_ENSURE_F(DepthStencilRenderTarget.ResolveTarget->GetFormat() == DepthStencilRenderTarget.DepthStencilTarget->GetFormat(),
				TXT("Depth/stencil resolve targets must have the same format as the MSAA target."));
		}
	}
	else
	{
		HLVM_ENSURE(DepthStencilRenderTarget.Action == EDepthStencilTargetActions::DontLoad_DontStore);
		HLVM_ENSURE(DepthStencilRenderTarget.ExclusiveDepthStencil == FExclusiveDepthStencil::DepthNop_StencilNop);
		HLVM_ENSURE(SubpassHint != ESubpassHint::DepthReading);
		HLVM_ENSURE(SubpassHint != ESubpassHint::CustomResolve);
	}
#endif
}
