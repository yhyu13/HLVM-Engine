/**
* Copyright (c) 2025. MIT License. All rights reserved.
*/

#pragma once

#include "RHIDefinition.h"
#include "Core/Object/RefCountPtr.h"

// Forward Declarations
class FRHICommandListBase;
class FRHICommandListImmediate;
class FRHIComputeCommandList;


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
class FRHITexture : public FRHIResource
{
public:
	virtual ~FRHITexture() override = default;

	// Returns the dimensions of the texture
	virtual FVec3 GetSize() const = 0;

	// Returns the pixel format of the texture
	virtual EPixelFormat GetFormat() const = 0;

	// Returns the texture flags
	virtual ETextureCreateFlags GetFlags() const = 0;

	// Returns the type of the RHI resource
	virtual ERHIResourceType GetType() const override { return ERHIResourceType::Texture; }
};

// Base class for RHI buffers
class FRHIBuffer : public FRHIResource
{
public:
	virtual ~FRHIBuffer() override = default;

	// Returns the size of the buffer in bytes
	virtual TUINT32 GetSize() const = 0;

	// Returns the usage flags of the buffer
	virtual EBufferUsageFlags GetUsageFlags() const = 0;

	// Returns the type of the RHI resource
	virtual ERHIResourceType GetType() const override { return ERHIResourceType::Buffer; }
};

// Base class for RHI shaders
class FRHIShader : public FRHIResource
{
public:
	virtual ~FRHIShader() override = default;

	// Returns the shader stage (e.g., vertex, pixel, compute)
	virtual EShaderStage GetStage() const = 0;

	// Returns the type of the RHI resource
	virtual ERHIResourceType GetType() const override { return ERHIResourceType::Shader; }
};

// Base class for RHI shader resource views
class FRHIShaderResourceView : public FRHIResource
{
public:
	virtual ~FRHIShaderResourceView() override = default;

	// Returns the type of the RHI resource
	virtual ERHIResourceType GetType() const override { return ERHIResourceType::ShaderResourceView; }
};

// Base class for RHI unordered access views
class FRHIUnorderedAccessView : public FRHIResource
{
public:
	virtual ~FRHIUnorderedAccessView() override = default;

	// Returns the type of the RHI resource
	virtual ERHIResourceType GetType() const override { return ERHIResourceType::UnorderedAccessView; }
};

// Base class for RHI sampler states
class FRHISamplerState : public FRHIResource
{
public:
	virtual ~FRHISamplerState() override = default;

	// Returns the type of the RHI resource
	virtual ERHIResourceType GetType() const override { return ERHIResourceType::SamplerState; }
};

// Base class for RHI render target views
class FRHIRenderTargetView : public FRHIResource
{
public:
	virtual ~FRHIRenderTargetView() override = default;

	// Returns the type of the RHI resource
	virtual ERHIResourceType GetType() const override { return ERHIResourceType::RenderTargetView; }
};

// Base class for RHI depth-stencil views
class FRHIDepthStencilView : public FRHIResource
{
public:
	virtual ~FRHIDepthStencilView() override = default;

	// Returns the type of the RHI resource
	virtual ERHIResourceType GetType() const override { return ERHIResourceType::DepthStencilView; }
};

// Base class for RHI pipeline states
class FRHIGraphicsPipelineState : public FRHIResource
{
public:
	virtual ~FRHIGraphicsPipelineState() override = default;

	// Returns the type of the RHI resource
	virtual ERHIResourceType GetType() const override { return ERHIResourceType::PipelineState; }
};

class FRHIComputePipelineState : public FRHIResource
{
public:
	virtual ~FRHIComputePipelineState() override = default;

	// Returns the type of the RHI resource
	virtual ERHIResourceType GetType() const override { return ERHIResourceType::PipelineState; }
};

// Base class for RHI queries
class FRHIQuery : public FRHIResource
{
public:
	virtual ~FRHIQuery() override = default;

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
