/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "Core/Object/RefCountPtr.h"
#include "RHIDefinition.h"
#include "RHIResourceDeclaration.h"

// Enumeration of RHI resource types
enum class ERHIResourceType : TUINT8
{
	Texture,
	Buffer,
	Shader,
	ShaderResourceView,
	UnorderedAccessView,
	SamplerState,
	PipelineState,
	Query,
	VertexDeclaration,
	Viewport,
	// Add other resource types as needed
};

// Base class for all RHI resources
class FRHIResource : public FRefCountable
{
public:
	FRHIResource() = default;
	virtual ~FRHIResource() = default;

	// Returns the type of the RHI resource
	virtual ERHIResourceType GetType() const = 0;

	// Returns the RHI interface type (e.g., Vulkan, DirectX)
	virtual ERHIInterfaceType GetInterfaceType() const = 0;

	// Returns the name of the resource for debugging
	virtual FString GetName() const { return TXT("Unnamed RHI Resource"); }
};

// Base class for RHI textures
class FRHITexture : virtual public FRHIResource
{
public:
	FRHITextureCreateDesc CreateDesc; // Declaration struct as a member

	// Returns the dimensions of the texture
	virtual FIntVec3 GetSize() const { return CreateDesc.Dimensions; }

	// Returns the pixel format of the texture
	virtual EPixelFormat GetFormat() const { return CreateDesc.Format; }

	// Returns the texture flags
	virtual ETextureCreateFlags GetFlags() const { return CreateDesc.Flags; }

	// Returns the type of the RHI resource
	virtual ERHIResourceType GetType() const override { return ERHIResourceType::Texture; }
};

// Base class for RHI buffers
class FRHIBuffer : virtual public FRHIResource
{
public:
	FRHIBufferCreateDesc CreateDesc; // Declaration struct as a member

	// Returns the size of the buffer in bytes
	virtual TSIZE GetSize() const { return CreateDesc.Size; }

	// Returns the usage flags of the buffer
	virtual EBufferUsageFlags GetUsageFlags() const { return CreateDesc.UsageFlags; }

	// Returns the memory flags of the buffer
	virtual EMemoryPropertyFlags GetMemoryFlags() const { return CreateDesc.MemoryPropertyFlags; }

	// Returns the type of the RHI resource
	virtual ERHIResourceType GetType() const override { return ERHIResourceType::Buffer; }
};

// Base class for RHI shaders
class FRHIShader : virtual public FRHIResource
{
public:
	FShaderCreateInfo CreateDesc; // Declaration struct as a member

	// Returns the shader stage (e.g., vertex, pixel, compute)
	virtual EShaderStage GetStage() const { return CreateDesc.Stage; }

	// Returns the type of the RHI resource
	virtual ERHIResourceType GetType() const override { return ERHIResourceType::Shader; }
};

// Base class for RHI shader resource views
class FRHIShaderResourceView : virtual public FRHIResource
{
public:
	FRHIShaderResourceViewCreateInfo CreateDesc; // Declaration struct as a member

	// Returns the type of the RHI resource
	virtual ERHIResourceType GetType() const override { return ERHIResourceType::ShaderResourceView; }
};

// Base class for RHI unordered access views
class FRHIUnorderedAccessView : virtual public FRHIResource
{
public:
	FRHIUnorderedAccessViewCreateInfo CreateDesc; // Declaration struct as a member

	// Returns the type of the RHI resource
	virtual ERHIResourceType GetType() const override { return ERHIResourceType::UnorderedAccessView; }
};

// Base class for RHI sampler states
class FRHISamplerState : virtual public FRHIResource
{
public:
	// Returns the type of the RHI resource
	virtual ERHIResourceType GetType() const override { return ERHIResourceType::SamplerState; }
};

// Base class for RHI pipeline states
class FRHIGraphicsPipelineState : virtual public FRHIResource
{
public:
	// Returns the type of the RHI resource
	virtual ERHIResourceType GetType() const override { return ERHIResourceType::PipelineState; }
};

class FRHIComputePipelineState : virtual public FRHIResource
{
public:
	// Returns the type of the RHI resource
	virtual ERHIResourceType GetType() const override { return ERHIResourceType::PipelineState; }
};

// Base class for RHI queries
class FRHIQuery : virtual public FRHIResource
{
public:
	// Returns the type of the query (e.g., occlusion, timestamp)
	virtual ERHIQueryType GetQueryType() const = 0;

	// Returns the type of the RHI resource
	virtual ERHIResourceType GetType() const override { return ERHIResourceType::Query; }
};

// Base class for RHI viewports
class FRHIViewport : virtual public FRHIResource
{
public:
	// Constructor
	FRHIViewport(const FRHIViewportCreateDesc& InCreateDesc)
		: CreateDesc(InCreateDesc)
	{
	}

