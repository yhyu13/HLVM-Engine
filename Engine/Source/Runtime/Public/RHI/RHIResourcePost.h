/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "RHIResource.h"

class FRHIRenderTargetView
{
public:
	FRHIRenderTargetView() = default;
	FRHIRenderTargetView(FRHIRenderTargetView&&) = default;
	FRHIRenderTargetView(const FRHIRenderTargetView&) = default;
	FRHIRenderTargetView& operator=(FRHIRenderTargetView&&) = default;
	FRHIRenderTargetView& operator=(const FRHIRenderTargetView&) = default;

	// common case
	explicit FRHIRenderTargetView(FRHITextureRef InTexture, ERenderTargetLoadAction InLoadAction)
		: Texture(InTexture), LoadAction(InLoadAction), StoreAction(ERenderTargetStoreAction::Store)
	{
	}

	// common case
	explicit FRHIRenderTargetView(FRHITextureRef InTexture, ERenderTargetLoadAction InLoadAction, TUINT8 InMipIndex, TINT8 InArraySliceIndex)
		: Texture(InTexture), MipIndex(InMipIndex), ArraySliceIndex(InArraySliceIndex), LoadAction(InLoadAction), StoreAction(ERenderTargetStoreAction::Store)
	{
	}

	explicit FRHIRenderTargetView(FRHITextureRef InTexture, TUINT8 InMipIndex, TINT8 InArraySliceIndex, ERenderTargetLoadAction InLoadAction, ERenderTargetStoreAction InStoreAction)
		: Texture(InTexture), MipIndex(InMipIndex), ArraySliceIndex(InArraySliceIndex), LoadAction(InLoadAction), StoreAction(InStoreAction)
	{
	}

	bool operator==(const FRHIRenderTargetView& Other) const
	{
		return Texture == Other.Texture && MipIndex == Other.MipIndex && ArraySliceIndex == Other.ArraySliceIndex && LoadAction == Other.LoadAction && StoreAction == Other.StoreAction;
	}

public:
	FRHITextureRef Texture = nullptr;
	TUINT8		   MipIndex = 0;

	/** Array slice or texture cube face.  Only valid if texture resource was created with TexCreate_TargetArraySlicesIndependently! */
	TINT8 ArraySliceIndex = -1;

	ERenderTargetLoadAction	 LoadAction = ERenderTargetLoadAction::DontCare;
	ERenderTargetStoreAction StoreAction = ERenderTargetStoreAction::DontCare;
};

class FRHIDepthStencilRenderTargetView
{
public:
	// accessor to prevent write access to StencilStoreAction
	ERenderTargetStoreAction GetStencilStoreAction() const { return StencilStoreAction; }
	// accessor to prevent write access to DepthStencilAccess
	FExclusiveDepthStencil GetDepthStencilAccess() const { return DepthStencilAccess; }

	explicit FRHIDepthStencilRenderTargetView()
		: Texture(nullptr), DepthLoadAction(ERenderTargetLoadAction::DontCare), DepthStoreAction(ERenderTargetStoreAction::DontCare), StencilLoadAction(ERenderTargetLoadAction::DontCare), StencilStoreAction(ERenderTargetStoreAction::DontCare), DepthStencilAccess(FExclusiveDepthStencil::DepthNop_StencilNop)
	{
		Validate();
	}

	// common case
	explicit FRHIDepthStencilRenderTargetView(FRHITextureRef InTexture, ERenderTargetLoadAction InLoadAction, ERenderTargetStoreAction InStoreAction)
		: Texture(InTexture), DepthLoadAction(InLoadAction), DepthStoreAction(InStoreAction), StencilLoadAction(InLoadAction), StencilStoreAction(InStoreAction), DepthStencilAccess(FExclusiveDepthStencil::DepthWrite_StencilWrite)
	{
		Validate();
	}

