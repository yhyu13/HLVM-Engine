/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "Core/Object/RefCountPtr.h"
#include "RHIDefinition.h"
#include "RHIResourcePre.h"

// Enumeration of RHI resource types
enum class ERHIResourceType : TUINT8
{
	Texture,
	Buffer,
	Shader,
	ShaderResourceView,
	UnorderedAccessView,
	SamplerState,
	GraphicsPSO,
	ComputePSO,
	Query,
	VertexDeclaration,
	Viewport,
	RenderPass,
	BlendState,
	RasterizerState,
	DepthStencilState,
	// Add other resource types as needed
};

// Base class for all RHI resources
class IRHIResource : public FRefCountable
{
public:
	template <typename T>
	struct IRHIHandle
	{
		static_assert(std::is_pointer_v<T>);

		IRHIHandle() = default;
		IRHIHandle(IRHIResource* InOwnerClass)
			: OwnerClass(InOwnerClass)
		{
		}
		IRHIHandle(IRHIResource* InOwnerClass, const IRHIHandle& InHandle)
			: Handle(InHandle.Handle), OwnerClass(InOwnerClass)
		{
		}
		explicit IRHIHandle(T InHandle)
			: Handle(InHandle)
		{
		}

		// assingment operator
		IRHIHandle& operator=(const IRHIHandle& InHandle)
		{
			Handle = InHandle.Handle;
			if (OwnerClass)
			{
				OwnerClass->UpdateHandle(Handle);
			}
			return *this;
		}
		IRHIHandle& operator=(T& InHandle)
		{
			Handle = InHandle;
			if (OwnerClass)
			{
				OwnerClass->UpdateHandle(Handle);
			}
			return *this;
		}
		IRHIHandle& operator=(IRHIHandle&& InHandle)
		{
			Handle = InHandle.Handle;
			if (OwnerClass)
			{
				OwnerClass->UpdateHandle(Handle);
			}
			return *this;
		}
		IRHIHandle& operator=(T&& InHandle)
		{
			Handle = InHandle;
			if (OwnerClass)
			{
				OwnerClass->UpdateHandle(Handle);
			}
			return *this;
		}

		// compare operators
		bool operator==(const IRHIHandle& InHandle) const
		{
			return Handle == InHandle.Handle;
		}
		bool operator!=(const IRHIHandle& InHandle) const
		{
			return Handle != InHandle.Handle;
		}
		bool operator==(std::nullptr_t) const
		{
			return Handle == nullptr;
		}
		bool operator!=(std::nullptr_t) const
		{
			return Handle != nullptr;
		}

		operator T() const
		{
			return Handle;
		}

		T			  Handle{ nullptr };
		IRHIResource* OwnerClass{ nullptr };
	};

public:
	IRHIResource() = default;
	virtual ~IRHIResource() = default;

	// Returns the type of the RHI resource
	virtual ERHIResourceType GetResourceType() const = 0;

	// Returns the RHI interface type (e.g., Vulkan, DirectX)
	virtual ERHIInterfaceType GetInterfaceType() const = 0;

	FString ToString() const
	{
		return FString::Format(TXT("name = {}, handle = 0x{:x}"), *GetDebugName(), HandlePtr);
	}

	// Returns the name of the resource for debugging
	FString GetDebugName() const
	{
		return FString::Format(TXT("[{}:\"{}\"]"),
			HLVM_ENUM_TO_TCHAR(GetInterfaceType()), *CreateInfoPtr->DebugName);
	}

protected:
	// Updates the create info struct using CreateInfoPtr
	template <typename T>
	void UpdateCreateInfo(const T& InCreateInfo)
	{
		*D_C(T*, CreateInfoPtr.Get()) = InCreateInfo;
	}

	template <typename T>
	void UpdateHandle(T InHandle)
	{
		static_assert(std::is_pointer_v<T>);
		static_assert(sizeof(T) == sizeof(TSIZE));
		HandlePtr = R_C(TSIZE, InHandle);
	}

protected:
	TNoNullablePtr<IRHICreateInfo> CreateInfoPtr;
	TSIZE						   HandlePtr;
};

#define DECLARE_RHI_RESOURCE(ClassName, CreateteInfoType) \
	ClassName(const CreateteInfoType& InCreateInfo)       \
	{                                                     \
		CreateInfoPtr = &CreateInfo;                      \
		UpdateCreateInfo(InCreateInfo);                   \
	}                                                     \
	const CreateteInfoType& GetCreateInfo() const         \
	{                                                     \
		return CreateInfo;                                \
	}

