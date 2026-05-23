/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 *
 *  Texture Implementation
 */

#include "Renderer/RHI/Object/Texture.h"

/*-----------------------------------------------------------------------------
   FTexture Implementation
-----------------------------------------------------------------------------*/

bool FTexture::Initialize(
	TUINT32				 InWidth,
	TUINT32				 InHeight,
	TUINT32				 InMipLevels,
	ETextureFormat		 InFormat,
	ETextureDimension	 InDimension,
	nvrhi::IDevice*		 InDevice,
	const void*			 InitialData,
	nvrhi::ICommandList* CommandList)
{
	HLVM_ENSURE_F(!TextureHandle, TXT("Texture already initialized"));
	HLVM_ENSURE_F(InWidth > 0 && InHeight > 0, TXT("Invalid texture dimensions"));
	HLVM_ENSURE_F(InDevice, TXT("Device is null"));

	Width = InWidth;
	Height = InHeight;
	MipLevels = InMipLevels > 0 ? InMipLevels : 1;
	ArraySize = 1;
	SampleCount = 1;
	Format = InFormat;
	Dimension = InDimension;
	Device = InDevice;

	// Create texture descriptor
	nvrhi::TextureDesc Desc;
	Desc.setDimension(hlvm_rhi::ConvertTextureDimension(Dimension));
	Desc.setWidth(Width);
	Desc.setHeight(Height);
	Desc.setFormat(hlvm_rhi::ConvertTextureFormat(Format));
	Desc.setMipLevels(MipLevels);
	Desc.setArraySize(ArraySize);
	Desc.setSampleCount(SampleCount);
	Desc.debugName = DebugName.IsEmpty() ? "Texture" : DebugName.ToCharCStr();

	// Set usage flags
	Desc.setInitialState(nvrhi::ResourceStates::ShaderResource);
	Desc.setKeepInitialState(InitialData == nullptr);

	// Create texture
	TextureHandle = Device->createTexture(Desc);
	HLVM_ENSURE_F(TextureHandle, TXT("Failed to create texture"));

	// Create views
	CreateViews();

	// Upload initial data if provided
	if (InitialData)
	{
		Update(InitialData, Width * Height * 4, 0, 0, CommandList);
	}

	return true;
}

bool FTexture::InitializeFromHandle(nvrhi::TextureHandle InHandle, nvrhi::IDevice* InDevice)
{
	HLVM_ENSURE_F(!TextureHandle, TXT("Texture already initialized"));
	HLVM_ENSURE_F(InHandle, TXT("Null texture handle"));
	HLVM_ENSURE_F(InDevice, TXT("Device is null"));

	const nvrhi::TextureDesc& Desc = InHandle->getDesc();

	Width = Desc.width;
	Height = Desc.height;
	MipLevels = Desc.mipLevels > 0 ? Desc.mipLevels : 1;
	ArraySize = Desc.arraySize;
	SampleCount = Desc.sampleCount;
	Format = Desc.format;
	Dimension = Desc.dimension;
	Device = InDevice;

	TextureHandle = InHandle;

	// Create views
	CreateViews();

	return true;
}

bool FTexture::InitializeRenderTarget(
	TUINT32			InWidth,
	TUINT32			InHeight,
	ETextureFormat	InFormat,
	nvrhi::IDevice* InDevice,
	TUINT32			InSampleCount)
{
	HLVM_ENSURE_F(!TextureHandle, TXT("Texture already initialized"));
	HLVM_ENSURE_F(InWidth > 0 && InHeight > 0, TXT("Invalid texture dimensions"));
	HLVM_ENSURE_F(InDevice, TXT("Device is null"));

	Width = InWidth;
	Height = InHeight;
	MipLevels = 1;
	ArraySize = 1;
	SampleCount = InSampleCount > 0 ? InSampleCount : 1;
	Format = InFormat;
	Dimension = ETextureDimension::Texture2D;
	Device = InDevice;

	// Create texture descriptor for render target
	nvrhi::TextureDesc Desc;
	Desc.setDimension(nvrhi::TextureDimension::Texture2D);
	Desc.setWidth(Width);
	Desc.setHeight(Height);
	Desc.setFormat(hlvm_rhi::ConvertTextureFormat(Format));
	Desc.setMipLevels(1);
	Desc.setArraySize(1);
	Desc.setSampleCount(SampleCount);
	Desc.setIsRenderTarget(true);
	Desc.setInitialState(nvrhi::ResourceStates::RenderTarget);
	Desc.setKeepInitialState(true);
	Desc.debugName = DebugName.IsEmpty() ? "RenderTarget" : DebugName.ToCharCStr();

	// Create texture
	TextureHandle = Device->createTexture(Desc);
	HLVM_ENSURE_F(TextureHandle, TXT("Failed to create render target texture"));

	// Create views (RTV will be set to TextureHandle for non-depth formats)
	CreateViews();

	return true;
}

void FTexture::CreateViews()
{
	if (!TextureHandle)
	{
		return;
	}

	// Determine if this is a depth format
	const bool bIsDepth = (Format == ETextureFormat::D16 || Format == ETextureFormat::D24S8 || 
						   Format == ETextureFormat::D32 || Format == ETextureFormat::D32S8);

	// For non-depth textures, the main handle can be used as SRV
	if (!bIsDepth)
	{
		TextureSRV = TextureHandle;
	}

	// For render targets, the main handle can be used as RTV
	// NVRHI handles the layout transitions internally
	if (!bIsDepth && SampleCount == 1)
	{
		TextureRTV = TextureHandle;
	}

	// For depth formats, the main handle serves as DSV
	if (bIsDepth)
	{
		TextureDSV = TextureHandle;
	}
}

