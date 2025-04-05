/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "GlobalDefinition.h"
#include "Math/MathGLM.h"

DECLARE_LOG_CATEGORY(LogRHI)

// RHI Interface Types
enum class ERHIInterfaceType : TUINT32
{
	Vulkan,
	Null,
	// Add other RHI types as needed
};

// Enumeration of pixel formats
enum class EPixelFormat : TUINT32
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
enum class ETextureCreateFlag : TUINT32
{
	None = 0,
	RenderTarget = 1 << 0,
	DepthStencil = 1 << 1,
	ShaderResource = 1 << 2,
	ShaderWrite = 1 << 3,
	Transient = 1 << 4,
	// Add more flags as needed
};
HLVM_DECLARE_ENMU_FLAGS(ETextureCreateFlag, ETextureCreateFlags)

// Enumeration of buffer usage flags
enum class EBufferUsageFlag : TUINT32
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
HLVM_DECLARE_ENMU_FLAGS(EBufferUsageFlag, EBufferUsageFlags)

// Enumeration of memory property flags
enum class EMemoryPropertyFlag : TUINT32
{
	None = 0,
	DeviceLocal = 1 << 0,            // VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	HostVisible = 1 << 1,            // VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
	HostCoherent = 1 << 2,           // VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
	HostCached = 1 << 3,             // VK_MEMORY_PROPERTY_HOST_CACHED_BIT
	LazilyAllocated = 1 << 4,        // VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT
	Protected = 1 << 5,              // VK_MEMORY_PROPERTY_PROTECTED_BIT
	DeviceCoherentAMD = 1 << 6,      // VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD
	DeviceUncachedAMD = 1 << 7,      // VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD
	RDMACapableNV = 1 << 8           // VK_MEMORY_PROPERTY_RDMA_CAPABLE_BIT_NV
};
HLVM_DECLARE_ENMU_FLAGS(EMemoryPropertyFlag, EMemoryPropertyFlags)


// Enumeration of shader stages
enum class EShaderStage : TUINT32
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
enum class ERHIQueryType : TUINT32
{
	Occlusion,
	Timestamp,
	PipelineStatistics,
	// Add other query types as needed
};

// Enumeration of texture filter modes
enum class ETextureFilter : TUINT32
{
	None,
	Point,
	Linear,
	Anisotropic
};

// Enumeration of texture address modes
enum class ETextureAddressMode : TUINT32
{
	None,
	Wrap,
	Clamp,
	Mirror,
	Border
};

// Enumeration of primitive topologies
enum class EPrimitiveTopology : TUINT32
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
enum class EPolygonMode : TUINT32
{
	Fill,
	Line,
	Point
};

// Enumeration of front face orientations
enum class EFrontFace : TUINT32
{
	Clockwise,
	CounterClockwise
};

// Enumeration of cull modes
enum class ECullMode : TUINT32
{
	None,
	Front,
	Back,
	FrontAndBack
};

// Enumeration of depth test modes
enum class EDepthTest : TUINT32
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
enum class EStencilTest : TUINT32
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
enum class EBlendMode : TUINT32
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
enum class ERHICompare : TUINT32
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
enum class ERHIViewportType : TUINT32
{
	Windowed,
	Fullscreen,
	// Add other viewport types as needed
};

// Enumeration of swap chain flags
enum class ESwapChainFlags : TUINT32
{
	None = 0,
	AllowTearing = 1 << 0, // Allows tearing for adaptive sync (e.g., FreeSync, G-Sync)
	Stereo = 1 << 1,       // Stereo rendering (e.g., VR)
	HDR = 1 << 2,          // High Dynamic Range (HDR) support
};

enum class ESubpassType : TUINT32
{
	Default,
	DepthReading,
	DeferredShading,
	CustomResolve,
};

enum class ERenderTargetLoadAction : TUINT32
{
	DontCare,
	Load,
	Clear,
};