	// Returns the type of the RHI resource
	virtual ERHIResourceType GetType() const override { return ERHIResourceType::Viewport; }

	// Returns the dimensions of the viewport
	virtual FUIntVec2 GetSize() const { return CreateDesc.Dimensions; }

	// Returns the viewport type (e.g., windowed, fullscreen)
	virtual ERHIViewportType GetViewportType() const { return CreateDesc.ViewportType; }

	// Returns the associated swap chain (if any)
	virtual void* GetSwapChain() const = 0;

	// Resizes the viewport and swap chain
	virtual void Resize(const FUIntVec2& NewDimensions) = 0;

	//	// Presents the viewport (swaps the back buffer)
	//	virtual void Present() = 0;

protected:
	FRHIViewportCreateDesc CreateDesc; // Viewport creation description
};

// Smart pointer types for RHI resources
using FTextureRHIRef = TRefCountPtr<FRHITexture>;
using FBufferRHIRef = TRefCountPtr<FRHIBuffer>;
using FShaderRHIRef = TRefCountPtr<FRHIShader>;
using FShaderResourceViewRHIRef = TRefCountPtr<FRHIShaderResourceView>;
using FUnorderedAccessViewRHIRef = TRefCountPtr<FRHIUnorderedAccessView>;
using FSamplerStateRHIRef = TRefCountPtr<FRHISamplerState>;
using FGraphicsPipelineStateRHIRef = TRefCountPtr<FRHIGraphicsPipelineState>;
using FComputePipelineStateRHIRef = TRefCountPtr<FRHIComputePipelineState>;
using FQueryRHIRef = TRefCountPtr<FRHIQuery>;
using FViewportRHIRef = TRefCountPtr<FRHIViewport>;

class FRHIRenderTargetView
{
public:
	FRHIRenderTargetView() = default;
	FRHIRenderTargetView(FRHIRenderTargetView&&) = default;
	FRHIRenderTargetView(const FRHIRenderTargetView&) = default;
	FRHIRenderTargetView& operator=(FRHIRenderTargetView&&) = default;
	FRHIRenderTargetView& operator=(const FRHIRenderTargetView&) = default;

	// common case
	explicit FRHIRenderTargetView(FTextureRHIRef InTexture, ERenderTargetLoadAction InLoadAction)
		: Texture(InTexture), LoadAction(InLoadAction), StoreAction(ERenderTargetStoreAction::Store)
	{
	}

	// common case
	explicit FRHIRenderTargetView(FTextureRHIRef InTexture, ERenderTargetLoadAction InLoadAction, TUINT32 InMipIndex, TUINT32 InArraySliceIndex)
		: Texture(InTexture), MipIndex(InMipIndex), ArraySliceIndex(InArraySliceIndex), LoadAction(InLoadAction), StoreAction(ERenderTargetStoreAction::Store)
	{
	}

	explicit FRHIRenderTargetView(FTextureRHIRef InTexture, TUINT32 InMipIndex, TUINT32 InArraySliceIndex, ERenderTargetLoadAction InLoadAction, ERenderTargetStoreAction InStoreAction)
		: Texture(InTexture), MipIndex(InMipIndex), ArraySliceIndex(InArraySliceIndex), LoadAction(InLoadAction), StoreAction(InStoreAction)
	{
	}

	bool operator==(const FRHIRenderTargetView& Other) const
	{
		return Texture == Other.Texture && MipIndex == Other.MipIndex && ArraySliceIndex == Other.ArraySliceIndex && LoadAction == Other.LoadAction && StoreAction == Other.StoreAction;
	}

public:
	FTextureRHIRef Texture = nullptr;
	TUINT32		   MipIndex = 0;

	/** Array slice or texture cube face.  Only valid if texture resource was created with TexCreate_TargetArraySlicesIndependently! */
	TUINT32 ArraySliceIndex = TUINT32_MAX;

	ERenderTargetLoadAction	 LoadAction = ERenderTargetLoadAction::DontCare;
	ERenderTargetStoreAction StoreAction = ERenderTargetStoreAction::DontCare;
};

class FRHIDepthRenderTargetView
{
public:
	// accessor to prevent write access to StencilStoreAction
	ERenderTargetStoreAction GetStencilStoreAction() const { return StencilStoreAction; }
	// accessor to prevent write access to DepthStencilAccess
	FExclusiveDepthStencil GetDepthStencilAccess() const { return DepthStencilAccess; }

	explicit FRHIDepthRenderTargetView()
		: Texture(nullptr), DepthLoadAction(ERenderTargetLoadAction::DontCare), DepthStoreAction(ERenderTargetStoreAction::DontCare), StencilLoadAction(ERenderTargetLoadAction::DontCare), StencilStoreAction(ERenderTargetStoreAction::DontCare), DepthStencilAccess(FExclusiveDepthStencil::DepthNop_StencilNop)
	{
		Validate();
	}