nvrhi::SamplerHandle FTexture::GetSampler(ETextureFilter Filter)
{
	// Check cache first
	if (SamplerCache.Contains(Filter))
	{
		return SamplerCache[Filter];
	}

	// Create new sampler
	nvrhi::SamplerDesc Desc;

	// Set filter
	switch (Filter)
	{
		default:
		case ETextureFilter::Nearest:
			Desc.setMinFilter(false).setMagFilter(false).setMipFilter(false);
			break;
		case ETextureFilter::Linear:
			Desc.setMinFilter(true).setMagFilter(true).setMipFilter(false);
			break;
		case ETextureFilter::NearestMipmapNearest:
			Desc.setMinFilter(false).setMagFilter(false).setMipFilter(false);
			break;
		case ETextureFilter::NearestMipmapLinear:
			Desc.setMinFilter(false).setMagFilter(false).setMipFilter(true);
			break;
		case ETextureFilter::LinearMipmapNearest:
			Desc.setMinFilter(true).setMagFilter(true).setMipFilter(false);
			break;
		case ETextureFilter::LinearMipmapLinear:
			Desc.setMinFilter(true).setMagFilter(true).setMipFilter(true);
			break;
		case ETextureFilter::Anisotropic:
			Desc.setMinFilter(true).setMagFilter(true).setMipFilter(true);
			Desc.setMaxAnisotropy(16.0f);
			break;
	}

	// Default address modes
	Desc.setAddressU(nvrhi::SamplerAddressMode::ClampToEdge);
	Desc.setAddressV(nvrhi::SamplerAddressMode::ClampToEdge);
	Desc.setAddressW(nvrhi::SamplerAddressMode::ClampToEdge);

	// Create sampler
	nvrhi::SamplerHandle Sampler = Device->createSampler(Desc);
	if (Sampler)
	{
		SamplerCache.Add(Filter, Sampler);
	}

	return Sampler;
}

void FTexture::GenerateMipmaps(nvrhi::ICommandList* /*CommandList*/)
{
	HLVM_ENSURE_F(TextureHandle, TXT("Texture not initialized"));
	HLVM_ENSURE_F(MipLevels > 1, TXT("Texture has only 1 mip level"));

	// TODO
	//  https://docs.vulkan.org/samples/latest/samples/api/texture_mipmap_generation/README.html
	// 	https://vulkan-tutorial.com/Generating_Mipmaps
	//  doesn't have built-in mipmap generation
	// This would require a compute shader or blit commands
	// For now, just log a message
	HLVM_LOG(LogRHI, warn, TXT("FTexture::GenerateMipmaps - Not implemented, requires compute shader"));
}

void FTexture::SetDebugName(const TCHAR* Name)
{
	DebugName = Name;
}

/*-----------------------------------------------------------------------------
   FSampler Implementation
-----------------------------------------------------------------------------*/

bool FSampler::Initialize(
	ETextureFilter	Filter,
	ETextureAddress AddressU,
	ETextureAddress AddressV,
	ETextureAddress AddressW,
	nvrhi::IDevice* InDevice,
	TFLOAT			MaxAnisotropy)
{
	HLVM_ENSURE_F(!SamplerHandle, TXT("Sampler already initialized"));
	HLVM_ENSURE_F(InDevice, TXT("Device is null"));

	// Create sampler descriptor
	nvrhi::SamplerDesc Desc;

	// Set filter
	switch (Filter)
	{
		default:
		case ETextureFilter::Nearest:
			Desc.setMinFilter(false).setMagFilter(false).setMipFilter(false);
			break;
		case ETextureFilter::Linear:
			Desc.setMinFilter(true).setMagFilter(true).setMipFilter(false);
			break;
		case ETextureFilter::NearestMipmapNearest:
			Desc.setMinFilter(false).setMagFilter(false).setMipFilter(false);
			break;
		case ETextureFilter::NearestMipmapLinear:
			Desc.setMinFilter(false).setMagFilter(false).setMipFilter(true);
			break;
		case ETextureFilter::LinearMipmapNearest:
			Desc.setMinFilter(true).setMagFilter(true).setMipFilter(false);
			break;
		case ETextureFilter::LinearMipmapLinear:
			Desc.setMinFilter(true).setMagFilter(true).setMipFilter(true);
			break;
		case ETextureFilter::Anisotropic:
			Desc.setMinFilter(true).setMagFilter(true).setMipFilter(true);
			Desc.setMaxAnisotropy(MaxAnisotropy);
			break;
	}

	// Set address modes
	auto ConvertAddress = [](ETextureAddress Address) -> nvrhi::SamplerAddressMode {
		switch (Address)
		{
			case ETextureAddress::Wrap:
				return nvrhi::SamplerAddressMode::Repeat;
			case ETextureAddress::Mirror:
				return nvrhi::SamplerAddressMode::Mirror;
			case ETextureAddress::Clamp:
				return nvrhi::SamplerAddressMode::ClampToEdge;
			case ETextureAddress::Border:
				return nvrhi::SamplerAddressMode::ClampToBorder;
			case ETextureAddress::MirrorOnce:
				return nvrhi::SamplerAddressMode::MirrorOnce;
			default:
				return nvrhi::SamplerAddressMode::ClampToEdge;
		}
	};

	Desc.setAddressU(ConvertAddress(AddressU));
	Desc.setAddressV(ConvertAddress(AddressV));
	Desc.setAddressW(ConvertAddress(AddressW));

	// Create sampler
	SamplerHandle = InDevice->createSampler(Desc);
	HLVM_ENSURE_F(SamplerHandle, TXT("Failed to create sampler"));

	return true;
}
