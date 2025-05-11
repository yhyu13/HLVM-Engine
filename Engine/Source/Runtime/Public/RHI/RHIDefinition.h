/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "GlobalDefinition.h"
#include "Math/MathGLM.h"
#include "Utility/Hash.h"

#define PLATFORM_SUPPORTS_MESH_SHADERS 1
#define PLATFORM_SUPPORTS_GEOMETRY_SHADERS 1

DECLARE_LOG_CATEGORY(LogRHI)

enum class ERHIZBuffer : TUINT8
{
	Far = 0,
	Near = 1,
	IsInverted = (Far < Near) ? 1 : 0,
};

// RHI Interface Types
enum class ERHIInterfaceType : TUINT8
{
	Null = 0,
	Vulkan,
	// Add other RHI types as needed
};

// Utility Functions
HLVM_INLINE_FUNC const TCHAR* GetRHIName(ERHIInterfaceType Type)
{
	switch (Type)
	{
		case ERHIInterfaceType::Vulkan:
			return TXT("Vulkan");
		case ERHIInterfaceType::Null:
			return TXT("Null");
		default:
			HLVM_ASSERT_F(false, TXT("Unknown RHI Interface Type"));
			return TXT("Unknown");
	}
};

// Enumeration of pixel formats
enum class EPixelFormat : TUINT8
{
	None = 0,
	R8_UNorm,
	R8G8_UNorm,
	R8G8B8A8_UNorm,
	B8G8R8A8_SRGB,
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
	InputAttachment = 1 << 5,
	MemoryLess = 1 << 6,
	// Add more flags as needed
};
HLVM_DECLARE_ENMU_FLAGS(ETextureCreateFlag, ETextureCreateFlags)

namespace RHI
{
	constexpr TUINT32 MAX_RT_ATTACHMENTS = 8;
}

// Enumeration of buffer usage flags
enum class EBufferUsageFlag : TUINT32
{
	None = 0,
	Vertex = 1 << 0,			 // Buffer is used as a vertex buffer
	Index = 1 << 1,				 // Buffer is used as an index buffer
	Uniform = 1 << 2,			 // Buffer is used as a uniform buffer
	Storage = 1 << 3,			 // Buffer is used as a storage buffer
	ShaderResource = 1 << 4,	 // Buffer is used as a shader resource
	TransferSource = 1 << 5,	 // Buffer is used as a transfer source
	TransferDestination = 1 << 6 // Buffer is used as a transfer destination
};
HLVM_DECLARE_ENMU_FLAGS(EBufferUsageFlag, EBufferUsageFlags)

// Enumeration of memory property flags
enum class EMemoryPropertyFlag : TUINT32
{
	None = 0,
	DeviceLocal = 1 << 0,		// VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	HostVisible = 1 << 1,		// VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
	HostCoherent = 1 << 2,		// VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
	HostCached = 1 << 3,		// VK_MEMORY_PROPERTY_HOST_CACHED_BIT
	LazilyAllocated = 1 << 4,	// VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT
	Protected = 1 << 5,			// VK_MEMORY_PROPERTY_PROTECTED_BIT
	DeviceCoherentAMD = 1 << 6, // VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD
	DeviceUncachedAMD = 1 << 7, // VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD
	RDMACapableNV = 1 << 8		// VK_MEMORY_PROPERTY_RDMA_CAPABLE_BIT_NV
};
HLVM_DECLARE_ENMU_FLAGS(EMemoryPropertyFlag, EMemoryPropertyFlags)

// Enumeration of shader stages
enum class EShaderStage : TUINT8
{
	Vertex,
	Pixel,
	Geometry,
	Mesh,
	Task,

	Compute,

	RayGeneration,
	RayIntersection,
	RayAnyHit,
	RayClosestHit,
	RayMiss,
	RayCallable
};

namespace RHI
{
	constexpr TUINT32 NUM_GFX_SHADER_STAGES = 5;
	constexpr TUINT32 MAX_SHADER_STAGES = 6;
	constexpr TUINT32 MAX_VERTEX_ELEMENTS = 16; // Vertex attributes, bindings
} // namespace RHI

// Query Types
enum class ERHIQueryType : TUINT8
{
	None = 0,
	Occlusion,
	Timestamp,
	PipelineStatistics,
	// Add other query types as needed
};

// Enumeration of texture filter modes
enum class ETextureFilter : TUINT8
{
	None = 0,
	Point,
	Linear,
	Anisotropic
};

// Enumeration of texture address modes
enum class ETextureAddressMode : TUINT8
{
	None = 0,
	Wrap,
	Clamp,
	Mirror,
	Border
};

