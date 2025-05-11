/**
* Copyright (c) 2025. MIT License. All rights reserved.
*/

#pragma once

#include "RHIDefinition.h"
#include "RHIResourcePost.h"
#include "RHIPipeline.h"
#include "RHICommand.h"

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
	virtual FRHITextureRef CreateTexture(const FRHITextureCreateInfo& CreateInfo) = 0;
	virtual FRHISamplerStateRef CreateSamplerState(const FRHISamplerStateCreateInfo& CreateInfo) = 0;
	virtual FRHIBufferRef CreateBuffer(const FRHIBufferCreateInfo& CreateInfo) = 0;
	virtual FShaderResourceViewRHIRef CreateShaderResourceView(FRHITexture* Texture, const FRHIShaderResourceViewCreateInfo& CreateInfo) = 0;
	virtual FUnorderedAccessViewRHIRef CreateUnorderedAccessView(FRHIBuffer* Buffer, const FRHIUnorderedAccessViewCreateInfo& CreateInfo) = 0;
	virtual FVertexDeclarationRHIRef CreateVertexDeclaration(const FVertexDeclarationElementList& Elements) = 0;

	// Shader Management
	virtual FRHIShaderRef CreateShader(const FShaderCreateInfo& CreateInfo) = 0;

	// Pipeline State Management
	virtual FRHIGraphicsPSO* CreateGraphicsPSO(const FGraphicsPSOInitializer& Initializer) = 0;
	virtual FRHIComputePSO* CreateComputePSO(const FComputePSOInitializer& Initializer) = 0;

	// Command List and Context
	virtual FRHICommandListImmediate& GetImmediateCommandList() = 0;
	virtual FRHIComputeCommandList& GetComputeCommandList() = 0;

	// Synchronization
	virtual void RHISubmitCommandsAndFlushGPU() = 0;
	virtual void RHIFlushResources() = 0;

	// Viewport and Swap Chain
	virtual void RHICreateViewport(void* WindowHandle, TUINT32 Width, TUINT32 Height, bool bIsFullscreen, EPixelFormat PreferredPixelFormat, FRHIViewportRef& OutViewport) = 0;
	virtual void RHIResizeViewport(FRHIViewportRef& Viewport, TUINT32 Width, TUINT32 Height, bool bIsFullscreen, EPixelFormat PreferredPixelFormat) = 0;
	virtual void RHISwapBuffers(FRHIViewportRef& Viewport) = 0;
	virtual FRHITextureRef GetRHIBackBuffer() = 0;
	virtual FRHIViewportRef GetRHIViewport() = 0;

	// Render Pass and Draw Commands
	virtual void RHIBeginRenderPass(const FRHIRenderPassInfo& RenderPassInfo) = 0;
	virtual void RHIEndRenderPass() = 0;
	virtual void RHIDrawPrimitive(TUINT32 BaseVertexIndex, TUINT32 NumPrimitives, TUINT32 NumInstances) = 0;
	virtual void RHIDrawIndexedPrimitive(FRHIBuffer* IndexBuffer, TUINT32 BaseVertexIndex, TUINT32 FirstInstance, TUINT32 NumVertices, TUINT32 StartIndex, TUINT32 NumPrimitives, TUINT32 NumInstances) = 0;

	// Compute Dispatch
	virtual void RHIDispatchComputeShader(TUINT32 ThreadGroupCountX, TUINT32 ThreadGroupCountY, TUINT32 ThreadGroupCountZ) = 0;

	// Query and Timestamp
	virtual FQueryRHIRef CreateQuery(ERHIQueryType QueryType) = 0;
	virtual void RHIBeginQuery(FQueryRHIRef& Query) = 0;
	virtual void RHIEndQuery(FQueryRHIRef& Query) = 0;
	virtual void RHIGetQueryResults(FQueryRHIRef& Query, TUINT64& OutResult, bool bWait) = 0;

	// Debugging and Profiling
	virtual void RHIPushEvent(const TCHAR* Name) = 0;
	virtual void RHIPopEvent() = 0;

	// Memory Management
	virtual void RHIFlushPendingDeletes() = 0;

	// Misc
	virtual void RHISetGraphicsPSO(FRHIGraphicsPSO* PipelineState) = 0;
	virtual void RHISetComputePSO(FRHIComputePSO* PipelineState) = 0;
	virtual void RHISetViewport(TUINT32 MinX, TUINT32 MinY, float MinZ, TUINT32 MaxX, TUINT32 MaxY, float MaxZ) = 0;
	virtual void RHISetScissorRect(bool bEnable, TUINT32 MinX, TUINT32 MinY, TUINT32 MaxX, TUINT32 MaxY) = 0;
};

HLVM_EXTERN_VAR TNoNullablePtr<FDynamicRHI> GDynamicRHI;

template<typename T>
HLVM_INLINE_FUNC void SetDynamicRHI(T* RHI)
{
	GDynamicRHI = S_C(FDynamicRHI*, RHI);
}

template<typename T>
HLVM_INLINE_FUNC TNoNullablePtr<T> GetDynamicRHI()
{
	return S_C(T*, GDynamicRHI.Get());
}
