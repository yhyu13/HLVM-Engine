// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RHIDefinition.h"
#include "RHIMisc.h"

struct IRHICreateInfo
{
	FString				DebugName;	// Debug name for the resource

	IRHICreateInfo() = default;
	IRHICreateInfo(const FString& InDebugName)
	: DebugName(InDebugName)
	{
	}
	virtual ~IRHICreateInfo() = default;

	IRHICreateInfo(const IRHICreateInfo& InCreateInfo) = default;
	IRHICreateInfo& operator=(const IRHICreateInfo& InCreateInfo) = default;
};

// Structure for describing texture creation parameters
struct FRHITextureCreateInfo : public IRHICreateInfo
{
	FUIntVec3			Dimensions; // Width, Height, Depth (or array size)
	EPixelFormat		Format;		// Pixel format of the texture
	TUINT8				NumMips;	// Number of mip levels
	TUINT8				NumSamples; // Number of samples (for MSAA)
	ETextureCreateFlags Flags;		// Texture creation flags
	FClearValueBinding	ClearValue; // Clear value for the texture

	FRHITextureCreateInfo() = default;
	// Constructor for easy initialization
	FRHITextureCreateInfo(const FString& InDebugName, const FIntVec3& InDimensions, EPixelFormat InFormat, TUINT8 InNumMips = 1, TUINT8 InNumSamples = 1, ETextureCreateFlags InFlags = ETextureCreateFlag::None, const FClearValueBinding& InClearValue = FClearValueBinding::None())
		: IRHICreateInfo(InDebugName)
		, Dimensions(InDimensions)
		, Format(InFormat)
		, NumMips(InNumMips)
		, NumSamples(InNumSamples)
		, Flags(InFlags)
		, ClearValue(InClearValue)
	{
	}
};

// Structure for describing buffer creation parameters
struct FRHIBufferCreateInfo : public IRHICreateInfo
{
	TSIZE				 Size;				  // Size of the buffer in bytes
	EBufferUsageFlags	 UsageFlags;		  // Buffer usage flags
	EMemoryPropertyFlags MemoryPropertyFlags; // Memory property flags

	FRHIBufferCreateInfo() = default;
	// Constructor for easy initialization
	FRHIBufferCreateInfo(const FString& InDebugName, TSIZE InSize, EBufferUsageFlags InUsageFlags,
		EMemoryPropertyFlags InMemoryPropertyFlags)
		: IRHICreateInfo(InDebugName)
		, Size(InSize)
		, UsageFlags(InUsageFlags)
		, MemoryPropertyFlags(InMemoryPropertyFlags)
	{
	}
};

// Structure for describing shader resource view creation parameters
struct FRHIShaderResourceViewCreateInfo : public IRHICreateInfo
{
	FRHITexture* Texture;		  // Texture to create the SRV for
	EPixelFormat Format;		  // Format of the SRV
	TUINT32		 MipLevel;		  // Mip level to use
	TUINT32		 NumMipLevels;	  // Number of mip levels
	TUINT32		 FirstArraySlice; // First array slice (for texture arrays)
	TUINT32		 NumArraySlices;  // Number of array slices (for texture arrays)

	FRHIShaderResourceViewCreateInfo() = default;
	// Constructor for easy initialization
	FRHIShaderResourceViewCreateInfo(FRHITexture* InTexture, EPixelFormat InFormat, TUINT32 InMipLevel = 0, TUINT32 InNumMipLevels = 1, TUINT32 InFirstArraySlice = 0, TUINT32 InNumArraySlices = 1)
		: Texture(InTexture)
		, Format(InFormat)
		, MipLevel(InMipLevel)
		, NumMipLevels(InNumMipLevels)
		, FirstArraySlice(InFirstArraySlice)
		, NumArraySlices(InNumArraySlices)
	{
	}
};

// Structure for describing unordered access view creation parameters
struct FRHIUnorderedAccessViewCreateInfo : public IRHICreateInfo
{
	FRHITexture* Texture;		  // Texture to create the UAV for
	EPixelFormat Format;		  // Format of the UAV
	TUINT32		 MipLevel;		  // Mip level to use
	TUINT32		 FirstArraySlice; // First array slice (for texture arrays)
	TUINT32		 NumArraySlices;  // Number of array slices (for texture arrays)
	TUINT64		 Offset;
	TUINT32		 Size;

