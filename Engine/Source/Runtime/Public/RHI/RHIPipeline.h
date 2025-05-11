/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "RHIPipelinePre.h"

// Structure for describing graphics pipeline state initialization parameters
// struct FGraphicsPSOInitializer
//{
//	FVertexDeclarationRHIRef VertexDeclaration;							  // Vertex declaration
//	FRHIShaderRef			 VertexShader;								  // Vertex shader
//	FRHIShaderRef			 PixelShader;								  // Pixel shader
//	FRHIShaderRef			 GeometryShader;							  // Geometry shader (optional)
//	FRHIShaderRef			 HullShader;								  // Hull shader (optional)
//	FRHIShaderRef			 DomainShader;								  // Domain shader (optional)
//	EPixelFormat			 RenderTargetFormats[RHI::MAX_RT_ATTACHMENTS]; // Formats of the render targets
//	EPixelFormat			 DepthStencilFormat;						  // Format of the depth-stencil target
//	TUINT32					 NumRenderTargets;							  // Number of render targets
//	TUINT32					 SampleCount;								  // Number of samples (for MSAA)
//
//	// Constructor for easy initialization
//	FGraphicsPSOInitializer()
//		: VertexDeclaration(nullptr)
//		, VertexShader(nullptr)
//		, PixelShader(nullptr)
//		, GeometryShader(nullptr)
//		, HullShader(nullptr)
//		, DomainShader(nullptr)
//		, DepthStencilFormat(EPixelFormat::None)
//		, NumRenderTargets(0)
//		, SampleCount(1)
//	{
//		std::memset(RenderTargetFormats, HLVM_ENUM_VALUE(EPixelFormat::None), sizeof(RenderTargetFormats));
//	}
//};

class FGraphicsPSOInitializer
{
public:
	// Can't use TEnumByte<EPixelFormat> as it changes the struct to be non trivially constructible, breaking memset
	using TRenderTargetFormats = TStaticVector<EPixelFormat, RHI::MAX_RT_ATTACHMENTS>;
	using TRenderTargetFlags = TStaticVector<ETextureCreateFlag, RHI::MAX_RT_ATTACHMENTS>;

	FGraphicsPSOInitializer()
		: BlendState(nullptr)
		, RasterizerState(nullptr)
		, DepthStencilState(nullptr)
		, RenderTargetsEnabled(0)
		, RenderTargetFormats({ RHI::MAX_RT_ATTACHMENTS, EPixelFormat::None })
		, RenderTargetFlags({ RHI::MAX_RT_ATTACHMENTS, ETextureCreateFlag::None })
		, DepthStencilTargetFormat(EPixelFormat::None)
		, DepthStencilTargetFlag(ETextureCreateFlag::None)
		, DepthTargetLoadAction(ERenderTargetLoadAction::DontCare)
		, DepthTargetStoreAction(ERenderTargetStoreAction::DontCare)
		, StencilTargetLoadAction(ERenderTargetLoadAction::DontCare)
		, StencilTargetStoreAction(ERenderTargetStoreAction::DontCare)
		, NumSamples(0)
		, SubpassHint(ESubpassHint::Default)
		, SubpassIndex(0)
		, bDepthBounds(false)
	{
	}

	FGraphicsPSOInitializer(
		FBoundShaderStateInput		InBoundShaderState,
		FRHIBlendStateRef			InBlendState,
		FRHIRasterizerStateRef		InRasterizerState,
		FRHIDepthStencilStateRef	InDepthStencilState,
		EPrimitiveType				InPrimitiveType,
		TUINT32						InRenderTargetsEnabled,
		const TRenderTargetFormats& InRenderTargetFormats,
		const TRenderTargetFlags&	InRenderTargetFlags,
		EPixelFormat				InDepthStencilTargetFormat,
		ETextureCreateFlags			InDepthStencilTargetFlag,
		ERenderTargetLoadAction		InDepthTargetLoadAction,
		ERenderTargetStoreAction	InDepthTargetStoreAction,
		ERenderTargetLoadAction		InStencilTargetLoadAction,
		ERenderTargetStoreAction	InStencilTargetStoreAction,
		FExclusiveDepthStencil		InDepthStencilAccess,
		TUINT8						InNumSamples,
		ESubpassHint				InSubpassHint,
		TUINT8						InSubpassIndex,
		bool						bInDepthBounds)
		: BoundShaderState(InBoundShaderState)
		, BlendState(InBlendState)
		, RasterizerState(InRasterizerState)
		, DepthStencilState(InDepthStencilState)
		, PrimitiveType(InPrimitiveType)
		, RenderTargetsEnabled(InRenderTargetsEnabled)
		, RenderTargetFormats(InRenderTargetFormats)
		, RenderTargetFlags(InRenderTargetFlags)
		, DepthStencilTargetFormat(InDepthStencilTargetFormat)
		, DepthStencilTargetFlag(InDepthStencilTargetFlag)
		, DepthTargetLoadAction(InDepthTargetLoadAction)
		, DepthTargetStoreAction(InDepthTargetStoreAction)
		, StencilTargetLoadAction(InStencilTargetLoadAction)
		, StencilTargetStoreAction(InStencilTargetStoreAction)
		, DepthStencilAccess(InDepthStencilAccess)
		, NumSamples(InNumSamples)
		, SubpassHint(InSubpassHint)
		, SubpassIndex(InSubpassIndex)
		, bDepthBounds(bInDepthBounds)
	{
	}

