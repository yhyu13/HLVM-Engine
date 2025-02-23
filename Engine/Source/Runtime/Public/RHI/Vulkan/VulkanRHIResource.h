/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "RHI/RHIResource.h"
#include "VulkanRHIResourceDeclaration.h"

// Base class for all RHI resources
class FVulkanRHIResource : virtual public FRHIResource
{
public:
	virtual ERHIInterfaceType GetInterfaceType() const override { return ERHIInterfaceType::Vulkan; }
};

// Vulkan-specific RHI texture
class FVulkanTexture : public FRHITexture, public FVulkanRHIResource
{
public:
	FVulkanTexture(VkImage InImage, const FRHITextureCreateDesc& InCreateDesc)
		: Image(InImage)
	{
		CreateDesc = InCreateDesc;
	}

	// Returns the dimensions of the texture
	virtual FIntVec3 GetSize() const override { return CreateDesc.Dimensions; }

	// Returns the pixel format of the texture
	virtual EPixelFormat GetFormat() const override { return CreateDesc.Format; }

	// Returns the texture flags
	virtual ETextureCreateFlags GetFlags() const override { return CreateDesc.Flags; }

	// Returns the Vulkan image handle
	VkImage GetImage() const { return Image; }

private:
	VkImage Image;
};

// Vulkan-specific RHI buffer
class FVulkanBuffer : public FRHIBuffer, public FVulkanRHIResource
{
public:
	FVulkanBuffer(VkBuffer InBuffer, VkDeviceMemory InMemory, const FRHIBufferCreateDesc& InCreateDesc)
		: Buffer(InBuffer), Memory(InMemory)
	{
		CreateDesc = InCreateDesc;
	}

	// Returns the size of the buffer in bytes
	virtual TUINT32 GetSize() const override { return CreateDesc.SizeInBytes; }

	// Returns the usage flags of the buffer
	virtual EBufferUsageFlags GetUsageFlags() const override { return CreateDesc.UsageFlags; }

	// Returns the Vulkan buffer handle
	VkBuffer GetBuffer() const { return Buffer; }

	// Returns the Vulkan device memory handle
	VkDeviceMemory GetMemory() const { return Memory; }

private:
	VkBuffer	   Buffer;
	VkDeviceMemory Memory;
};

// Vulkan-specific RHI shader
class FVulkanShader : public FRHIShader, public FVulkanRHIResource
{
public:
	FVulkanShader(VkShaderModule InShaderModule, const FShaderCreateInfo& InCreateDesc)
		: ShaderModule(InShaderModule)
	{
		CreateDesc = InCreateDesc;
	}

	// Returns the shader stage (e.g., vertex, pixel, compute)
	virtual EShaderStage GetStage() const override { return CreateDesc.Stage; }

	// Returns the Vulkan shader module handle
	VkShaderModule GetShaderModule() const { return ShaderModule; }

private:
	VkShaderModule ShaderModule;
};

// Vulkan-specific RHI shader resource view
class FVulkanShaderResourceView : public FRHIShaderResourceView, public FVulkanRHIResource
{
public:
	FVulkanShaderResourceView(VkImageView InImageView, const FRHIShaderResourceViewCreateInfo& InCreateDesc)
		: ImageView(InImageView)
	{
		CreateDesc = InCreateDesc;
	}

	// Returns the Vulkan image view handle
	VkImageView GetImageView() const { return ImageView; }

private:
	VkImageView ImageView;
};

// Vulkan-specific RHI unordered access view
class FVulkanUnorderedAccessView : public FRHIUnorderedAccessView, public FVulkanRHIResource
{
public:
	FVulkanUnorderedAccessView(VkBufferView InBufferView, const FRHIUnorderedAccessViewCreateInfo& InCreateDesc)
		: BufferView(InBufferView)
	{
		CreateDesc = InCreateDesc;
	}

	// Returns the Vulkan buffer view handle
	VkBufferView GetBufferView() const { return BufferView; }

private:
	VkBufferView BufferView;
};

// Vulkan-specific RHI sampler state
class FVulkanSamplerState : public FRHISamplerState, public FVulkanRHIResource
{
public:
	FVulkanSamplerState(VkSampler InSampler)
		: Sampler(InSampler) {}

	// Returns the Vulkan sampler handle
	VkSampler GetSampler() const { return Sampler; }

private:
	VkSampler Sampler;
};

// Vulkan-specific RHI render target view
class FVulkanRenderTargetView : public FRHIRenderTargetView, public FVulkanRHIResource
{
public:
	FVulkanRenderTargetView(VkImageView InImageView)
		: ImageView(InImageView) {}

	// Returns the Vulkan image view handle
	VkImageView GetImageView() const { return ImageView; }

private:
	VkImageView ImageView;
};

