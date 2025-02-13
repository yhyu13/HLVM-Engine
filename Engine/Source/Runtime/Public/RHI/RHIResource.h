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
	RenderTargetView,
	DepthStencilView,
	PipelineState,
	Query,
	VertexDeclaration,
	// Add other resource types as needed
};

// Base class for all RHI resources
class FRHIResource : public FRefCountable
{
public:
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
	virtual TUINT32 GetSize() const { return CreateDesc.SizeInBytes; }

	// Returns the usage flags of the buffer
	virtual EBufferUsageFlags GetUsageFlags() const { return CreateDesc.UsageFlags; }

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

// Base class for RHI render target views
class FRHIRenderTargetView : virtual public FRHIResource
{
public:
	// Returns the type of the RHI resource
	virtual ERHIResourceType GetType() const override { return ERHIResourceType::RenderTargetView; }
};

// Base class for RHI depth-stencil views
class FRHIDepthStencilView : virtual public FRHIResource
{
public:
	// Returns the type of the RHI resource
	virtual ERHIResourceType GetType() const override { return ERHIResourceType::DepthStencilView; }
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

// Smart pointer types for RHI resources
using FTextureRHIRef = TRefCountPtr<FRHITexture>;
using FBufferRHIRef = TRefCountPtr<FRHIBuffer>;
using FShaderRHIRef = TRefCountPtr<FRHIShader>;
using FShaderResourceViewRHIRef = TRefCountPtr<FRHIShaderResourceView>;
using FUnorderedAccessViewRHIRef = TRefCountPtr<FRHIUnorderedAccessView>;
using FSamplerStateRHIRef = TRefCountPtr<FRHISamplerState>;
using FRenderTargetViewRHIRef = TRefCountPtr<FRHIRenderTargetView>;
using FDepthStencilViewRHIRef = TRefCountPtr<FRHIDepthStencilView>;
using FGraphicsPipelineStateRHIRef = TRefCountPtr<FRHIGraphicsPipelineState>;
using FComputePipelineStateRHIRef = TRefCountPtr<FRHIComputePipelineState>;
using FQueryRHIRef = TRefCountPtr<FRHIQuery>;
