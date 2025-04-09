/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "Core/Object/RefCountPtr.h"
#include "RHIDefinition.h"
#include "RHIResourcePre.h"

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
	const FRHITextureCreateInfo& GetCreateInfo() const { return CreateInfo; }

	// Returns the dimensions of the texture
	FIntVec3 GetSize() const { return CreateInfo.Dimensions; }

	// Returns the pixel format of the texture
	EPixelFormat GetFormat() const { return CreateInfo.Format; }

	// Returns the texture flags
	ETextureCreateFlags GetFlags() const { return CreateInfo.Flags; }

	// Returns whether the texture is multisampled
	bool IsMultiSampled() const { return CreateInfo.NumSamples > 1; }

	// Returns the number of samples in the texture
	TUINT8 GetNumSamples() const { return CreateInfo.NumSamples; }

	// Returns the type of the RHI resource
	virtual ERHIResourceType GetType() const override { return ERHIResourceType::Texture; }

protected:
	FRHITextureCreateInfo CreateInfo; // Declaration struct as a member
};

// Base class for RHI buffers
class FRHIBuffer : virtual public FRHIResource
{
public:
	const FRHIBufferCreateInfo& GetCreateInfo() const { return CreateInfo; }

	// Returns the size of the buffer in bytes
	virtual TSIZE GetSize() const { return CreateInfo.Size; }

	// Returns the usage flags of the buffer
	virtual EBufferUsageFlags GetUsageFlags() const { return CreateInfo.UsageFlags; }

	// Returns the memory flags of the buffer
	virtual EMemoryPropertyFlags GetMemoryFlags() const { return CreateInfo.MemoryPropertyFlags; }

	// Returns the type of the RHI resource
	virtual ERHIResourceType GetType() const override { return ERHIResourceType::Buffer; }

protected:
	FRHIBufferCreateInfo CreateInfo; // Declaration struct as a member
};

// Base class for RHI shaders
class FRHIShader : virtual public FRHIResource
{
public:
	const FShaderCreateInfo& GetCreateInfo() const { return CreateInfo; }

	// Returns the shader stage (e.g., vertex, pixel, compute)
	virtual EShaderStage GetStage() const { return CreateInfo.Stage; }

	// Returns the type of the RHI resource
	virtual ERHIResourceType GetType() const override { return ERHIResourceType::Shader; }

protected:
	FShaderCreateInfo CreateInfo; // Declaration struct as a member
};

// Base class for RHI shader resource views
class FRHIShaderResourceView : virtual public FRHIResource
{
public:
	FRHIShaderResourceViewCreateInfo CreateInfo; // Declaration struct as a member

	// Returns the type of the RHI resource
	virtual ERHIResourceType GetType() const override { return ERHIResourceType::ShaderResourceView; }
};

// Base class for RHI unordered access views
class FRHIUnorderedAccessView : virtual public FRHIResource
{
public:
	FRHIUnorderedAccessViewCreateInfo CreateInfo; // Declaration struct as a member

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
	FRHIViewport(const FRHIViewportCreateInfo& InCreateInfo)
		: CreateInfo(InCreateInfo)
	{
	}

	// Returns the type of the RHI resource
	virtual ERHIResourceType GetType() const override { return ERHIResourceType::Viewport; }

	// Returns the dimensions of the viewport
	virtual FUIntVec2 GetSize() const { return CreateInfo.Dimensions; }

	// Returns the viewport type (e.g., windowed, fullscreen)
	virtual ERHIViewportType GetViewportType() const { return CreateInfo.ViewportType; }

	// Returns the associated swap chain (if any)
	virtual void* GetSwapChain() const = 0;

	// Resizes the viewport and swap chain
	virtual void Resize(const FUIntVec2& NewDimensions) = 0;

	//	// Presents the viewport (swaps the back buffer)
	//	virtual void Present() = 0;

protected:
	FRHIViewportCreateInfo CreateInfo; // Viewport creation description
};

// Smart pointer types for RHI resources
using FRHITextureRef = TRefCountPtr<FRHITexture>;
using FBufferRHIRef = TRefCountPtr<FRHIBuffer>;
using FShaderRHIRef = TRefCountPtr<FRHIShader>;
using FShaderResourceViewRHIRef = TRefCountPtr<FRHIShaderResourceView>;
using FUnorderedAccessViewRHIRef = TRefCountPtr<FRHIUnorderedAccessView>;
using FSamplerStateRHIRef = TRefCountPtr<FRHISamplerState>;
using FGraphicsPipelineStateRHIRef = TRefCountPtr<FRHIGraphicsPipelineState>;
using FComputePipelineStateRHIRef = TRefCountPtr<FRHIComputePipelineState>;
using FQueryRHIRef = TRefCountPtr<FRHIQuery>;
using FViewportRHIRef = TRefCountPtr<FRHIViewport>;