	FRHIUnorderedAccessViewCreateInfo() = default;
	// Constructor for easy initialization
	FRHIUnorderedAccessViewCreateInfo(FRHITexture* InTexture, EPixelFormat InFormat, TUINT32 InMipLevel = 0, TUINT32 InFirstArraySlice = 0, TUINT32 InNumArraySlices = 1)
		: Texture(InTexture)
		, Format(InFormat)
		, MipLevel(InMipLevel)
		, FirstArraySlice(InFirstArraySlice)
		, NumArraySlices(InNumArraySlices)
	{
	}
};

// Structure for describing shader creation parameters
struct FShaderCreateInfo : public IRHICreateInfo
{
	EShaderStage	 Stage;		  // Shader stage (e.g., vertex, pixel, compute)
	TVector<TUINT8>	 Code;		  // Shader bytecode
	TVector<FString> EntryPoints; // Entry points for the shader

	FShaderCreateInfo() = default;
	// Constructor for easy initialization
	FShaderCreateInfo(const FString& InDebugName, EShaderStage InStage, const TVector<TUINT8>& InCode, const TVector<FString>& InEntryPoints = { "Main" })
		: IRHICreateInfo(InDebugName)
		, Stage(InStage)
		, Code(InCode)
		, EntryPoints(InEntryPoints)
	{
	}
};

// Structure for describing sampler state creation parameters
struct FRHISamplerStateCreateInfo : public IRHICreateInfo
{
	ETextureFilter		Filter;					 // Filter mode
	ETextureAddressMode AddressModeU;			 // Address mode for U coordinate
	ETextureAddressMode AddressModeV;			 // Address mode for V coordinate
	ETextureAddressMode AddressModeW;			 // Address mode for W coordinate
	TUINT32				MipMapLevelOfDetailBias; // Mip map level of detail bias
	TUINT32				MaxAnisotropy;			 // Maximum anisotropy
	ERHICompare			ComparisonFunction;		 // Comparison function
	FVec4				BorderColor;			 // Border color

	// Constructor for easy initialization
	FRHISamplerStateCreateInfo(const FString& InDebugName, ETextureFilter InFilter, ETextureAddressMode InAddressModeU, ETextureAddressMode InAddressModeV, ETextureAddressMode InAddressModeW, TUINT32 InMipMapLevelOfDetailBias = 0, TUINT32 InMaxAnisotropy = 1, ERHICompare InComparisonFunction = ERHICompare::Never, const FVec4& InBorderColor = FVec4(0.0f, 0.0f, 0.0f, 0.0f))
		: IRHICreateInfo(InDebugName)
		, Filter(InFilter)
		, AddressModeU(InAddressModeU)
		, AddressModeV(InAddressModeV)
		, AddressModeW(InAddressModeW)
		, MipMapLevelOfDetailBias(InMipMapLevelOfDetailBias)
		, MaxAnisotropy(InMaxAnisotropy)
		, ComparisonFunction(InComparisonFunction)
		, BorderColor(InBorderColor)
	{
	}
};

// Structure for describing graphics pipeline layout creation parameters
struct FRHIGraphicsPipelineLayoutCreateInfo : public IRHICreateInfo
{
	TVector<FRHIShaderResourceViewCreateInfo>  ShaderResourceViews;	 // Shader resource views
	TVector<FRHIUnorderedAccessViewCreateInfo> UnorderedAccessViews; // Unordered access views
	TVector<FRHISamplerStateCreateInfo>		   SamplerStates;		 // Sampler states

	// Constructor for easy initialization
	FRHIGraphicsPipelineLayoutCreateInfo(const FString& InDebugName, const TVector<FRHIShaderResourceViewCreateInfo>& InShaderResourceViews, const TVector<FRHIUnorderedAccessViewCreateInfo>& InUnorderedAccessViews, const TVector<FRHISamplerStateCreateInfo>& InSamplerStates)
		: IRHICreateInfo(InDebugName)
		, ShaderResourceViews(InShaderResourceViews)
		, UnorderedAccessViews(InUnorderedAccessViews)
		, SamplerStates(InSamplerStates)
	{
	}
};

