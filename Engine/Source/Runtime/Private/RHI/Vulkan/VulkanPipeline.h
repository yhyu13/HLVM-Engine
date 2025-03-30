/**
* Copyright (c) 2025. MIT License. All rights reserved.
*/

#pragma once

#include "RHI/RHIResource.h"
#include "VulkanRHIResourceDeclaration.h"



// Vulkan-specific RHI depth-stencil view
class FVulkanDepthStencilView : public FRHIDepthStencilView, public FVulkanResource
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
class FVulkanGraphicsPipelineState : public FRHIGraphicsPipelineState, public FVulkanResource
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
class FVulkanComputePipelineState : public FRHIComputePipelineState, public FVulkanResource
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
class FVulkanQuery : public FRHIQuery, public FVulkanResource
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

using FVulkanDepthStencilViewRef = TRefCountPtr<FVulkanDepthStencilView>;
using FVulkanGraphicsPipelineStateRef = TRefCountPtr<FVulkanGraphicsPipelineState>;
using FVulkanComputePipelineStateRef = TRefCountPtr<FVulkanComputePipelineState>;
using FVulkanQueryRef = TRefCountPtr<FVulkanQuery>;