// Enumeration of primitive topologies
enum class EPrimitiveTopology : TUINT8
{
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

HLVM_ENUM(ESamplerFilter, TUINT8,
	Point,
	Bilinear,
	Trilinear,
	AnisotropicPoint,
	AnisotropicLinear);
static_assert(ESamplerFilter_NUM <= (1 << 3), "ESamplerFilter will not fit on 3 bits");

HLVM_ENUM(ESamplerAddressMode, TUINT8,
	Wrap,
	Clamp,
	Mirror,
	/** Not supported on all platforms */
	Border);
static_assert(ESamplerAddressMode_NUM <= (1 << 2), "ESamplerAddressMode will not fit on 2 bits");

enum class ESamplerCompareFunction : TUINT8
{
	Never,
	Less
};

HLVM_ENUM(ERasterizerFillMode, TUINT8,
	Point,
	Wireframe,
	Solid);
static_assert(ERasterizerFillMode_NUM <= (1 << 2), "ERasterizerFillMode will not fit on 2 bits");

HLVM_ENUM(ERasterizerCullMode, TUINT8,
	None,
	CW,
	CCW);
static_assert(ERasterizerCullMode_NUM <= (1 << 2), "ERasterizerCullMode will not fit on 2 bits");

HLVM_ENUM(ERasterizerDepthClipMode, TUINT8,
	DepthClip,
	DepthClamp);
static_assert(ERasterizerDepthClipMode_NUM <= (1 << 1), "ERasterizerDepthClipMode will not fit on 1 bits");

enum class EColorWriteMask : TUINT32
{
	RED = 1 << 0,
	GREEN = 1 << 1,
	BLUE = 1 << 2,
	ALPHA = 1 << 3,

	NUM,

	NONE = 0,
	RGB = RED | GREEN | BLUE,
	RGBA = RED | GREEN | BLUE | ALPHA,
	RG = RED | GREEN,
	BA = BLUE | ALPHA,
};
static_assert(HLVM_ENUM_VALUE(EColorWriteMask::NUM) <= (1 << 4), "EColorWriteMask will not fit on 4 bits");

enum class ECompareFunction : TUINT32
{
	Less = 0,
	LessEqual = 1,
	Greater = 2,
	GreaterEqual = 3,
	Equal = 4,
	NotEqual = 5,
	Never = 6,
	Always = 7,

	NUM,

	// Utility enumerations
	DepthNearOrEqual = ((HLVM_ENUM_VALUE(ERHIZBuffer::IsInverted) != 0) ? GreaterEqual : LessEqual),
	DepthNear = ((HLVM_ENUM_VALUE(ERHIZBuffer::IsInverted) != 0) ? Greater : Less),
	DepthFartherOrEqual = ((HLVM_ENUM_VALUE(ERHIZBuffer::IsInverted) != 0) ? LessEqual : GreaterEqual),
	DepthFarther = ((HLVM_ENUM_VALUE(ERHIZBuffer::IsInverted) != 0) ? Less : Greater)
};
static_assert(HLVM_ENUM_VALUE(ECompareFunction::NUM) <= (1 << 3), "ECompareFunction will not fit on 3 bits");

enum class EStencilMask : TUINT8
{
	Default = 0,
	V_255 = 255,
	V_1 = 1,
	V_2 = 2,
	V_4 = 4,
	V_8 = 8,
	V_16 = 16,
	V_32 = 32,
	V_64 = 64,
	V_128 = 128
};

HLVM_ENUM(EStencilOp, TUINT8,
	Keep,
	Zero,
	Replace,
	SaturatedIncrement,
	SaturatedDecrement,
	Invert,
	Increment,
	Decrement);
static_assert(EStencilOp_NUM <= (1 << 3), "EStencilOp will not fit on 3 bits");

HLVM_ENUM(EBlendOperation, TUINT8,
	Add,
	Subtract,
	Min,
	Max,
	ReverseSubtract);
static_assert(EBlendOperation_NUM <= (1 << 3), "EBlendOperation will not fit on 3 bits");

HLVM_ENUM(EBlendFactor, TUINT8,
	Zero,
	One,
	SourceColor,
	InverseSourceColor,
	SourceAlpha,
	InverseSourceAlpha,
	DestAlpha,
	InverseDestAlpha,
	DestColor,
	InverseDestColor,
	ConstantBlendFactor,
	InverseConstantBlendFactor,
	Source1Color,
	InverseSource1Color,
	Source1Alpha,
	InverseSource1Alpha);
static_assert(EBlendFactor_NUM <= (1 << 4), "EBlendFactor will not fit on 4 bits");

// Enumeration of vertex element types
HLVM_ENUM(EVertexElementType, TUINT8,
	None,
	Float1,
	Float2,
	Float3,
	Float4,
	PackedNormal,   // FPackedNormal
	UByte4,
	UByte4N,
	Color,
	Short2,
	Short4,
	Short2N,       // 16 bit word normalized to (value/32767.0,value/32767.0,0,0,1)
	Half2,        // 16 bit float using 1 bit sign, 5 bit exponent, 10 bit mantissa
	Half4,
	Short4N,       // 4 X 16 bit word, normalized
	UShort2,
	UShort4,
	UShort2N,      // 16 bit word normalized to (value/65535.0,value/65535.0,0,0,1)
	UShort4N,      // 4 X 16 bit word unsigned, normalized
	URGB10A2N,     // 10 bit r, g, b and 2 bit a normalized to (value/1023.0f, value/1023.0f, value/1023.0f, value/3.0f)
	UInt
);
static_assert(EVertexElementType_NUM <= (1 << 5), "EVertexElementType will not fit on 5 bits");

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
	Stereo = 1 << 1,	   // Stereo rendering (e.g., VR)
	HDR = 1 << 2,		   // High Dynamic Range (HDR) support
};