	explicit FRHIDepthStencilRenderTargetView(FRHITextureRef InTexture, ERenderTargetLoadAction InLoadAction, ERenderTargetStoreAction InStoreAction, FExclusiveDepthStencil InDepthStencilAccess)
		: Texture(InTexture), DepthLoadAction(InLoadAction), DepthStoreAction(InStoreAction), StencilLoadAction(InLoadAction), StencilStoreAction(InStoreAction), DepthStencilAccess(InDepthStencilAccess)
	{
		Validate();
	}

	explicit FRHIDepthStencilRenderTargetView(FRHITextureRef InTexture, ERenderTargetLoadAction InDepthLoadAction, ERenderTargetStoreAction InDepthStoreAction, ERenderTargetLoadAction InStencilLoadAction, ERenderTargetStoreAction InStencilStoreAction)
		: Texture(InTexture), DepthLoadAction(InDepthLoadAction), DepthStoreAction(InDepthStoreAction), StencilLoadAction(InStencilLoadAction), StencilStoreAction(InStencilStoreAction), DepthStencilAccess(FExclusiveDepthStencil::DepthWrite_StencilWrite)
	{
		Validate();
	}

	explicit FRHIDepthStencilRenderTargetView(FRHITextureRef InTexture, ERenderTargetLoadAction InDepthLoadAction, ERenderTargetStoreAction InDepthStoreAction, ERenderTargetLoadAction InStencilLoadAction, ERenderTargetStoreAction InStencilStoreAction, FExclusiveDepthStencil InDepthStencilAccess)
		: Texture(InTexture), DepthLoadAction(InDepthLoadAction), DepthStoreAction(InDepthStoreAction), StencilLoadAction(InStencilLoadAction), StencilStoreAction(InStencilStoreAction), DepthStencilAccess(InDepthStencilAccess)
	{
		Validate();
	}

	void Validate() const
	{
		// VK and Metal MAY leave the attachment in an undefined state if the StoreAction is DontCare. So we can't assume read-only implies it should be DontCare unless we know for sure it will never be used again.
		// ensureMsgf(DepthStencilAccess.IsDepthWrite() || DepthStoreAction == ERenderTargetStoreAction::DontCare, TEXT("Depth is read-only, but we are performing a store.  This is a waste on mobile.  If depth can't change, we don't need to store it out again"));
		/*ensureMsgf(DepthStencilAccess.IsStencilWrite() || StencilStoreAction == ERenderTargetStoreAction::DontCare, TEXT("Stencil is read-only, but we are performing a store.  This is a waste on mobile.  If stencil can't change, we don't need to store it out again"));*/
	}

	bool operator==(const FRHIDepthStencilRenderTargetView& Other) const
	{
		return Texture == Other.Texture && DepthLoadAction == Other.DepthLoadAction && DepthStoreAction == Other.DepthStoreAction && StencilLoadAction == Other.StencilLoadAction && StencilStoreAction == Other.StencilStoreAction && DepthStencilAccess == Other.DepthStencilAccess;
	}

public:
	FRHITextureRef Texture;

	ERenderTargetLoadAction	 DepthLoadAction;
	ERenderTargetStoreAction DepthStoreAction;
	ERenderTargetLoadAction	 StencilLoadAction;

private:
	ERenderTargetStoreAction StencilStoreAction;
	FExclusiveDepthStencil	 DepthStencilAccess;
};

// Created from render pass info, Used to generate frame buffers
class FRHIRenderTargetsInfo
{
public:
	// Color Render Targets Info
	FRHIRenderTargetView ColorRenderTarget[RHI::MAX_RT_ATTACHMENTS];
	TUINT32				 NumColorRenderTargets;
	bool				 bClearColor;

	// Color Render Targets Info
	FRHIRenderTargetView ColorResolveRenderTarget[RHI::MAX_RT_ATTACHMENTS];
	bool				 bHasResolveAttachments;

	// Depth/Stencil Render Target Info
	FRHIDepthStencilRenderTargetView DepthStencilRenderTarget;
	// Used when depth resolve is enabled.
	FRHIDepthStencilRenderTargetView DepthStencilResolveRenderTarget;
	bool							 bClearDepth;
	bool							 bClearStencil;