	// common case
	explicit FRHIDepthRenderTargetView(FTextureRHIRef InTexture, ERenderTargetLoadAction InLoadAction, ERenderTargetStoreAction InStoreAction)
		: Texture(InTexture), DepthLoadAction(InLoadAction), DepthStoreAction(InStoreAction), StencilLoadAction(InLoadAction), StencilStoreAction(InStoreAction), DepthStencilAccess(FExclusiveDepthStencil::DepthWrite_StencilWrite)
	{
		Validate();
	}

	explicit FRHIDepthRenderTargetView(FTextureRHIRef InTexture, ERenderTargetLoadAction InLoadAction, ERenderTargetStoreAction InStoreAction, FExclusiveDepthStencil InDepthStencilAccess)
		: Texture(InTexture), DepthLoadAction(InLoadAction), DepthStoreAction(InStoreAction), StencilLoadAction(InLoadAction), StencilStoreAction(InStoreAction), DepthStencilAccess(InDepthStencilAccess)
	{
		Validate();
	}

	explicit FRHIDepthRenderTargetView(FTextureRHIRef InTexture, ERenderTargetLoadAction InDepthLoadAction, ERenderTargetStoreAction InDepthStoreAction, ERenderTargetLoadAction InStencilLoadAction, ERenderTargetStoreAction InStencilStoreAction)
		: Texture(InTexture), DepthLoadAction(InDepthLoadAction), DepthStoreAction(InDepthStoreAction), StencilLoadAction(InStencilLoadAction), StencilStoreAction(InStencilStoreAction), DepthStencilAccess(FExclusiveDepthStencil::DepthWrite_StencilWrite)
	{
		Validate();
	}

	explicit FRHIDepthRenderTargetView(FTextureRHIRef InTexture, ERenderTargetLoadAction InDepthLoadAction, ERenderTargetStoreAction InDepthStoreAction, ERenderTargetLoadAction InStencilLoadAction, ERenderTargetStoreAction InStencilStoreAction, FExclusiveDepthStencil InDepthStencilAccess)
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

	bool operator==(const FRHIDepthRenderTargetView& Other) const
	{
		return Texture == Other.Texture && DepthLoadAction == Other.DepthLoadAction && DepthStoreAction == Other.DepthStoreAction && StencilLoadAction == Other.StencilLoadAction && StencilStoreAction == Other.StencilStoreAction && DepthStencilAccess == Other.DepthStencilAccess;
	}

public:
	FTextureRHIRef Texture;

	ERenderTargetLoadAction	 DepthLoadAction;
	ERenderTargetStoreAction DepthStoreAction;
	ERenderTargetLoadAction	 StencilLoadAction;

private:
	ERenderTargetStoreAction StencilStoreAction;
	FExclusiveDepthStencil	 DepthStencilAccess;
};

class FRHISetRenderTargetsInfo
{
public:
	// Color Render Targets Info
	FRHIRenderTargetView ColorRenderTarget[RHI::RT_ATTACHMENT_MAX];
	TUINT32				 NumColorRenderTargets;
	bool				 bClearColor;

	// Color Render Targets Info
	FRHIRenderTargetView ColorResolveRenderTarget[RHI::RT_ATTACHMENT_MAX];
	bool				 bHasResolveAttachments;

	// Depth/Stencil Render Target Info
	FRHIDepthRenderTargetView DepthStencilRenderTarget;
	// Used when depth resolve is enabled.
	FRHIDepthRenderTargetView DepthStencilResolveRenderTarget;
	bool					  bClearDepth;
	bool					  bClearStencil;

	FTextureRHIRef	 ShadingRateTexture;
	EVariableRateShadingCombiner ShadingRateTextureCombiner;

	TUINT8 MultiViewCount;

	FRHISetRenderTargetsInfo()
		: NumColorRenderTargets(0), bClearColor(false), bHasResolveAttachments(false), bClearDepth(false), ShadingRateTexture(nullptr), MultiViewCount(0)
	{
	}

	FRHISetRenderTargetsInfo(TUINT32 InNumColorRenderTargets, const FRHIRenderTargetView* InColorRenderTargets, const FRHIDepthRenderTargetView& InDepthStencilRenderTarget)
		: NumColorRenderTargets(InNumColorRenderTargets), bClearColor(InNumColorRenderTargets > 0 && InColorRenderTargets[0].LoadAction == ERenderTargetLoadAction::Clear), bHasResolveAttachments(false), DepthStencilRenderTarget(InDepthStencilRenderTarget), bClearDepth(InDepthStencilRenderTarget.Texture && InDepthStencilRenderTarget.DepthLoadAction == ERenderTargetLoadAction::Clear), ShadingRateTexture(nullptr), ShadingRateTextureCombiner(EVariableRateShadingCombiner::Passthrough)
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