enum class ESubpassHint : TUINT32
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

enum class ERenderTargetActions : TUINT32
{
	LoadOpMask = 2,
#define RTACTION_MAKE_MASK(LoadAction, StoreAction) ((HLVM_ENUM_VALUE(ERenderTargetLoadAction::LoadAction) << LoadOpMask) | HLVM_ENUM_VALUE(ERenderTargetStoreAction::StoreAction))
	DontLoad_DontStore = RTACTION_MAKE_MASK(DontCare, DontCare),
	DontLoad_Store = RTACTION_MAKE_MASK(DontCare, Store),
	Clear_Store = RTACTION_MAKE_MASK(Clear, Store),
	Load_Store = RTACTION_MAKE_MASK(Load, Store),
	Clear_DontStore = RTACTION_MAKE_MASK(Clear, DontCare),
	Load_DontStore = RTACTION_MAKE_MASK(Load, DontCare),
	Clear_Resolve = RTACTION_MAKE_MASK(Clear, MultisampleResolve),
	Load_Resolve = RTACTION_MAKE_MASK(Load, MultisampleResolve),
#undef RTACTION_MAKE_MASK
};

namespace RHI
{
	HLVM_INLINE_FUNC ERenderTargetActions MakeRenderTargetActions(ERenderTargetLoadAction Load, ERenderTargetStoreAction Store)
	{
		return S_C(ERenderTargetActions, (HLVM_ENUM_VALUE(Load) << HLVM_ENUM_VALUE(ERenderTargetActions::LoadOpMask)) | HLVM_ENUM_VALUE(Store));
	}

	HLVM_INLINE_FUNC ERenderTargetLoadAction GetLoadAction(ERenderTargetActions Action)
	{
		return S_C(ERenderTargetLoadAction, HLVM_ENUM_VALUE(Action) >> HLVM_ENUM_VALUE(ERenderTargetActions::LoadOpMask));
	}

	HLVM_INLINE_FUNC ERenderTargetStoreAction GetStoreAction(ERenderTargetActions Action)
	{
		return S_C(ERenderTargetStoreAction, HLVM_ENUM_VALUE(Action) & ((1 << HLVM_ENUM_VALUE(ERenderTargetActions::LoadOpMask)) - 1));
	}
} // namespace RHI

