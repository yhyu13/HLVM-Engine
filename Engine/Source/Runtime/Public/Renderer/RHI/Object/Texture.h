/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 *
 *  Texture Objects
 *
 * Texture/Image management using .
 * Borrowed from RBDOOM-3-BFG's Image_ with HLVM coding style.
 */

#pragma once

#include "Renderer/RHI/RHICommon.h"
#include "Template/PointerTemplate.tpp"

/*-----------------------------------------------------------------------------
   FTexture - Main Texture Class
-----------------------------------------------------------------------------*/

/**
 * Texture resource for  rendering
 *
 * Manages:
 * -  texture handle and views
 * - Sampler state
 * - Mipmap generation
 * - Texture uploads
 *
 * Usage:
 * 1. Create instance
 * 2. Call Initialize() or InitializeRenderTarget()
 * 3. Use GetTextureHandle() for shader bindings
 * 4. Use GetSampler() for sampler bindings
 */
class FTexture
{
public:
	NOCOPYMOVE(FTexture)

	FTexture();
	virtual ~FTexture();

	// Initialization
	bool Initialize(
		TUINT32				 Width,
		TUINT32				 Height,
		TUINT32				 MipLevels,
		ETextureFormat		 Format,
		ETextureDimension	 Dimension,
		nvrhi::IDevice*		 Device,
		const void*			 InitialData = nullptr,
		nvrhi::ICommandList* CommandList = nullptr);

	// Render target initialization
	bool InitializeRenderTarget(
		TUINT32			Width,
		TUINT32			Height,
		ETextureFormat	Format,
		nvrhi::IDevice* Device,
		TUINT32			InSampleCount = 1);

	// Resource access
	[[nodiscard]] nvrhi::TextureHandle GetTextureHandle() const { return TextureHandle; }
	[[nodiscard]] nvrhi::TextureHandle GetTextureRTV() const { return TextureRTV; }
	[[nodiscard]] nvrhi::TextureHandle GetTextureDSV() const { return TextureDSV; }
	[[nodiscard]] nvrhi::TextureHandle GetTextureSRV() const { return TextureSRV; }
	[[nodiscard]] nvrhi::TextureHandle GetTextureUAV() const { return TextureUAV; }

	// Sampler access
	[[nodiscard]] nvrhi::SamplerHandle GetSampler(ETextureFilter Filter = ETextureFilter::Linear);

	// Properties
	[[nodiscard]] TUINT32			GetWidth() const { return Width; }
	[[nodiscard]] TUINT32			GetHeight() const { return Height; }
	[[nodiscard]] TUINT32			GetMipLevels() const { return MipLevels; }
	[[nodiscard]] ETextureFormat	GetFormat() const { return Format; }
	[[nodiscard]] ETextureDimension GetDimension() const { return Dimension; }

	// Texture upload
	void Update(
		const void*			 Data,
		TUINT32				 DataSize,
		TUINT32				 MipLevel,
		TUINT32				 ArraySlice,
		nvrhi::ICommandList* CommandList);

	// Generate mipmaps
	void GenerateMipmaps(nvrhi::ICommandList* CommandList);

	// Debug name
	void SetDebugName(const TCHAR* Name);

protected:
	nvrhi::TextureHandle TextureHandle;
	nvrhi::TextureHandle TextureRTV; // Render target view
	nvrhi::TextureHandle TextureDSV; // Depth stencil view
	nvrhi::TextureHandle TextureSRV; // Shader resource view
	nvrhi::TextureHandle TextureUAV; // Unordered access view

	TUINT32				   Width;
	TUINT32				   Height;
	TUINT32				   MipLevels;
	TUINT32				   ArraySize;
	TUINT32				   SampleCount;
	ETextureFormat		   Format;
	ETextureDimension	   Dimension;
	TNNPtr<nvrhi::IDevice> Device;
	TCharArray<64>		   DebugName;

	mutable TMapSmall<ETextureFilter, nvrhi::SamplerHandle> SamplerCache;
	void													CreateViews();
};

/*-----------------------------------------------------------------------------
   FSampler - Standalone Sampler
-----------------------------------------------------------------------------*/

/**
 * Standalone sampler state object
 *
 * Usage:
 * ```cpp
 * FSampler Sampler;
 * Sampler.Initialize(ETextureFilter::LinearMipmapLinear, ETextureAddress::Wrap, Device);
 * CommandList->bindSamplers(0, &Sampler.GetSamplerHandle().Get(), 1);
 * ```
 */
class FSampler
{
public:
	NOCOPYMOVE(FSampler)

	FSampler() = default;
	~FSampler();

	// Initialize sampler
	bool Initialize(
		ETextureFilter	Filter,
		ETextureAddress AddressU,
		ETextureAddress AddressV,
		ETextureAddress AddressW,
		nvrhi::IDevice* Device,
		TFLOAT			MaxAnisotropy = 16.0f);

	// Access
	[[nodiscard]] nvrhi::SamplerHandle GetSamplerHandle() const { return SamplerHandle; }

protected:
	nvrhi::SamplerHandle SamplerHandle;
};

/*-----------------------------------------------------------------------------
   Inline Implementations
-----------------------------------------------------------------------------*/

HLVM_INLINE_FUNC FTexture::FTexture()
	: Width(0)
	, Height(0)
	, MipLevels(1)
	, ArraySize(1)
	, SampleCount(1)
	, Format(ETextureFormat::RGBA8_UNORM)
	, Dimension(ETextureDimension::Texture2D)
{
}

HLVM_INLINE_FUNC FTexture::~FTexture()
{
	TextureHandle.Reset();
	TextureRTV.Reset();
	TextureDSV.Reset();
	TextureSRV.Reset();
	TextureUAV.Reset();
	SamplerCache.Empty();
}

HLVM_INLINE_FUNC FSampler::~FSampler()
{
	SamplerHandle.Reset();
}
