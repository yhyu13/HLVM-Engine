/**
* Copyright (c) 2025. MIT License. All rights reserved.
*/

#pragma once

#include "RHIDefinition.h"
#include "RHIResource.h"
#include "RHITextureReference.h"
#include "RHIShader.h"
#include "RHIBuffer.h"
#include "RHIVertexDeclaration.h"
#include "RHIShaderResourceView.h"
#include "RHIUnorderedAccessView.h"
#include "RHIRenderPassInfo.h"
#include "RHIShaderParameters.h"
#include "RHIShaderParameterStruct.h"
#include "RHIShaderParameterStructInline.h"
#include "RHIShaderParameterStructResource.h"
#include "RHIShaderParameterStructSampler.h"
#include "RHIShaderParameterStructUniformBuffer.h"
#include "RHIShaderParameterStructStorageBuffer.h"
#include "RHIShaderParameterStructRayTracing.h"
#include "RHIShaderParameterStructInlineUniformBuffer.h"
#include "RHIShaderParameterStructResourceArray.h"
#include "RHIShaderParameterStructResourceTable.h"
#include "RHIShaderParameterStructResourceView.h"
#include "RHIShaderParameterStructSamplerState.h"
#include "RHIShaderParameterStructUniformBufferArray.h"
#include "RHIShaderParameterStructStorageBufferArray.h"
#include "RHIShaderParameterStructRayTracingArray.h"
#include "RHIShaderParameterStructInlineUniformBufferArray.h"
#include "RHIShaderParameterStructResourceArrayArray.h"
#include "RHIShaderParameterStructResourceTableArray.h"
#include "RHIShaderParameterStructResourceViewArray.h"
#include "RHIShaderParameterStructSamplerStateArray.h"

class FDynamicRHI
{
public:
	virtual ~FDynamicRHI() = default;

	// RHI Interface Type
	virtual ERHIInterfaceType GetInterfaceType() const = 0;

	// Initialization and Shutdown
	virtual void Init() = 0;
	virtual void Shutdown() = 0;

	// Resource Creation
	virtual FTextureRHIRef CreateTexture(const FRHITextureCreateDesc& CreateDesc) = 0;
	virtual FBufferRHIRef CreateBuffer(const FRHIBufferCreateDesc& CreateDesc) = 0;
	virtual FShaderResourceViewRHIRef CreateShaderResourceView(FRHITexture* Texture, const FRHIShaderResourceViewCreateInfo& CreateInfo) = 0;
	virtual FUnorderedAccessViewRHIRef CreateUnorderedAccessView(FRHITexture* Texture, const FRHIUnorderedAccessViewCreateInfo& CreateInfo) = 0;
	virtual FVertexDeclarationRHIRef CreateVertexDeclaration(const FVertexDeclarationElementList& Elements) = 0;

	// Shader Management
	virtual FShaderRHIRef CreateShader(const FShaderCreateInfo& CreateInfo) = 0;
	virtual void ReleaseShader(FShaderRHIRef& Shader) = 0;

	// Pipeline State Management
	virtual FRHIGraphicsPipelineState* CreateGraphicsPipelineState(const FGraphicsPipelineStateInitializer& Initializer) = 0;
	virtual FRHIComputePipelineState* CreateComputePipelineState(const FComputePipelineStateInitializer& Initializer) = 0;

	// Command List and Context
	virtual FRHICommandListImmediate& GetImmediateCommandList() = 0;
	virtual FRHIComputeCommandList& GetComputeCommandList() = 0;

	// Synchronization
	virtual void RHISubmitCommandsAndFlushGPU() = 0;
	virtual void RHIFlushResources() = 0;

	// Viewport and Swap Chain
	virtual void RHICreateViewport(void* WindowHandle, uint32 Width, uint32 Height, bool bIsFullscreen, EPixelFormat PreferredPixelFormat, FViewportRHIRef& OutViewport) = 0;
	virtual void RHIResizeViewport(FViewportRHIRef& Viewport, uint32 Width, uint32 Height, bool bIsFullscreen, EPixelFormat PreferredPixelFormat) = 0;
	virtual void RHISwapBuffers(FViewportRHIRef& Viewport) = 0;

	// Render Pass and Draw Commands
	virtual void RHIBeginRenderPass(const FRHIRenderPassInfo& RenderPassInfo, const TCHAR* Name) = 0;
	virtual void RHIEndRenderPass() = 0;
	virtual void RHIDrawPrimitive(uint32 BaseVertexIndex, uint32 NumPrimitives, uint32 NumInstances) = 0;
	virtual void RHIDrawIndexedPrimitive(FRHIBuffer* IndexBuffer, uint32 BaseVertexIndex, uint32 FirstInstance, uint32 NumVertices, uint32 StartIndex, uint32 NumPrimitives, uint32 NumInstances) = 0;

	// Compute Dispatch
	virtual void RHIDispatchComputeShader(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ) = 0;

	// Query and Timestamp
	virtual FRHIQueryRHIRef CreateQuery(ERHIQueryType QueryType) = 0;
	virtual void RHIBeginQuery(FRHIQueryRHIRef& Query) = 0;
	virtual void RHIEndQuery(FRHIQueryRHIRef& Query) = 0;
	virtual void RHIGetQueryResults(FRHIQueryRHIRef& Query, uint64& OutResult, bool bWait) = 0;

	// Debugging and Profiling
	virtual void RHIPushEvent(const TCHAR* Name, FColor Color) = 0;
	virtual void RHIPopEvent() = 0;

	// Memory Management
	virtual void RHIFlushPendingDeletes() = 0;

	// Misc
	virtual void RHISetGraphicsPipelineState(FRHIGraphicsPipelineState* PipelineState) = 0;
	virtual void RHISetComputePipelineState(FRHIComputePipelineState* PipelineState) = 0;
	virtual void RHISetViewport(uint32 MinX, uint32 MinY, float MinZ, uint32 MaxX, uint32 MaxY, float MaxZ) = 0;
	virtual void RHISetScissorRect(bool bEnable, uint32 MinX, uint32 MinY, uint32 MaxX, uint32 MaxY) = 0;
};