// Structure for describing graphics pipeline state creation parameters
struct FRHIGraphicsPipelineStateCreateInfo : public IRHICreateInfo
{
	EPrimitiveTopology		   PrimitiveTopology; // Primitive topology
	EPolygonMode			   PolygonMode;		  // Polygon mode
	EFrontFace				   FrontFace;		  // Front face
	ECullMode				   CullMode;		  // Cull mode
	EDepthTest				   DepthTest;		  // Depth test
	EStencilTest			   StencilTest;		  // Stencil test
	EBlendMode				   BlendMode;		  // Blend mode
	TVector<FShaderCreateInfo> Shaders;			  // Shaders

	// Constructor for easy initialization
	FRHIGraphicsPipelineStateCreateInfo(const FString& InDebugName, EPrimitiveTopology InPrimitiveTopology, EPolygonMode InPolygonMode, EFrontFace InFrontFace, ECullMode InCullMode, EDepthTest InDepthTest, EStencilTest InStencilTest, EBlendMode InBlendMode, const TVector<FShaderCreateInfo>& InShaders)
		: IRHICreateInfo(InDebugName)
		, PrimitiveTopology(InPrimitiveTopology)
		, PolygonMode(InPolygonMode)
		, FrontFace(InFrontFace)
		, CullMode(InCullMode)
		, DepthTest(InDepthTest)
		, StencilTest(InStencilTest)
		, BlendMode(InBlendMode)
		, Shaders(InShaders)
	{
	}
};

// Structure for describing compute pipeline state creation parameters
struct FRHIComputePipelineStateCreateInfo : public IRHICreateInfo
{
	FShaderCreateInfo Shader;	 // Compute shader

	// Constructor for easy initialization
	FRHIComputePipelineStateCreateInfo(const FString& InDebugName, const FShaderCreateInfo& InShader)
		: IRHICreateInfo(InDebugName)
		, Shader(InShader)
	{
	}
};

// Structure for describing query creation parameters
struct FRHIQueryCreateInfo : public IRHICreateInfo
{
	ERHIQueryType QueryType;  // Type of the query (e.g., occlusion, timestamp)
	TUINT32		  NumQueries; // Number of queries

	// Constructor for easy initialization
	FRHIQueryCreateInfo(const FString& InDebugName, ERHIQueryType InQueryType, TUINT32 InNumQueries = 1)
		: IRHICreateInfo(InDebugName)
		, QueryType(InQueryType)
		, NumQueries(InNumQueries)
	{
	}
};

// Structure for describing swap chain creation parameters
struct FRHISwapChainCreateInfo : public IRHICreateInfo
{
	FIntVec2		Dimensions;	   // Width and height of the swap chain buffers
	EPixelFormat	Format;		   // Pixel format of the swap chain buffers
	TUINT32			NumBuffers;	   // Number of buffers in the swap chain (e.g., double or triple buffering)
	TUINT32			NumSamples;	   // Number of samples (for MSAA)
	ESwapChainFlags Flags;		   // Swap chain creation flags
	FRHIViewport*	OwnerViewport; // Associated viewport

	// Default constructor
	FRHISwapChainCreateInfo() = default;

	// Constructor for easy initialization
	FRHISwapChainCreateInfo(
		const FString&	InDebugName,
		const FIntVec2& InDimensions,
		EPixelFormat	InFormat = EPixelFormat::R8G8B8A8_UNorm,
		TUINT32			InNumBuffers = 2, // Default to double buffering
		TUINT32			InNumSamples = 1, // Default to no MSAA
		ESwapChainFlags InFlags = ESwapChainFlags::None,
		FRHIViewport*	InViewport = nullptr)
		: IRHICreateInfo(InDebugName)
		, Dimensions(InDimensions)
		, Format(InFormat)
		, NumBuffers(InNumBuffers)
		, NumSamples(InNumSamples)
		, Flags(InFlags)
		, OwnerViewport(InViewport)
	{
	}
};