	bool operator==(const FGraphicsPSOInitializer& rhs) const
	{
		if (BoundShaderState.VertexDeclarationRHI != rhs.BoundShaderState.VertexDeclarationRHI
			|| BoundShaderState.VertexShaderRHI != rhs.BoundShaderState.VertexShaderRHI
			|| BoundShaderState.PixelShaderRHI != rhs.BoundShaderState.PixelShaderRHI
			|| BoundShaderState.GetMeshShader() != rhs.BoundShaderState.GetMeshShader()
			|| BoundShaderState.GetTaskShader() != rhs.BoundShaderState.GetTaskShader()
			|| BoundShaderState.GetGeometryShader() != rhs.BoundShaderState.GetGeometryShader()
			|| BlendState != rhs.BlendState
			|| RasterizerState != rhs.RasterizerState
			|| DepthStencilState != rhs.DepthStencilState
			|| PrimitiveType != rhs.PrimitiveType
			|| bDepthBounds != rhs.bDepthBounds
			|| RenderTargetsEnabled != rhs.RenderTargetsEnabled
			|| RenderTargetFormats != rhs.RenderTargetFormats
			|| !RelevantRenderTargetFlagsEqual(RenderTargetFlags, rhs.RenderTargetFlags)
			|| DepthStencilTargetFormat != rhs.DepthStencilTargetFormat
			|| !RelevantDepthStencilFlagsEqual(DepthStencilTargetFlag, rhs.DepthStencilTargetFlag)
			|| DepthTargetLoadAction != rhs.DepthTargetLoadAction
			|| DepthTargetStoreAction != rhs.DepthTargetStoreAction
			|| StencilTargetLoadAction != rhs.StencilTargetLoadAction
			|| StencilTargetStoreAction != rhs.StencilTargetStoreAction
			|| DepthStencilAccess != rhs.DepthStencilAccess
			|| NumSamples != rhs.NumSamples
			|| SubpassHint != rhs.SubpassHint
			|| SubpassIndex != rhs.SubpassIndex)
		{
			return false;
		}

		return true;
	}

	// We care about flags that influence RT formats (which is the only thing the underlying API cares about).
	// In most RHIs, the format is only influenced by TexCreate_SRGB. D3D12 additionally uses TexCreate_Shared in its format selection logic.
	static constexpr ETextureCreateFlags RelevantRenderTargetFlagMask = ETextureCreateFlag::RenderTarget;

	// We care about flags that influence DS formats (which is the only thing the underlying API cares about).
	// D3D12 shares the format choice function with the RT, so preserving all the flags used there out of abundance of caution.
	static constexpr ETextureCreateFlags RelevantDepthStencilFlagMask = ETextureCreateFlag::DepthStencil;

	static bool RelevantRenderTargetFlagsEqual(const TRenderTargetFlags& A, const TRenderTargetFlags& B)
	{
		for (TUINT32 Index = 0; Index < A.Num(); ++Index)
		{
			ETextureCreateFlags FlagsA = A[Index] & RelevantRenderTargetFlagMask;
			ETextureCreateFlags FlagsB = B[Index] & RelevantRenderTargetFlagMask;
			if (FlagsA != FlagsB)
			{
				return false;
			}
		}
		return true;
	}

	static bool RelevantDepthStencilFlagsEqual(const ETextureCreateFlags A, const ETextureCreateFlags B)
	{
		ETextureCreateFlags FlagsA = (A & RelevantDepthStencilFlagMask);
		ETextureCreateFlags FlagsB = (B & RelevantDepthStencilFlagMask);
		return (FlagsA == FlagsB);
	}

	TUINT32 ComputeNumValidRenderTargets() const
	{
		// Get the count of valid render targets (ignore those at the end of the array with PF_Unknown)
		if (RenderTargetsEnabled > 0)
		{
			TUINT32 LastValidTarget = 0;
			for (TUINT32 i = RenderTargetsEnabled; i > 0; i--)
			{
				if (RenderTargetFormats[i - 1] != EPixelFormat::None)
				{
					LastValidTarget = i - 1;
					break;
				}
			}
			return LastValidTarget;
		}
		return RenderTargetsEnabled;
	}

	FBoundShaderStateInput	 BoundShaderState;
	FRHIBlendStateRef		 BlendState;
	FRHIRasterizerStateRef	 RasterizerState;
	FRHIDepthStencilStateRef DepthStencilState;

	EPrimitiveType			 PrimitiveType;
	TUINT32					 RenderTargetsEnabled;
	TRenderTargetFormats	 RenderTargetFormats;
	TRenderTargetFlags		 RenderTargetFlags;
	EPixelFormat			 DepthStencilTargetFormat;
	ETextureCreateFlags		 DepthStencilTargetFlag;
	ERenderTargetLoadAction	 DepthTargetLoadAction;
	ERenderTargetStoreAction DepthTargetStoreAction;
	ERenderTargetLoadAction	 StencilTargetLoadAction;
	ERenderTargetStoreAction StencilTargetStoreAction;
	FExclusiveDepthStencil	 DepthStencilAccess;
	TUINT8					 NumSamples;
	ESubpassHint			 SubpassHint;
	TUINT8					 SubpassIndex;
	bool					 bDepthBounds;
};

// Structure for describing compute pipeline state initialization parameters
struct FComputePSOInitializer
{
	FRHIShader* ComputeShader; // Compute shader

	// Constructor for easy initialization
	FComputePSOInitializer(FRHIShader* InComputeShader = nullptr)
		: ComputeShader(InComputeShader)
	{
	}
};
