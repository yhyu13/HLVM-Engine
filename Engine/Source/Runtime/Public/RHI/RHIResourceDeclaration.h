// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RHIDefinition.h"
#include "RHIMisc.h"

// Structure for describing texture creation parameters
struct FRHITextureCreateDesc
{
	FString				DebugName;	// Debug name for the texture
	FUIntVec3			Dimensions; // Width, Height, Depth (or array size)
	EPixelFormat		Format;		// Pixel format of the texture
	TUINT32				NumMips;	// Number of mip levels
	TUINT32				NumSamples; // Number of samples (for MSAA)
	ETextureCreateFlags Flags;		// Texture creation flags
	FClearValueBinding	ClearValue; // Clear value for the texture

	FRHITextureCreateDesc() = default;
	// Constructor for easy initialization
	FRHITextureCreateDesc(const FString& InDebugName, const FIntVec3& InDimensions, EPixelFormat InFormat, TUINT32 InNumMips = 1, TUINT32 InNumSamples = 1, ETextureCreateFlags InFlags = ETextureCreateFlags::None, const FClearValueBinding& InClearValue = FClearValueBinding::None())
		: DebugName(InDebugName)
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
struct FRHIBufferCreateDesc
{
	FString			  DebugName;   // Debug name for the buffer
	TUINT32			  SizeInBytes; // Size of the buffer in bytes
	EBufferUsageFlags UsageFlags;  // Buffer usage flags
	TUINT32			  Stride;	   // Stride of the buffer (for structured buffers)

	FRHIBufferCreateDesc() = default;
	// Constructor for easy initialization
	FRHIBufferCreateDesc(const FString& InDebugName, TUINT32 InSizeInBytes, EBufferUsageFlags InUsageFlags, TUINT32 InStride = 0)
		: DebugName(InDebugName)
		, SizeInBytes(InSizeInBytes)
		, UsageFlags(InUsageFlags)
		, Stride(InStride)
	{
	}
};

// Structure for describing shader resource view creation parameters
struct FRHIShaderResourceViewCreateInfo
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
struct FRHIUnorderedAccessViewCreateInfo
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
struct FShaderCreateInfo
{
	FString			 DebugName;	  // Debug name for the shader
	EShaderStage	 Stage;		  // Shader stage (e.g., vertex, pixel, compute)
	TVector<TUINT8>	 Code;		  // Shader bytecode
	TVector<FString> EntryPoints; // Entry points for the shader

	FShaderCreateInfo() = default;
	// Constructor for easy initialization
	FShaderCreateInfo(const FString& InDebugName, EShaderStage InStage, const TVector<TUINT8>& InCode, const TVector<FString>& InEntryPoints = { "Main" })
		: DebugName(InDebugName)
		, Stage(InStage)
		, Code(InCode)
		, EntryPoints(InEntryPoints)
	{
	}
};

// Structure for describing sampler state creation parameters
struct FRHISamplerStateCreateInfo
{
	FString				DebugName;				 // Debug name for the sampler state
	ETextureFilter		Filter;					 // Filter mode
	ETextureAddressMode AddressModeU;			 // Address mode for U coordinate
	ETextureAddressMode AddressModeV;			 // Address mode for V coordinate
	ETextureAddressMode AddressModeW;			 // Address mode for W coordinate
	TUINT32				MipMapLevelOfDetailBias; // Mip map level of detail bias
	TUINT32				MaxAnisotropy;			 // Maximum anisotropy
	ECompareFunction	ComparisonFunction;		 // Comparison function
	FVec4				BorderColor;			 // Border color

	// Constructor for easy initialization
	FRHISamplerStateCreateInfo(const FString& InDebugName, ETextureFilter InFilter, ETextureAddressMode InAddressModeU, ETextureAddressMode InAddressModeV, ETextureAddressMode InAddressModeW, TUINT32 InMipMapLevelOfDetailBias = 0, TUINT32 InMaxAnisotropy = 1, ECompareFunction InComparisonFunction = ECompareFunction::Never, const FVec4& InBorderColor = FVec4(0.0f, 0.0f, 0.0f, 0.0f))
		: DebugName(InDebugName)
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
struct FRHIGraphicsPipelineLayoutCreateInfo
{
	FString									   DebugName;			 // Debug name for the pipeline layout
	TVector<FRHIShaderResourceViewCreateInfo>  ShaderResourceViews;	 // Shader resource views
	TVector<FRHIUnorderedAccessViewCreateInfo> UnorderedAccessViews; // Unordered access views
	TVector<FRHISamplerStateCreateInfo>		   SamplerStates;		 // Sampler states