class IWindow;
// Structure for describing viewport creation parameters
struct FRHIViewportCreateInfo : public IRHICreateInfo
{
	FUIntVec2				Dimensions;	  // Width and height of the viewport
	ERHIViewportType		ViewportType; // Type of the viewport (e.g., windowed, fullscreen)
	EPixelFormat			Format;		  // Pixel format of the viewport's back buffer
	TNoNullablePtr<IWindow> NativeWindowHandle;
	bool					bHeadlessRendering;

	// Default constructor
	FRHIViewportCreateInfo() = default;

	// Constructor for easy initialization
	FRHIViewportCreateInfo(
		const FString&	 InDebugName,
		const FUIntVec2& InDimensions,
		IWindow*		 InWindowHandle,
		ERHIViewportType InViewportType = ERHIViewportType::Fullscreen,
		EPixelFormat	 InFormat = EPixelFormat::R8G8B8A8_UNorm,
		bool			 InHeadlessRendering = false)
		: IRHICreateInfo(InDebugName)
		, Dimensions(InDimensions)
		, ViewportType(InViewportType)
		, Format(InFormat)
		, NativeWindowHandle(InWindowHandle)
		, bHeadlessRendering(InHeadlessRendering)
	{
	}
};

class FExclusiveDepthStencil
{
public:
	enum Type
	{
		// don't use those directly, use the combined versions below
		// 4 bits are used for depth and 4 for stencil to make the hex value readable and non overlapping
		DepthNop = 0x00,
		DepthRead = 0x01,
		DepthWrite = 0x02,
		DepthMask = 0x0f,
		StencilNop = 0x00,
		StencilRead = 0x10,
		StencilWrite = 0x20,
		StencilMask = 0xf0,

		// use those:
		DepthNop_StencilNop = DepthNop + StencilNop,
		DepthRead_StencilNop = DepthRead + StencilNop,
		DepthWrite_StencilNop = DepthWrite + StencilNop,
		DepthNop_StencilRead = DepthNop + StencilRead,
		DepthRead_StencilRead = DepthRead + StencilRead,
		DepthWrite_StencilRead = DepthWrite + StencilRead,
		DepthNop_StencilWrite = DepthNop + StencilWrite,
		DepthRead_StencilWrite = DepthRead + StencilWrite,
		DepthWrite_StencilWrite = DepthWrite + StencilWrite,
	};

private:
	Type Value;

public:
	// constructor
	FExclusiveDepthStencil(Type InValue = DepthNop_StencilNop)
		: Value(InValue)
	{
	}

	inline bool IsUsingDepthStencil() const
	{
		return Value != DepthNop_StencilNop;
	}
	inline bool IsUsingDepth() const
	{
		return (ExtractDepth() != DepthNop);
	}
	inline bool IsUsingStencil() const
	{
		return (ExtractStencil() != StencilNop);
	}
	inline bool IsDepthWrite() const
	{
		return ExtractDepth() == DepthWrite;
	}
	inline bool IsDepthRead() const
	{
		return ExtractDepth() == DepthRead;
	}
	inline bool IsStencilWrite() const
	{
		return ExtractStencil() == StencilWrite;
	}
	inline bool IsStencilRead() const
	{
		return ExtractStencil() == StencilRead;
	}

	inline bool IsAnyWrite() const
	{
		return IsDepthWrite() || IsStencilWrite();
	}

	inline void SetDepthWrite()
	{
		Value = S_C(Type, ExtractStencil() | DepthWrite);
	}
	inline void SetStencilWrite()
	{
		Value = S_C(Type, ExtractDepth() | StencilWrite);
	}
	inline void SetDepthStencilWrite(bool bDepth, bool bStencil)
	{
		Value = DepthNop_StencilNop;

		if (bDepth)
		{
			SetDepthWrite();
		}
		if (bStencil)
		{
			SetStencilWrite();
		}
	}
	bool operator==(const FExclusiveDepthStencil& rhs) const
	{
		return Value == rhs.Value;
	}

	bool operator!=(const FExclusiveDepthStencil& RHS) const
	{
		return Value != RHS.Value;
	}