	FRHIRenderTargetsInfo()
		: NumColorRenderTargets(0), bClearColor(false), bHasResolveAttachments(false), bClearDepth(false)
	{
	}

	FRHIRenderTargetsInfo(TUINT32 InNumColorRenderTargets, const FRHIRenderTargetView* InColorRenderTargets, const FRHIDepthStencilRenderTargetView& InDepthStencilRenderTarget)
		: NumColorRenderTargets(InNumColorRenderTargets), bClearColor(InNumColorRenderTargets > 0 && InColorRenderTargets[0].LoadAction == ERenderTargetLoadAction::Clear), bHasResolveAttachments(false), DepthStencilRenderTarget(InDepthStencilRenderTarget), bClearDepth(InDepthStencilRenderTarget.Texture && InDepthStencilRenderTarget.DepthLoadAction == ERenderTargetLoadAction::Clear)
	{
		HLVM_ASSERT(InNumColorRenderTargets == 0 || InColorRenderTargets);
		for (TUINT32 Index = 0; Index < InNumColorRenderTargets; ++Index)
		{
			ColorRenderTarget[Index] = InColorRenderTargets[Index];
		}
	}

	void SetClearDepthStencil(bool bInClearDepth, bool bInClearStencil = false)
	{
		if (bInClearDepth)
		{
			DepthStencilRenderTarget.DepthLoadAction = ERenderTargetLoadAction::Clear;
		}
		if (bInClearStencil)
		{
			DepthStencilRenderTarget.StencilLoadAction = ERenderTargetLoadAction::Clear;
		}
		bClearDepth = bInClearDepth;
		bClearStencil = bInClearStencil;
	}
};

struct FResolveRect
{
	TUINT32 X1 = 0; // MinX
	TUINT32 Y1 = 0; // MinY
	TUINT32 X2 = 0; // MaxX
	TUINT32 Y2 = 0; // MaxY

	bool IsValid() const
	{
		return X1 < X2 && Y1 < Y2;
	}
};

// Structure for describing render pass initialization parameters
class FRHIRenderPassInfo
{
public:
	struct ColorRTBinding
	{
		FRHITextureRef		 RenderTarget;
		FRHITextureRef		 ResolveTarget;
		TINT8				 ArraySliceIndex = -1;
		TUINT8				 MipIndex = 0;
		ERenderTargetActions Action = ERenderTargetActions::DontLoad_DontStore;
	};

	struct DepthStencilRTBinding
	{
		FRHITextureRef			   DepthStencilTarget;
		FRHITextureRef			   ResolveTarget;
		EDepthStencilTargetActions Action = EDepthStencilTargetActions::DontLoad_DontStore;
		FExclusiveDepthStencil	   ExclusiveDepthStencil;
	};

public:
	FRHIRenderPassInfo() = default;

	// Color, no depth, optional resolve, optional mip, optional array slice
	explicit FRHIRenderPassInfo(FRHITextureRef ColorRT, ERenderTargetActions ColorAction, FRHITextureRef ResolveRT = nullptr, TUINT8 InMipIndex = 0, TINT8 InArraySlice = -1)
	{
		HLVM_ASSERT(!(ResolveRT && ResolveRT->IsMultiSampled()));
		HLVM_ASSERT(ColorRT);
		ColorRenderTargets[0].RenderTarget = ColorRT;
		ColorRenderTargets[0].ResolveTarget = ResolveRT;
		ColorRenderTargets[0].ArraySliceIndex = InArraySlice;
		ColorRenderTargets[0].MipIndex = InMipIndex;
		ColorRenderTargets[0].Action = ColorAction;
	}

