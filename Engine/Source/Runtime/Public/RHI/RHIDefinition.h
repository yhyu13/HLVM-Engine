/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "GlobalDefinition.h"
#include "Math/MathGLM.h"

// RHI Interface Types
enum class ERHIInterfaceType : TUINT8
{
	Vulkan,
	Null,
	// Add other RHI types as needed
};

// Pixel Formats
enum class EPixelFormat : TUINT8
{
	PF_Unknown,
	PF_R8G8B8A8,
	PF_B8G8R8A8,
	PF_FloatRGBA,
	PF_DepthStencil,
	// Add other pixel formats as needed
};

// Texture Flags
enum class ETextureCreateFlags : TUINT32
{
	None = 0,
	RenderTargetable = 1 << 0,
	DepthStencilTargetable = 1 << 1,
	ShaderResource = 1 << 2,
	Dynamic = 1 << 3,
	// Add other texture flags as needed
};

// Buffer Flags
enum class EBufferUsageFlags : TUINT32
{
	None = 0,
	VertexBuffer = 1 << 0,
	IndexBuffer = 1 << 1,
	StructuredBuffer = 1 << 2,
	// Add other buffer usage flags as needed
};


// Shader Stages
enum class EShaderStage : TUINT8
{
	Vertex,
	Pixel,
	Compute,
	Geometry,
	Hull,
	Domain,
	// Add other shader stages as needed
};

// Query Types
enum class ERHIQueryType : TUINT8
{
	Occlusion,
	Timestamp,
	PipelineStatistics,
	// Add other query types as needed
};

// RHI Resource Types
class FRHITexture;
class FRHIBuffer;
class FRHIShader;
class FRHIShaderResourceView;
class FRHIUnorderedAccessView;
class FRHISamplerState;
class FRHIRenderTargetView;
class FRHIDepthStencilView;
class FRHIShaderParameterStruct;

// RHI Command List Types
class FRHICommandListBase;
class FRHICommandListImmediate;
class FRHIComputeCommandList;

// RHI Pipeline State Types
class FRHIGraphicsPipelineState;
class FRHIComputePipelineState;

// RHI Viewport and Swap Chain Types
class FViewportRHIRef;

// RHI Query Types
class FRHIQueryRHIRef;

// Macros for RHI Debugging and Verification
#define CHECK_RHI_RESOURCE(Resource) checkf(Resource != nullptr, TXT("Invalid RHI resource: %s"), TXT(#Resource))
#define VERIFY_RHI_RESULT(Result) checkf(Result == true, TXT("RHI operation failed: %s"), TXT(#Result))

// Macros for RHI Resource Creation
#define CREATE_RHI_TEXTURE(Desc) GDynamicRHI->CreateTexture(Desc)
#define CREATE_RHI_BUFFER(Desc) GDynamicRHI->CreateBuffer(Desc)

// Macros for RHI Command List
#define RHICmdList GDynamicRHI->GetImmediateCommandList()

// Macros for RHI Synchronization
#define FLUSH_RHI_COMMANDS() GDynamicRHI->RHISubmitCommandsAndFlushGPU()

// Macros for RHI Debugging Events
#define RHI_PUSH_EVENT(Name, Color) GDynamicRHI->RHIPushEvent(TXT(Name), Color)
#define RHI_POP_EVENT() GDynamicRHI->RHIPopEvent()

// Utility Functions
inline const TCHAR* GetRHIName(ERHIInterfaceType Type)
{
	switch (Type)
	{
		case ERHIInterfaceType::Vulkan: return TXT("Vulkan");
		case ERHIInterfaceType::Null: return TXT("Null");
	}
};
