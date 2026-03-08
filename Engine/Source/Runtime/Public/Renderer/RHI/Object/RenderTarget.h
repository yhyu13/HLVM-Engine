/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 *
 *  Render Target Objects
 *
 *  Color and depth render target management using NVRHI.
 */

#pragma once

#include "Renderer/RHI/RHICommon.h"
#include "Template/PointerTemplate.tpp"
#include "Renderer/RHI/Object/Texture.h"

/*-----------------------------------------------------------------------------
	FRenderTarget - Color Render Target
-----------------------------------------------------------------------------*/

/**
 * Color render target for off-screen rendering
 *
 * Manages:
 * - Texture creation with render target flags
 * - Clear operations
 * - Resolve operations (MSAA)
 * - State transitions
 *
 * Usage:
 * ```cpp
 * FRenderTarget RenderTarget;
 * RenderTarget.Initialize(Device, 1920, 1080, ETextureFormat::RGBA8);
 *
 * // In render pass
 * CommandList->open();
 * RenderTarget.Clear(CommandList, TFloatColor(0.0f, 0.0f, 0.0f, 1.0f));
 * // ... render commands ...
 * CommandList->close();
 * ```
 */
class FRenderTarget
{
public:
	NOCOPYMOVE(FRenderTarget)

	FRenderTarget();
	virtual ~FRenderTarget();

	bool Initialize(
		nvrhi::IDevice* Device,
		uint32_t		Width,
		uint32_t		Height,
		ETextureFormat	Format = ETextureFormat::RGBA8_UNORM,
		uint32_t		SampleCount = 1,
		bool			bAllowShared = false);

	void Clear(
		nvrhi::ICommandList* CommandList,
		const nvrhi::Color&	 Color);

	void Resolve(
		nvrhi::ICommandList* CommandList,
		FRenderTarget*		 DestTarget);

	[[nodiscard]] nvrhi::TextureHandle GetTextureHandle() const { return TextureHandle; }
	[[nodiscard]] uint32_t			   GetWidth() const { return Width; }
	[[nodiscard]] uint32_t			   GetHeight() const { return Height; }
	[[nodiscard]] ETextureFormat	   GetFormat() const { return Format; }
	[[nodiscard]] uint32_t			   GetSampleCount() const { return SampleCount; }

	void SetDebugName(const TCHAR* Name);

protected:
	nvrhi::TextureHandle   TextureHandle;
	TNNPtr<nvrhi::IDevice> Device;
	uint32_t			   Width;
	uint32_t			   Height;
	ETextureFormat		   Format;
	uint32_t			   SampleCount;
	bool				   bAllowShared;
	TCharArray<64>		   DebugName;
};

/*-----------------------------------------------------------------------------
	FDepthTarget - Depth/Stencil Target
-----------------------------------------------------------------------------*/

/**
 * Depth/stencil target for depth testing and stencil operations
 *
 * Manages:
 * - Depth/stencil texture creation
 * - Clear operations
 * - Depth state management
 *
 * Usage:
 * ```cpp
 * FDepthTarget DepthTarget;
 * DepthTarget.Initialize(Device, 1920, 1080, ETextureFormat::D32);
 *
 * // In render pass
 * CommandList->open();
 * DepthTarget.Clear(CommandList, 1.0f, 0);
 * // ... render commands ...
 * CommandList->close();
 * ```
 */
class FDepthTarget
{
public:
	NOCOPYMOVE(FDepthTarget)

	FDepthTarget();
	virtual ~FDepthTarget();

	bool Initialize(
		nvrhi::IDevice* Device,
		uint32_t		Width,
		uint32_t		Height,
		ETextureFormat	Format = ETextureFormat::D32,
		bool			bHasStencil = false);

	void Clear(
		nvrhi::ICommandList* CommandList,
		float				 Depth = 1.0f,
		uint8_t				 Stencil = 0);

	[[nodiscard]] nvrhi::TextureHandle GetTextureHandle() const { return TextureHandle; }
	[[nodiscard]] uint32_t			   GetWidth() const { return Width; }
	[[nodiscard]] uint32_t			   GetHeight() const { return Height; }
	[[nodiscard]] bool				   HasStencil() const { return bHasStencil; }

	void SetDebugName(const TCHAR* Name);

protected:
	nvrhi::TextureHandle   TextureHandle;
	TNNPtr<nvrhi::IDevice> Device;
	ETextureFormat		   Format;
	uint32_t			   Width;
	uint32_t			   Height;
	bool				   bHasStencil;
	TCharArray<64>		   DebugName;
};