	// Color MRTs, no depth
	explicit FRHIRenderPassInfo(TUINT32 NumColorRTs, FRHITextureRef ColorRTs[], ERenderTargetActions ColorAction)
	{
		HLVM_ASSERT(NumColorRTs > 0);
		for (TUINT32 Index = 0; Index < NumColorRTs; ++Index)
		{
			HLVM_ASSERT(ColorRTs[Index]);
			ColorRenderTargets[Index].RenderTarget = ColorRTs[Index];
			ColorRenderTargets[Index].ArraySliceIndex = -1;
			ColorRenderTargets[Index].Action = ColorAction;
		}
		DepthStencilRenderTarget.DepthStencilTarget = nullptr;
		DepthStencilRenderTarget.Action = EDepthStencilTargetActions::DontLoad_DontStore;
		DepthStencilRenderTarget.ExclusiveDepthStencil = FExclusiveDepthStencil::DepthNop_StencilNop;
		DepthStencilRenderTarget.ResolveTarget = nullptr;
	}

	// Color MRTs, no depth
	explicit FRHIRenderPassInfo(TUINT32 NumColorRTs, FRHITextureRef ColorRTs[], ERenderTargetActions ColorAction, FRHITextureRef ResolveTargets[])
	{
		HLVM_ASSERT(NumColorRTs > 0);
		for (TUINT32 Index = 0; Index < NumColorRTs; ++Index)
		{
			HLVM_ASSERT(ColorRTs[Index]);
			ColorRenderTargets[Index].RenderTarget = ColorRTs[Index];
			ColorRenderTargets[Index].ResolveTarget = ResolveTargets[Index];
			ColorRenderTargets[Index].ArraySliceIndex = -1;
			ColorRenderTargets[Index].MipIndex = 0;
			ColorRenderTargets[Index].Action = ColorAction;
		}
		DepthStencilRenderTarget.DepthStencilTarget = nullptr;
		DepthStencilRenderTarget.Action = EDepthStencilTargetActions::DontLoad_DontStore;
		DepthStencilRenderTarget.ExclusiveDepthStencil = FExclusiveDepthStencil::DepthNop_StencilNop;
		DepthStencilRenderTarget.ResolveTarget = nullptr;
	}

	// Color MRTs and depth
	explicit FRHIRenderPassInfo(TUINT32 NumColorRTs, FRHITextureRef ColorRTs[], ERenderTargetActions ColorAction, FRHITextureRef DepthRT, EDepthStencilTargetActions DepthActions, FExclusiveDepthStencil InEDS = FExclusiveDepthStencil::DepthWrite_StencilWrite)
	{
		HLVM_ASSERT(NumColorRTs > 0);
		for (TUINT32 Index = 0; Index < NumColorRTs; ++Index)
		{
			HLVM_ASSERT(ColorRTs[Index]);
			ColorRenderTargets[Index].RenderTarget = ColorRTs[Index];
			ColorRenderTargets[Index].ResolveTarget = nullptr;
			ColorRenderTargets[Index].ArraySliceIndex = -1;
			ColorRenderTargets[Index].MipIndex = 0;
			ColorRenderTargets[Index].Action = ColorAction;
		}
		HLVM_ASSERT(DepthRT);
		DepthStencilRenderTarget.DepthStencilTarget = DepthRT;
		DepthStencilRenderTarget.ResolveTarget = nullptr;
		DepthStencilRenderTarget.Action = DepthActions;
		DepthStencilRenderTarget.ExclusiveDepthStencil = InEDS;
	}

	// Color MRTs and depth
	explicit FRHIRenderPassInfo(TUINT32 NumColorRTs, FRHITextureRef ColorRTs[], ERenderTargetActions ColorAction, FRHITextureRef ResolveRTs[], FRHITextureRef DepthRT, EDepthStencilTargetActions DepthActions, FRHITextureRef ResolveDepthRT, FExclusiveDepthStencil InEDS = FExclusiveDepthStencil::DepthWrite_StencilWrite)
	{
		HLVM_ASSERT(NumColorRTs > 0);
		for (TUINT32 Index = 0; Index < NumColorRTs; ++Index)
		{
			HLVM_ASSERT(!ResolveRTs[Index] || ResolveRTs[Index]->IsMultiSampled());
			HLVM_ASSERT(ColorRTs[Index]);
			ColorRenderTargets[Index].RenderTarget = ColorRTs[Index];
			ColorRenderTargets[Index].ResolveTarget = ResolveRTs[Index];
			ColorRenderTargets[Index].ArraySliceIndex = -1;
			ColorRenderTargets[Index].MipIndex = 0;
			ColorRenderTargets[Index].Action = ColorAction;
		}
		HLVM_ASSERT(!ResolveDepthRT || ResolveDepthRT->IsMultiSampled());
		HLVM_ASSERT(DepthRT);
		DepthStencilRenderTarget.DepthStencilTarget = DepthRT;
		DepthStencilRenderTarget.ResolveTarget = ResolveDepthRT;
		DepthStencilRenderTarget.Action = DepthActions;
		DepthStencilRenderTarget.ExclusiveDepthStencil = InEDS;
	}

