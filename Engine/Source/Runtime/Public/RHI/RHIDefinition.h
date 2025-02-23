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

// Enumeration of pixel formats
enum class EPixelFormat : TUINT8
{
	Unknown,
	R8_UNorm,
	R8G8_UNorm,
	R8G8B8A8_UNorm,
	R16_UNorm,
	R16G16_UNorm,
	R16G16B16A16_UNorm,
	R32_UInt,
	R32G32_UInt,
	R32G32B32A32_UInt,
	R32_Float,
	R32G32_Float,
	R32G32B32A32_Float,
	D16_UNorm,
	D24_UNorm_S8_UInt,
	D32_Float,
	D32_Float_S8_UInt,
	// Add more formats as needed
};

// Enumeration of texture creation flags
enum class ETextureCreateFlags : TUINT8
{
	None = 0,
	RenderTarget = 1 << 0,
	DepthStencil = 1 << 1,
	ShaderResource = 1 << 2,
	ShaderWrite = 1 << 3,
	Transient = 1 << 4,
	// Add more flags as needed
};
HLVM_ENUM_FLAG_OPERATOR(ETextureCreateFlags, &)

// Enumeration of buffer usage flags
enum class EBufferUsageFlags : TUINT8
{
	None = 0,
	Vertex = 1 << 0,             // Buffer is used as a vertex buffer
	Index = 1 << 1,              // Buffer is used as an index buffer
	Uniform = 1 << 2,            // Buffer is used as a uniform buffer
	Storage = 1 << 3,            // Buffer is used as a storage buffer
	ShaderResource = 1 << 4,     // Buffer is used as a shader resource
	TransferSource = 1 << 5,     // Buffer is used as a transfer source
	TransferDestination = 1 << 6 // Buffer is used as a transfer destination
};
HLVM_ENUM_FLAG_OPERATOR(EBufferUsageFlags, &)

// Enumeration of shader stages
enum class EShaderStage : TUINT8
{
	Vertex,
	Pixel, // Also known as Fragment in Vulkan
	Compute,
	Geometry,
	Hull, // Also known as Tessellation Control in Vulkan
	Domain, // Also known as Tessellation Evaluation in Vulkan
	RayGeneration,
	Intersection,
	AnyHit,
	ClosestHit,
	Miss,
	Callable
};

// Query Types
enum class ERHIQueryType : TUINT8
{
	Occlusion,
	Timestamp,
	PipelineStatistics,
	// Add other query types as needed
};

// Enumeration of texture filter modes
enum class ETextureFilter : TUINT8
{
	None,
	Point,
	Linear,
	Anisotropic
};

// Enumeration of texture address modes
enum class ETextureAddressMode : TUINT8
{
	None,
	Wrap,
	Clamp,
	Mirror,
	Border
};

// Enumeration of primitive topologies
enum class EPrimitiveTopology : TUINT8
{
	Undefined,
	PointList,
	LineList,
	LineStrip,
	TriangleList,
	TriangleStrip,
	PatchList
};

// Enumeration of polygon modes
enum class EPolygonMode : TUINT8
{
	Fill,
	Line,
	Point
};

// Enumeration of front face orientations
enum class EFrontFace : TUINT8
{
	Clockwise,
	CounterClockwise
};

// Enumeration of cull modes
enum class ECullMode : TUINT8
{
	None,
	Front,
	Back,
	FrontAndBack
};

// Enumeration of depth test modes
enum class EDepthTest : TUINT8
{
	Never,
	Less,
	Equal,
	LessEqual,
	Greater,
	NotEqual,
	GreaterEqual,
	Always
};

// Enumeration of stencil test modes
enum class EStencilTest : TUINT8
{
	Never,
	Less,
	Equal,
	LessEqual,
	Greater,
	NotEqual,
	GreaterEqual,
	Always
};

// Enumeration of blend modes
enum class EBlendMode : TUINT8
{
	Opaque,
	Masked,
	Translucent,
	Additive,
	Modulate,
	AlphaComposite,
	Custom
};

// Enumeration of comparison functions
enum class ECompareFunction : TUINT8
{
	Never,
	Less,
	Equal,
	LessEqual,
	Greater,
	NotEqual,
	GreaterEqual,
	Always
};

// Enumeration of viewport types (e.g., windowed, fullscreen)
enum class ERHIViewportType : TUINT8
{
	Windowed,
	Fullscreen,
	// Add other viewport types as needed
};

// Enumeration of swap chain flags
enum class ESwapChainFlags : TUINT8
{
	None = 0,
	AllowTearing = 1 << 0, // Allows tearing for adaptive sync (e.g., FreeSync, G-Sync)
	Stereo = 1 << 1,       // Stereo rendering (e.g., VR)
	HDR = 1 << 2,          // High Dynamic Range (HDR) support
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
class FRHIViewport;

// RHI Command List Types
class FRHICommandListBase;
class FRHICommandListImmediate;
class FRHIComputeCommandList;

// RHI Pipeline State Types
class FRHIGraphicsPipelineState;
class FRHIComputePipelineState;

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

#define RHI_MAX_SIMULTANEOUS_RENDER_TARGETS 8
