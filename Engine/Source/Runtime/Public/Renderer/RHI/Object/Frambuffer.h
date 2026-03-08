/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 *
 *  Framebuffer Objects
 *
 * Framebuffer management using .
 * Borrowed from RBDOOM-3-BFG's Framebuffer_ with HLVM coding style.
 */

#pragma once

#include "Renderer/RHI/RHICommon.h"
#include "Template/PointerTemplate.tpp"
#include "Renderer/RHI/Object/Texture.h"

/*-----------------------------------------------------------------------------
   Forward Declarations
-----------------------------------------------------------------------------*/

class FDeviceManager;

/*-----------------------------------------------------------------------------
   FFramebufferAttachment
-----------------------------------------------------------------------------*/

/**
 * Framebuffer attachment descriptor
 */
struct FFramebufferAttachment
{
	nvrhi::TextureHandle Texture;
	TUINT32				 MipLevel;
	TUINT32				 ArraySlice;
	TUINT32				 SampleCount;

	FFramebufferAttachment()
		: MipLevel(0)
		, ArraySlice(0)
		, SampleCount(1)
	{
	}

	FFramebufferAttachment(nvrhi::TextureHandle InTexture, TUINT32 InMipLevel = 0, TUINT32 InArraySlice = 0)
		: Texture(InTexture)
		, MipLevel(InMipLevel)
		, ArraySlice(InArraySlice)
		, SampleCount(1)
	{
	}
};

/*-----------------------------------------------------------------------------
   FFramebuffer - Main Framebuffer Class
-----------------------------------------------------------------------------*/

/**
 * Framebuffer wrapper for
 *
 * Manages:
 * -  framebuffer handle
 * - Color and depth attachments
 * - Viewport and scissor state
 *
 * Usage:
 * ```cpp
 * FFramebuffer Framebuffer;
 * Framebuffer.Initialize(Device);
 * Framebuffer.AddColorAttachment(ColorTexture);
 * Framebuffer.AddDepthAttachment(DepthTexture);
 * Framebuffer.CreateFramebuffer();
 *
 * // In render loop:
 * CommandList->open();
 * CommandList->beginRenderPass(PassDesc, Framebuffer.GetFramebufferHandle());
 * // ... render commands ...
 * CommandList->endRenderPass();
 * CommandList->close();
 * ```
 */
class FFramebuffer
{
public:
	NOCOPYMOVE(FFramebuffer)

	FFramebuffer();
	virtual ~FFramebuffer();

	// Initialization
	bool Initialize(nvrhi::IDevice* InDevice);

	// Attachment management
	void AddColorAttachment(const FFramebufferAttachment& Attachment);
	void AddColorAttachment(nvrhi::TextureHandle Texture, TUINT32 MipLevel = 0, TUINT32 ArraySlice = 0);
	void SetDepthAttachment(const FFramebufferAttachment& Attachment);
	void SetDepthAttachment(nvrhi::TextureHandle Texture, TUINT32 MipLevel = 0, TUINT32 ArraySlice = 0);

	// Create framebuffer (call after adding attachments)
	bool CreateFramebuffer();

	// Accessors
	[[nodiscard]] nvrhi::FramebufferHandle GetFramebufferHandle() const { return FramebufferHandle; }
	[[nodiscard]] TUINT32				   GetWidth() const { return Width; }
	[[nodiscard]] TUINT32				   GetHeight() const { return Height; }
	[[nodiscard]] TUINT32				   GetColorAttachmentCount() const { return ColorAttachments.Num32(); }

	// Viewport
	void						  SetViewport(TFLOAT X, TFLOAT Y, TFLOAT Width, TFLOAT Height, TFLOAT MinDepth = 0.0f, TFLOAT MaxDepth = 1.0f);
	[[nodiscard]] nvrhi::Viewport GetViewport() const { return Viewport; }

	// Scissor
	void					  SetScissor(TINT32 X, TINT32 Y, TUINT32 Width, TUINT32 Height);
	[[nodiscard]] nvrhi::Rect GetScissor() const { return Scissor; }

	// Debug name
	void SetDebugName(const TCHAR* Name);

protected:
	nvrhi::FramebufferHandle FramebufferHandle;
	TNNPtr<nvrhi::IDevice>	 Device;

	TVector<FFramebufferAttachment> ColorAttachments;
	FFramebufferAttachment			DepthAttachment;

	TUINT32 Width;
	TUINT32 Height;

	nvrhi::Viewport Viewport;
	nvrhi::Rect		Scissor;

	TCharArray<64> DebugName;
};

/*-----------------------------------------------------------------------------
   FFramebufferManager - Framebuffer Pool Management
-----------------------------------------------------------------------------*/

/**
 * Manages multiple framebuffers for different rendering passes
 *
 * Usage:
 * ```cpp
 * FFramebufferManager FBManager;
 * FBManager.Initialize(Device);
 *
 * // Create GBuffer framebuffer
 * auto* GBufferFB = FBManager.CreateFramebuffer("GBuffer");
 * GBufferFB->AddColorAttachment(AlbedoTexture);
 * GBufferFB->AddColorAttachment(NormalTexture);
 * GBufferFB->AddDepthAttachment(DepthTexture);
 * GBufferFB->CreateFramebuffer();
 *
 * // Create post-process framebuffer
 * auto* PostFB = FBManager.CreateFramebuffer("PostProcess");
 * PostFB->AddColorAttachment(OutputTexture);
 * PostFB->CreateFramebuffer();
 * ```
 */
class FFramebufferManager
{
public:
	NOCOPYMOVE(FFramebufferManager)

	FFramebufferManager() = default;
	~FFramebufferManager();

	// Initialize manager
	void Initialize(nvrhi::IDevice* InDevice);

	// Create framebuffer
	FFramebuffer* CreateFramebuffer(const TCHAR* Name);

	// Find framebuffer by name
	FFramebuffer* FindFramebuffer(const TCHAR* Name);

	// Remove framebuffer
	void RemoveFramebuffer(const TCHAR* Name);

	// Remove all framebuffers
	void RemoveAllFramebuffers();

	// Access device
	[[nodiscard]] nvrhi::IDevice* GetDevice() const { return Device; }

protected:
	TNNPtr<nvrhi::IDevice>					Device;
	TMap<FString, TUniquePtr<FFramebuffer>> FramebufferPool;
};

/*-----------------------------------------------------------------------------
   Inline Implementations
-----------------------------------------------------------------------------*/

HLVM_INLINE_FUNC FFramebuffer::FFramebuffer()
	: Width(0), Height(0)
{
}

HLVM_INLINE_FUNC FFramebuffer::~FFramebuffer()
{
	FramebufferHandle.Reset();
}

HLVM_INLINE_FUNC void FFramebuffer::AddColorAttachment(nvrhi::TextureHandle Texture, TUINT32 MipLevel, TUINT32 ArraySlice)
{
	AddColorAttachment(FFramebufferAttachment(Texture, MipLevel, ArraySlice));
}

HLVM_INLINE_FUNC void FFramebuffer::SetDepthAttachment(nvrhi::TextureHandle Texture, TUINT32 MipLevel, TUINT32 ArraySlice)
{
	SetDepthAttachment(FFramebufferAttachment(Texture, MipLevel, ArraySlice));
}