// Vulkan-specific RHI depth-stencil view
class FVulkanDepthStencilView : public FRHIDepthStencilView, public FVulkanRHIResource
{
public:
	FVulkanDepthStencilView(VkImageView InImageView)
		: ImageView(InImageView) {}

	// Returns the Vulkan image view handle
	VkImageView GetImageView() const { return ImageView; }

private:
	VkImageView ImageView;
};

// Vulkan-specific RHI graphics pipeline state
class FVulkanGraphicsPipelineState : public FRHIGraphicsPipelineState, public FVulkanRHIResource
{
public:
	FVulkanGraphicsPipelineState(VkPipeline InPipeline, VkPipelineLayout InPipelineLayout)
		: Pipeline(InPipeline), PipelineLayout(InPipelineLayout) {}

	// Returns the Vulkan pipeline handle
	VkPipeline GetPipeline() const { return Pipeline; }

	// Returns the Vulkan pipeline layout handle
	VkPipelineLayout GetPipelineLayout() const { return PipelineLayout; }

private:
	VkPipeline		 Pipeline;
	VkPipelineLayout PipelineLayout;
};

// Vulkan-specific RHI compute pipeline state
class FVulkanComputePipelineState : public FRHIComputePipelineState, public FVulkanRHIResource
{
public:
	FVulkanComputePipelineState(VkPipeline InPipeline, VkPipelineLayout InPipelineLayout)
		: Pipeline(InPipeline), PipelineLayout(InPipelineLayout) {}

	// Returns the Vulkan pipeline handle
	VkPipeline GetPipeline() const { return Pipeline; }

	// Returns the Vulkan pipeline layout handle
	VkPipelineLayout GetPipelineLayout() const { return PipelineLayout; }

private:
	VkPipeline		 Pipeline;
	VkPipelineLayout PipelineLayout;
};

// Vulkan-specific RHI query
class FVulkanQuery : public FRHIQuery, public FVulkanRHIResource
{
public:
	FVulkanQuery(VkQueryPool InQueryPool, TUINT32 InQueryIndex, ERHIQueryType InQueryType)
		: QueryPool(InQueryPool), QueryIndex(InQueryIndex), QueryType(InQueryType) {}

	// Returns the type of the query (e.g., occlusion, timestamp)
	virtual ERHIQueryType GetQueryType() const override { return QueryType; }

	// Returns the Vulkan query pool handle
	VkQueryPool GetQueryPool() const { return QueryPool; }

	// Returns the query index within the pool
	TUINT32 GetQueryIndex() const { return QueryIndex; }

private:
	VkQueryPool	  QueryPool;
	TUINT32		  QueryIndex;
	ERHIQueryType QueryType;
};

class FVulkanViewport;
class FVulkanBackBuffer : public FVulkanTexture
{
public:
	FVulkanBackBuffer(VkImage InImage, const FRHITextureCreateDesc& InCreateDesc, FVulkanViewport* InViewport)
		: FVulkanTexture(InImage, InCreateDesc), Viewport(InViewport)
	{
	}

	~FVulkanBackBuffer() override
	{
		HLVM_ASSERT(Viewport);
	}

private:
	FVulkanViewport* Viewport;
};

class FVulkanViewport : public FRHIViewport, public FVulkanRHIResource, public FVulkanMinimalContext
{
public:
	FVulkanViewport(const FRHIViewportCreateDesc& InCreateDesc, const FVulkanMinimalContext& InContext)
		: FRHIViewport(InCreateDesc), FVulkanMinimalContext(InContext)
	{
	}

	// Returns the Vulkan swap chain handle
	void* GetSwapChain() const override { return SwapChain; }

private:
	FVulkanSwapChain* SwapChain;
};

// Smart pointer types for Vulkan RHI resources
using FVulkanTextureRef = TRefCountPtr<FVulkanTexture>;
using FVulkanBufferRef = TRefCountPtr<FVulkanBuffer>;
using FVulkanShaderRef = TRefCountPtr<FVulkanShader>;
using FVulkanShaderResourceViewRef = TRefCountPtr<FVulkanShaderResourceView>;
using FVulkanUnorderedAccessViewRef = TRefCountPtr<FVulkanUnorderedAccessView>;
using FVulkanSamplerStateRef = TRefCountPtr<FVulkanSamplerState>;
using FVulkanRenderTargetViewRef = TRefCountPtr<FVulkanRenderTargetView>;
using FVulkanDepthStencilViewRef = TRefCountPtr<FVulkanDepthStencilView>;
using FVulkanGraphicsPipelineStateRef = TRefCountPtr<FVulkanGraphicsPipelineState>;
using FVulkanComputePipelineStateRef = TRefCountPtr<FVulkanComputePipelineState>;
using FVulkanQueryRef = TRefCountPtr<FVulkanQuery>;