	// Depth, no color
	explicit FRHIRenderPassInfo(FRHITextureRef DepthRT, EDepthStencilTargetActions DepthActions, FRHITextureRef ResolveDepthRT = nullptr, FExclusiveDepthStencil InEDS = FExclusiveDepthStencil::DepthWrite_StencilWrite)
	{
		HLVM_ASSERT(!ResolveDepthRT || ResolveDepthRT->IsMultiSampled());
		HLVM_ASSERT(DepthRT);
		DepthStencilRenderTarget.DepthStencilTarget = DepthRT;
		DepthStencilRenderTarget.ResolveTarget = ResolveDepthRT;
		DepthStencilRenderTarget.Action = DepthActions;
		DepthStencilRenderTarget.ExclusiveDepthStencil = InEDS;
	}

	// Color and depth
	explicit FRHIRenderPassInfo(FRHITextureRef ColorRT, ERenderTargetActions ColorAction, FRHITextureRef DepthRT, EDepthStencilTargetActions DepthActions, FExclusiveDepthStencil InEDS = FExclusiveDepthStencil::DepthWrite_StencilWrite)
	{
		HLVM_ASSERT(ColorRT);
		ColorRenderTargets[0].RenderTarget = ColorRT;
		ColorRenderTargets[0].ResolveTarget = nullptr;
		ColorRenderTargets[0].ArraySliceIndex = -1;
		ColorRenderTargets[0].MipIndex = 0;
		ColorRenderTargets[0].Action = ColorAction;
		HLVM_ASSERT(DepthRT);
		DepthStencilRenderTarget.DepthStencilTarget = DepthRT;
		DepthStencilRenderTarget.ResolveTarget = nullptr;
		DepthStencilRenderTarget.Action = DepthActions;
		DepthStencilRenderTarget.ExclusiveDepthStencil = InEDS;
		FMemory::Memzero(&ColorRenderTargets[1], sizeof(ColorRTBinding) * (RHI::MAX_RT_ATTACHMENTS - 1));
	}

	// Color and depth with resolve
	explicit FRHIRenderPassInfo(FRHITextureRef ColorRT, ERenderTargetActions ColorAction, FRHITextureRef ResolveColorRT,
		FRHITextureRef DepthRT, EDepthStencilTargetActions DepthActions, FRHITextureRef ResolveDepthRT, FExclusiveDepthStencil InEDS = FExclusiveDepthStencil::DepthWrite_StencilWrite)
	{
		HLVM_ASSERT(!ResolveColorRT || ResolveColorRT->IsMultiSampled());
		HLVM_ASSERT(!ResolveDepthRT || ResolveDepthRT->IsMultiSampled());
		HLVM_ASSERT(ColorRT);
		ColorRenderTargets[0].RenderTarget = ColorRT;
		ColorRenderTargets[0].ResolveTarget = ResolveColorRT;
		ColorRenderTargets[0].ArraySliceIndex = -1;
		ColorRenderTargets[0].MipIndex = 0;
		ColorRenderTargets[0].Action = ColorAction;
		HLVM_ASSERT(DepthRT);
		DepthStencilRenderTarget.DepthStencilTarget = DepthRT;
		DepthStencilRenderTarget.ResolveTarget = ResolveDepthRT;
		DepthStencilRenderTarget.Action = DepthActions;
		DepthStencilRenderTarget.ExclusiveDepthStencil = InEDS;
		FMemory::Memzero(&ColorRenderTargets[1], sizeof(ColorRTBinding) * (RHI::MAX_RT_ATTACHMENTS - 1));
	}