// Base class for RHI textures
class FRHITexture : virtual public IRHIResource
{
public:
	DECLARE_RHI_RESOURCE(FRHITexture, FRHITextureCreateInfo)

	// Returns the extent of the texture
	FUInt3 GetExtent() const { return CreateInfo.Extent; }

	// Return dimension of the texture
	ETextureDimension GetDimension() const { return CreateInfo.Dimension; }

	// Return Array size
	TUINT8 GetArraySize() const { return CreateInfo.ArraySize; }

	// Returns the pixel format of the texture
	EPixelFormat GetFormat() const { return CreateInfo.Format; }

	// Returns the texture flags
	ETextureCreateFlags GetCreateFlags() const { return CreateInfo.Flags; }

	bool IsSRGB() const { return CreateInfo.Flags & ETextureCreateFlag::SRGB; }

	// Returns whether the texture is multisampled
	bool IsMultiSampled() const { return CreateInfo.NumSamples > 1; }

	// Returns the number of samples in the texture
	TUINT8 GetNumSamples() const { return CreateInfo.NumSamples; }

	TUINT8 GetNumMips() const { return CreateInfo.NumMips; }

	// Returns the type of the RHI resource
	virtual ERHIResourceType GetResourceType() const override { return ERHIResourceType::Texture; }

protected:
	FRHITextureCreateInfo CreateInfo; // Declaration struct as a member
};
using FRHITextureRef = TRefCountPtr<FRHITexture>;

// Base class for RHI buffers
class FRHIBuffer : virtual public IRHIResource
{
public:
	DECLARE_RHI_RESOURCE(FRHIBuffer, FRHIBufferCreateInfo)

	// Returns the size of the buffer in bytes
	virtual TSIZE GetSize() const { return CreateInfo.Size; }

	// Returns the usage flags of the buffer
	virtual EBufferUsageFlags GetUsageFlags() const { return CreateInfo.UsageFlags; }

	// Returns the memory flags of the buffer
	virtual EMemoryPropertyFlags GetMemoryFlags() const { return CreateInfo.MemoryPropertyFlags; }

	// Returns the type of the RHI resource
	virtual ERHIResourceType GetResourceType() const override { return ERHIResourceType::Buffer; }

protected:
	FRHIBufferCreateInfo CreateInfo; // Declaration struct as a member
};

// Base class for RHI shaders
class FRHIShader : virtual public IRHIResource
{
public:
	DECLARE_RHI_RESOURCE(FRHIShader, FShaderCreateInfo)

	// Returns the shader stage (e.g., vertex, pixel, compute)
	virtual EShaderStage GetStage() const { return CreateInfo.Stage; }

	// Returns the type of the RHI resource
	virtual ERHIResourceType GetResourceType() const override { return ERHIResourceType::Shader; }

protected:
	FShaderCreateInfo CreateInfo; // Declaration struct as a member
};

// Base class for RHI shader resource views
class FRHIShaderResourceView : virtual public IRHIResource
{
public:
	DECLARE_RHI_RESOURCE(FRHIShaderResourceView, FRHIShaderResourceViewCreateInfo)

	FRHIShaderResourceViewCreateInfo CreateInfo; // Declaration struct as a member

	// Returns the type of the RHI resource
	virtual ERHIResourceType GetResourceType() const override { return ERHIResourceType::ShaderResourceView; }
};

// Base class for RHI unordered access views
class FRHIUnorderedAccessView : virtual public IRHIResource
{
public:
	DECLARE_RHI_RESOURCE(FRHIUnorderedAccessView, FRHIUnorderedAccessViewCreateInfo)

	FRHIUnorderedAccessViewCreateInfo CreateInfo; // Declaration struct as a member

	// Returns the type of the RHI resource
	virtual ERHIResourceType GetResourceType() const override { return ERHIResourceType::UnorderedAccessView; }
};

// Base class for RHI sampler states
class FRHISamplerState : virtual public IRHIResource
{
public:
	DECLARE_RHI_RESOURCE(FRHISamplerState, FRHISamplerStateCreateInfo)

	// Returns the type of the RHI resource
	virtual ERHIResourceType GetResourceType() const override { return ERHIResourceType::SamplerState; }

protected:
	FRHISamplerStateCreateInfo CreateInfo;
};

// Base class for RHI pipeline states object (PSO)
class FRHIGraphicsPSO : virtual public IRHIResource
{
public:
	// Returns the type of the RHI resource
	virtual ERHIResourceType GetResourceType() const override { return ERHIResourceType::GraphicsPSO; }
};