enum class ERenderTargetStoreAction : TUINT32
{
	DontCare,
	Store,
	MultisampleResolve,
};

enum class ERHIAccessFlag : TUINT32
{
	// Used when the previous state of a resource is not known,
	// which implies we have to flush all GPU caches etc.
	Unknown = 0,

	// Read states
	CPURead                 = 1 <<  0,
	Present                 = 1 <<  1,
	IndirectArgs            = 1 <<  2,
	VertexOrIndexBuffer     = 1 <<  3,
	SRVCompute              = 1 <<  4,
	SRVGraphicsPixel        = 1 <<  5,
	SRVGraphicsNonPixel     = 1 <<  6,
	CopySrc                 = 1 <<  7,
	ResolveSrc              = 1 <<  8,
	DSVRead                 = 1 <<  9,

	// Read-write states
	UAVCompute              = 1 << 10,
	UAVGraphics             = 1 << 11,
	RTV                     = 1 << 12,
	CopyDest                = 1 << 13,
	ResolveDst              = 1 << 14,
	DSVWrite                = 1 << 15,

	// Ray tracing acceleration structure states.
	// Buffer that contains an AS must always be in either of these states.
	// BVHRead -- required for AS inputs to build/update/copy/trace commands.
	// BVHWrite -- required for AS outputs of build/update/copy commands.
	BVHRead                  = 1 << 16,
	BVHWrite                 = 1 << 17,

	// Invalid released state (transient resources)
	Discard                = 1 << 18,

	// Shading Rate Source
	ShadingRateSource  = 1 << 19,

	Last = ShadingRateSource,
	None = Unknown,
	Mask = (Last << 1) - 1,

	// Graphics is a combination of pixel and non-pixel
	SRVGraphics = SRVGraphicsPixel | SRVGraphicsNonPixel,

	// A mask of the two possible SRV states
	SRVMask = SRVCompute | SRVGraphics,

	// A mask of the two possible UAV states
	UAVMask = UAVCompute | UAVGraphics,

	// A mask of all bits representing read-only states which cannot be combined with other write states.
	ReadOnlyExclusiveMask = CPURead | Present | IndirectArgs | VertexOrIndexBuffer | SRVGraphics | SRVCompute | CopySrc | ResolveSrc | BVHRead | ShadingRateSource,

	// A mask of all bits representing read-only states on the compute pipe which cannot be combined with other write states.
	ReadOnlyExclusiveComputeMask = CPURead | IndirectArgs | SRVCompute | CopySrc | BVHRead,

	// A mask of all bits representing read-only states which may be combined with other write states.
	ReadOnlyMask = ReadOnlyExclusiveMask | DSVRead | ShadingRateSource,

	// A mask of all bits representing readable states which may also include writable states.
	ReadableMask = ReadOnlyMask | UAVMask,

	// A mask of all bits representing write-only states which cannot be combined with other read states.
	WriteOnlyExclusiveMask = RTV | CopyDest | ResolveDst,

	// A mask of all bits representing write-only states which may be combined with other read states.
	WriteOnlyMask = WriteOnlyExclusiveMask | DSVWrite,

	// A mask of all bits representing writable states which may also include readable states.
	WritableMask = WriteOnlyMask | UAVMask | BVHWrite,

	// A mask of all bits representing read-write states.
	ReadWrite = DSVRead | DSVWrite,
};
HLVM_DECLARE_ENMU_FLAGS(ERHIAccessFlag, ERHIAccessFlags)

enum class EVariableRateShadingCombiner : TUINT32
{
	Passthrough,
	Override,
	Min,
	Max,
	Sum
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

// Utility Functions
inline const TCHAR* GetRHIName(ERHIInterfaceType Type)
{
	switch (Type)
	{
		case ERHIInterfaceType::Vulkan: return TXT("Vulkan");
		case ERHIInterfaceType::Null: return TXT("Null");
	}
};

namespace RHI
{
	constexpr TUINT32 RT_ATTACHMENT_MAX = 8;
}