	inline bool IsValid(FExclusiveDepthStencil& Current) const
	{
		Type Depth = ExtractDepth();

		if (Depth != DepthNop && Depth != Current.ExtractDepth())
		{
			return false;
		}

		Type Stencil = ExtractStencil();

		if (Stencil != StencilNop && Stencil != Current.ExtractStencil())
		{
			return false;
		}

		return true;
	}

	inline void GetAccess(ERHIAccessFlags& DepthAccess, ERHIAccessFlags& StencilAccess) const
	{
		DepthAccess = ERHIAccessFlag::None;

		// SRV access is allowed whilst a depth stencil target is "readable".
		constexpr ERHIAccessFlags DSVReadOnlyMask =
			ERHIAccessFlag::DSVRead;

		// If write access is required, only the depth block can access the resource.
		constexpr ERHIAccessFlags DSVReadWriteMask =
			ERHIAccessFlag::ReadWrite;

		if (IsUsingDepth())
		{
			DepthAccess = IsDepthWrite() ? DSVReadWriteMask : DSVReadOnlyMask;
		}

		StencilAccess = ERHIAccessFlag::None;

		if (IsUsingStencil())
		{
			StencilAccess = IsStencilWrite() ? DSVReadWriteMask : DSVReadOnlyMask;
		}
	}

	/**
	 * Returns a new FExclusiveDepthStencil to be used to transition a depth stencil resource to readable.
	 * If the depth or stencil is already in a readable state, that particular component is returned as Nop,
	 * to avoid unnecessary subresource transitions.
	 */
	inline FExclusiveDepthStencil GetReadableTransition() const
	{
		FExclusiveDepthStencil::Type NewDepthState = IsDepthWrite()
			? FExclusiveDepthStencil::DepthRead
			: FExclusiveDepthStencil::DepthNop;

		FExclusiveDepthStencil::Type NewStencilState = IsStencilWrite()
			? FExclusiveDepthStencil::StencilRead
			: FExclusiveDepthStencil::StencilNop;

		return S_C(FExclusiveDepthStencil::Type, NewDepthState | NewStencilState);
	}

	/**
	 * Returns a new FExclusiveDepthStencil to be used to transition a depth stencil resource to readable.
	 * If the depth or stencil is already in a readable state, that particular component is returned as Nop,
	 * to avoid unnecessary subresource transitions.
	 */
	inline FExclusiveDepthStencil GetWritableTransition() const
	{
		FExclusiveDepthStencil::Type NewDepthState = IsDepthRead()
			? FExclusiveDepthStencil::DepthWrite
			: FExclusiveDepthStencil::DepthNop;

		FExclusiveDepthStencil::Type NewStencilState = IsStencilRead()
			? FExclusiveDepthStencil::StencilWrite
			: FExclusiveDepthStencil::StencilNop;

		return S_C(FExclusiveDepthStencil::Type, NewDepthState | NewStencilState);
	}

	TUINT32 GetIndex() const
	{
		// Note: The array to index has views created in that specific order.

		// we don't care about the Nop versions so less views are needed
		// we combine Nop and Write
		switch (Value)
		{
			case DepthWrite_StencilNop:
			case DepthNop_StencilWrite:
			case DepthWrite_StencilWrite:
			case DepthNop_StencilNop:
				return 0; // old DSAT_Writable

			case DepthRead_StencilNop:
			case DepthRead_StencilWrite:
				return 1; // old DSAT_ReadOnlyDepth

			case DepthNop_StencilRead:
			case DepthWrite_StencilRead:
				return 2; // old DSAT_ReadOnlyStencil

			case DepthRead_StencilRead:
				return 3; // old DSAT_ReadOnlyDepthAndStencil

			case DepthMask:
			case StencilMask:
			default:
				HLVM_ASSERT(0);
				return TUINT32_MAX;
		}
	}
	static const TUINT32 MaxIndex = 4;

private:
	inline Type ExtractDepth() const
	{
		return S_C(Type, Value & DepthMask);
	}
	inline Type ExtractStencil() const
	{
		return S_C(Type, Value & StencilMask);
	}
};