	// Constructor for easy initialization
	FRHIGraphicsPipelineLayoutCreateInfo(const FString& InDebugName, const TVector<FRHIShaderResourceViewCreateInfo>& InShaderResourceViews, const TVector<FRHIUnorderedAccessViewCreateInfo>& InUnorderedAccessViews, const TVector<FRHISamplerStateCreateInfo>& InSamplerStates)
		: DebugName(InDebugName)
		, ShaderResourceViews(InShaderResourceViews)
		, UnorderedAccessViews(InUnorderedAccessViews)
		, SamplerStates(InSamplerStates)
	{
	}
};

// Structure for describing graphics pipeline state creation parameters
struct FRHIGraphicsPipelineStateCreateInfo
{
	FString					   DebugName;		  // Debug name for the pipeline state
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
		: DebugName(InDebugName)
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
struct FRHIComputePipelineStateCreateInfo
{
	FString			  DebugName; // Debug name for the pipeline state
	FShaderCreateInfo Shader;	 // Compute shader

	// Constructor for easy initialization
	FRHIComputePipelineStateCreateInfo(const FString& InDebugName, const FShaderCreateInfo& InShader)
		: DebugName(InDebugName)
		, Shader(InShader)
	{
	}
};

// Structure for describing query creation parameters
struct FRHIQueryCreateInfo
{
	FString		  DebugName;  // Debug name for the query
	ERHIQueryType QueryType;  // Type of the query (e.g., occlusion, timestamp)
	TUINT32		  NumQueries; // Number of queries

	// Constructor for easy initialization
	FRHIQueryCreateInfo(const FString& InDebugName, ERHIQueryType InQueryType, TUINT32 InNumQueries = 1)
		: DebugName(InDebugName)
		, QueryType(InQueryType)
		, NumQueries(InNumQueries)
	{
	}
};

// Structure for describing swap chain creation parameters
struct FRHISwapChainCreateDesc
{
	FString			DebugName;	   // Debug name for the swap chain
	FIntVec2		Dimensions;	   // Width and height of the swap chain buffers
	EPixelFormat	Format;		   // Pixel format of the swap chain buffers
	TUINT32			NumBuffers;	   // Number of buffers in the swap chain (e.g., double or triple buffering)
	TUINT32			NumSamples;	   // Number of samples (for MSAA)
	ESwapChainFlags Flags;		   // Swap chain creation flags
	FRHIViewport*	OwnerViewport; // Associated viewport

	// Default constructor
	FRHISwapChainCreateDesc() = default;

	// Constructor for easy initialization
	FRHISwapChainCreateDesc(
		const FString&	InDebugName,
		const FIntVec2& InDimensions,
		EPixelFormat	InFormat = EPixelFormat::R8G8B8A8_UNorm,
		TUINT32			InNumBuffers = 2, // Default to double buffering
		TUINT32			InNumSamples = 1, // Default to no MSAA
		ESwapChainFlags InFlags = ESwapChainFlags::None,
		FRHIViewport*	InViewport = nullptr)
		: DebugName(InDebugName)
		, Dimensions(InDimensions)
		, Format(InFormat)
		, NumBuffers(InNumBuffers)
		, NumSamples(InNumSamples)
		, Flags(InFlags)
		, OwnerViewport(InViewport)
	{
	}
};

// Structure for describing viewport creation parameters
struct FRHIViewportCreateDesc
{
	FString			 DebugName;	   // Debug name for the viewport
	FIntVec2		 Dimensions;   // Width and height of the viewport
	ERHIViewportType ViewportType; // Type of the viewport (e.g., windowed, fullscreen)
	EPixelFormat	 Format;	   // Pixel format of the viewport's back buffer
	IWindow*		 NativeWindowHandle;

	// Default constructor
	FRHIViewportCreateDesc() = default;

	// Constructor for easy initialization
	FRHIViewportCreateDesc(
		const FString&	 InDebugName,
		const FIntVec2&	 InDimensions,
		ERHIViewportType InViewportType = ERHIViewportType::Fullscreen,
		EPixelFormat	 InFormat = EPixelFormat::R8G8B8A8_UNorm,
		IWindow*		 InWindowHandle = nullptr)
		: DebugName(InDebugName)
		, Dimensions(InDimensions)
		, ViewportType(InViewportType)
		, Format(InFormat)
		, NativeWindowHandle(InWindowHandle)
	{
	}
};