class FRHIComputePSO : virtual public IRHIResource
{
public:
	// Returns the type of the RHI resource
	virtual ERHIResourceType GetResourceType() const override { return ERHIResourceType::ComputePSO; }
};

// Base class for RHI queries
class FRHIQuery : virtual public IRHIResource
{
public:
	// Returns the type of the query (e.g., occlusion, timestamp)
	virtual ERHIQueryType GetQueryType() const = 0;

	// Returns the type of the RHI resource
	virtual ERHIResourceType GetResourceType() const override { return ERHIResourceType::Query; }
};

// Base class for RHI viewports
class FRHIViewport : virtual public IRHIResource
{
public:
	DECLARE_RHI_RESOURCE(FRHIViewport, FRHIViewportCreateInfo)

	// Returns the type of the RHI resource
	virtual ERHIResourceType GetResourceType() const override { return ERHIResourceType::Viewport; }

	// Returns the dimensions of the viewport
	virtual FUInt2 GetSize() const { return CreateInfo.Extent; }

	// Returns the viewport type (e.g., windowed, fullscreen)
	virtual ERHIViewportType GetViewportType() const { return CreateInfo.ViewportType; }

	// Returns the associated swap chain (if any)
	virtual void* GetSwapChain() const = 0;

	// Resizes the viewport and swap chain
	virtual void Resize(const FUInt2& NewExtent) = 0;

	// Begins a frame, acquire next back buffer
	virtual void BeginFrame() = 0;

	// Presents the viewport, swaps the back buffer
	virtual void Present() = 0;

	// Returns the RHI texture associated with the back buffer
	virtual FRHITextureRef GetBackBuffer() const = 0;

protected:
	FRHIViewportCreateInfo CreateInfo; // Viewport creation description
};

class FRHIRenderPass : virtual public IRHIResource
{
public:
	// Returns the type of the RHI resource
	virtual ERHIResourceType GetResourceType() const override { return ERHIResourceType::RenderPass; }
};

class FRHIBlendState : virtual public IRHIResource
{
public:
	DECLARE_RHI_RESOURCE(FRHIBlendState, FRHIBlendStateCreateInfo)

	// Returns the type of the RHI resource
	virtual ERHIResourceType GetResourceType() const override { return ERHIResourceType::BlendState; }

protected:
	FRHIBlendStateCreateInfo CreateInfo;
};

class FRHIRasterizerState : virtual public IRHIResource
{
public:
	DECLARE_RHI_RESOURCE(FRHIRasterizerState, FRHIRasterizerStateCreateInfo)

	// Returns the type of the RHI resource
	virtual ERHIResourceType GetResourceType() const override { return ERHIResourceType::RasterizerState; }

protected:
	FRHIRasterizerStateCreateInfo CreateInfo;
};

class FRHIDepthStencilState : virtual public IRHIResource
{
public:
	DECLARE_RHI_RESOURCE(FRHIDepthStencilState, FRHIDepthStencilStateCreateInfo)

	// Returns the type of the RHI resource
	virtual ERHIResourceType GetResourceType() const override { return ERHIResourceType::DepthStencilState; }

protected:
	FRHIDepthStencilStateCreateInfo CreateInfo;
};

// Smart pointer types for RHI resources
using FRHIBufferRef = TRefCountPtr<FRHIBuffer>;
using FRHIShaderRef = TRefCountPtr<FRHIShader>;
using FShaderResourceViewRHIRef = TRefCountPtr<FRHIShaderResourceView>;
using FUnorderedAccessViewRHIRef = TRefCountPtr<FRHIUnorderedAccessView>;
using FRHISamplerStateRef = TRefCountPtr<FRHISamplerState>;
using FRHIGraphicsPSORef = TRefCountPtr<FRHIGraphicsPSO>;
using FRHIComputePSORef = TRefCountPtr<FRHIComputePSO>;
using FQueryRHIRef = TRefCountPtr<FRHIQuery>;
using FRHIViewportRef = TRefCountPtr<FRHIViewport>;
using FRHIRenderPassRef = TRefCountPtr<FRHIRenderPass>;
using FRHIBlendStateRef = TRefCountPtr<FRHIBlendState>;
using FRHIRasterizerStateRef = TRefCountPtr<FRHIRasterizerState>;
using FRHIDepthStencilStateRef = TRefCountPtr<FRHIDepthStencilState>;

#undef DECLARE_RHI_RESOURCE