enum class EDepthStencilTargetActions : TUINT32
{
	DepthMask = 4,
#define RTACTION_MAKE_MASK(DepthAction, StencilAction) ((HLVM_ENUM_VALUE(ERenderTargetActions::DepthAction) << DepthMask) | HLVM_ENUM_VALUE(ERenderTargetActions::StencilAction))
	DontLoad_DontStore = RTACTION_MAKE_MASK(DontLoad_DontStore, DontLoad_DontStore),
	DontLoad_StoreDepthStencil = RTACTION_MAKE_MASK(DontLoad_Store, DontLoad_Store),
	DontLoad_StoreStencilNotDepth = RTACTION_MAKE_MASK(DontLoad_DontStore, DontLoad_Store),
	ClearDepthStencil_StoreDepthStencil = RTACTION_MAKE_MASK(Clear_Store, Clear_Store),
	LoadDepthStencil_StoreDepthStencil = RTACTION_MAKE_MASK(Load_Store, Load_Store),
	LoadDepthNotStencil_StoreDepthNotStencil = RTACTION_MAKE_MASK(Load_Store, DontLoad_DontStore),
	LoadDepthNotStencil_DontStore = RTACTION_MAKE_MASK(Load_DontStore, DontLoad_DontStore),
	LoadDepthStencil_StoreStencilNotDepth = RTACTION_MAKE_MASK(Load_DontStore, Load_Store),
	ClearDepthStencil_DontStoreDepthStencil = RTACTION_MAKE_MASK(Clear_DontStore, Clear_DontStore),
	LoadDepthStencil_DontStoreDepthStencil = RTACTION_MAKE_MASK(Load_DontStore, Load_DontStore),
	ClearDepthStencil_StoreDepthNotStencil = RTACTION_MAKE_MASK(Clear_Store, Clear_DontStore),
	ClearDepthStencil_StoreStencilNotDepth = RTACTION_MAKE_MASK(Clear_DontStore, Clear_Store),
	ClearDepthStencil_ResolveDepthNotStencil = RTACTION_MAKE_MASK(Clear_Resolve, Clear_DontStore),
	ClearDepthStencil_ResolveStencilNotDepth = RTACTION_MAKE_MASK(Clear_DontStore, Clear_Resolve),
	LoadDepthClearStencil_StoreDepthStencil = RTACTION_MAKE_MASK(Load_Store, Clear_Store),
	ClearStencilDontLoadDepth_StoreStencilNotDepth = RTACTION_MAKE_MASK(DontLoad_DontStore, Clear_Store),
#undef RTACTION_MAKE_MASK
};

namespace RHI
{
	HLVM_INLINE_FUNC EDepthStencilTargetActions MakeDepthStencilTargetActions(ERenderTargetLoadAction Depth, ERenderTargetStoreAction Stencil)
	{
		return S_C(EDepthStencilTargetActions, (HLVM_ENUM_VALUE(Depth) << HLVM_ENUM_VALUE(EDepthStencilTargetActions::DepthMask)) | HLVM_ENUM_VALUE(Stencil));
	}

	HLVM_INLINE_FUNC ERenderTargetActions GetDepthActions(EDepthStencilTargetActions Action)
	{
		return S_C(ERenderTargetActions, HLVM_ENUM_VALUE(Action) >> HLVM_ENUM_VALUE(EDepthStencilTargetActions::DepthMask));
	}

	HLVM_INLINE_FUNC ERenderTargetActions GetStencilActions(EDepthStencilTargetActions Action)
	{
		return S_C(ERenderTargetActions, HLVM_ENUM_VALUE(Action) & ((1 << HLVM_ENUM_VALUE(EDepthStencilTargetActions::DepthMask)) - 1));
	}
} // namespace RHI

enum class ERHIAccessFlag : TUINT32
{
	// Used when the previous state of a resource is not known,
	// which implies we have to flush all GPU caches etc.
	None = 0,

	// Read states
	CPURead = 1 << 0,
	Present = 1 << 1,
	IndirectArgs = 1 << 2,
	VertexOrIndexBuffer = 1 << 3,
	SRVCompute = 1 << 4,
	SRVGraphicsPixel = 1 << 5,
	SRVGraphicsNonPixel = 1 << 6,
	CopySrc = 1 << 7,
	ResolveSrc = 1 << 8,
	DSVRead = 1 << 9,

	// Read-write states
	UAVCompute = 1 << 10,
	UAVGraphics = 1 << 11,
	RTV = 1 << 12,
	CopyDest = 1 << 13,
	ResolveDst = 1 << 14,
	DSVWrite = 1 << 15,

	// Ray tracing acceleration structure states.
	// Buffer that contains an AS must always be in either of these states.
	// BVHRead -- required for AS inputs to build/update/copy/trace commands.
	// BVHWrite -- required for AS outputs of build/update/copy commands.
	BVHRead = 1 << 16,
	BVHWrite = 1 << 17,

	// Invalid released state (transient resources)
	Discard = 1 << 18,

	// Shading Rate Source
	ShadingRateSource = 1 << 19,

	Last = ShadingRateSource,
	Unknown = None,
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

enum class EPrimitiveType : TUINT8
{
	// Topology that defines a triangle N with 3 vertex extremities: 3*N+0, 3*N+1, 3*N+2.
	TriangleList,

	// Topology that defines a triangle N with 3 vertex extremities: N+0, N+1, N+2.
	TriangleStrip,

	// Topology that defines a line with 2 vertex extremities: 2*N+0, 2*N+1.
	LineList,

	Num
};
static_assert(HLVM_ENUM_VALUE(EPrimitiveType::Num) <= (1 << 3), "EPrimitiveType doesn't fit in 3 bits");