	TUINT32 GetNumColorRenderTargets() const
	{
		TUINT32 ColorIndex = 0;
		for (; ColorIndex < RHI::MAX_RT_ATTACHMENTS; ++ColorIndex)
		{
			const ColorRTBinding& Entry = ColorRenderTargets[ColorIndex];
			if (!Entry.RenderTarget)
			{
				break;
			}
		}

		return ColorIndex;
	}

	//	FGraphicsPipelineRenderTargetsInfo ExtractRenderTargetsInfo() const
	//	{
	//		FGraphicsPipelineRenderTargetsInfo RenderTargetsInfo;
	//
	//		RenderTargetsInfo.NumSamples = 1;
	//		TUINT32 RenderTargetIndex = 0;
	//
	//		for (; RenderTargetIndex < RHI::MAX_RT_ATTACHMENTS; ++RenderTargetIndex)
	//		{
	//			FRHITextureRef RenderTarget = ColorRenderTargets[RenderTargetIndex].RenderTarget;
	//			if (!RenderTarget)
	//			{
	//				break;
	//			}
	//
	//			RenderTargetsInfo.RenderTargetFormats[RenderTargetIndex] = (uint8)RenderTarget->GetFormat();
	//			RenderTargetsInfo.RenderTargetFlags[RenderTargetIndex] = RenderTarget->GetFlags();
	//			RenderTargetsInfo.NumSamples |= RenderTarget->GetNumSamples();
	//		}
	//
	//		RenderTargetsInfo.RenderTargetsEnabled = RenderTargetIndex;
	//		for (; RenderTargetIndex < RHI::MAX_RT_ATTACHMENTS; ++RenderTargetIndex)
	//		{
	//			RenderTargetsInfo.RenderTargetFormats[RenderTargetIndex] = PF_Unknown;
	//		}
	//
	//		if (DepthStencilRenderTarget.DepthStencilTarget)
	//		{
	//			RenderTargetsInfo.DepthStencilTargetFormat = DepthStencilRenderTarget.DepthStencilTarget->GetFormat();
	//			RenderTargetsInfo.DepthStencilTargetFlag = DepthStencilRenderTarget.DepthStencilTarget->GetFlags();
	//			RenderTargetsInfo.NumSamples |= DepthStencilRenderTarget.DepthStencilTarget->GetNumSamples();
	//		}
	//		else
	//		{
	//			RenderTargetsInfo.DepthStencilTargetFormat = PF_Unknown;
	//		}
	//
	//		const ERenderTargetActions DepthActions = GetDepthActions(DepthStencilRenderTarget.Action);
	//		const ERenderTargetActions StencilActions = GetStencilActions(DepthStencilRenderTarget.Action);
	//		RenderTargetsInfo.DepthTargetLoadAction = GetLoadAction(DepthActions);
	//		RenderTargetsInfo.DepthTargetStoreAction = GetStoreAction(DepthActions);
	//		RenderTargetsInfo.StencilTargetLoadAction = GetLoadAction(StencilActions);
	//		RenderTargetsInfo.StencilTargetStoreAction = GetStoreAction(StencilActions);
	//		RenderTargetsInfo.DepthStencilAccess = DepthStencilRenderTarget.ExclusiveDepthStencil;
	//
	//		return RenderTargetsInfo;
	//	}

	void ConvertToRenderTargetsInfo(FRHIRenderTargetsInfo& OutRTInfo) const;

	void Validate() const;

public:
	FString				  DebugName;
	ColorRTBinding		  ColorRenderTargets[RHI::MAX_RT_ATTACHMENTS]; // Render targets
	DepthStencilRTBinding DepthStencilRenderTarget;					   // Depth-stencil target
	ESubpassHint		  SubpassHint = ESubpassHint::Default;

	FResolveRect ResolveRect;
	// TODO : occlusion query, multiview, variable shading rate support?
};
