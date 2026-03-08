/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 *
 *  Render Target Implementation
 */

#include "Renderer/RHI/Object/RenderTarget.h"
#include "Renderer/RHI/Object/Texture.h"

/*-----------------------------------------------------------------------------
	FRenderTarget Implementation
-----------------------------------------------------------------------------*/

FRenderTarget::FRenderTarget()
	: Device(nullptr)
	, Width(0)
	, Height(0)
	, Format(ETextureFormat::RGBA8_UNORM)
	, SampleCount(1)
{
}

FRenderTarget::~FRenderTarget()
{
	TextureHandle.Reset();
}

bool FRenderTarget::Initialize(
	nvrhi::IDevice* InDevice,
	uint32_t		InWidth,
	uint32_t		InHeight,
	ETextureFormat	InFormat,
	uint32_t		InSampleCount,
	bool			InbAllowShared)
{
	HLVM_ENSURE_F(!TextureHandle, TXT("Texture already initialized"));
	HLVM_ENSURE_F(InDevice, TXT("Device is null"));
	HLVM_ENSURE_F(InWidth > 0, TXT("Width is zero"));
	HLVM_ENSURE_F(InHeight > 0, TXT("Height is zero"));

	Device = InDevice;
	Width = InWidth;
	Height = InHeight;
	Format = InFormat;
	SampleCount = InSampleCount;
	bAllowShared = InbAllowShared;

	nvrhi::TextureDesc Desc;
	Desc.setDimension(nvrhi::TextureDimension::Texture2D);
	Desc.setFormat(hlvm_rhi::ConvertTextureFormat(InFormat));
	Desc.setWidth(Width);
	Desc.setHeight(Height);
	Desc.setInitialState(nvrhi::ResourceStates::RenderTarget);
	Desc.setKeepInitialState(true);
	Desc.setIsRenderTarget(true);
	Desc.setSampleCount(SampleCount);
	if (bAllowShared)
	{
		Desc.setSharedResourceFlags(nvrhi::SharedResourceFlags::Shared);
	}

	TextureHandle = Device->createTexture(Desc);
	HLVM_ENSURE_F(TextureHandle, TXT("Failed to create render target texture"));

	return true;
}

void FRenderTarget::Clear(
	nvrhi::ICommandList* CommandList,
	const nvrhi::Color&	 Color)
{
	HLVM_ENSURE_F(TextureHandle, TXT("Texture not initialized"));
	HLVM_ENSURE_F(CommandList, TXT("CommandList is null"));

	CommandList->clearTextureFloat(TextureHandle, nvrhi::AllSubresources, Color);
}

void FRenderTarget::Resolve(
	nvrhi::ICommandList* CommandList,
	FRenderTarget*		 DestTarget)
{
	HLVM_ENSURE_F(TextureHandle, TXT("Source texture not initialized"));
	HLVM_ENSURE_F(DestTarget, TXT("Dest target is null"));
	HLVM_ENSURE_F(DestTarget->GetTextureHandle(), TXT("Dest target texture not initialized"));
	HLVM_ENSURE_F(SampleCount > 1, TXT("Source is not MSAA"));
	HLVM_ENSURE_F(DestTarget->GetSampleCount() == 1, TXT("Dest target must be non-MSAA"));

	CommandList->resolveTexture(DestTarget->TextureHandle, nvrhi::AllSubresources, TextureHandle, nvrhi::AllSubresources);
}

void FRenderTarget::SetDebugName(const TCHAR* Name)
{
	DebugName = Name;
}

/*-----------------------------------------------------------------------------
	FDepthTarget Implementation
-----------------------------------------------------------------------------*/

FDepthTarget::FDepthTarget()
	: Device(nullptr)
	, Width(0)
	, Height(0)
	, bHasStencil(false)
{
}

FDepthTarget::~FDepthTarget()
{
	TextureHandle.Reset();
}

bool FDepthTarget::Initialize(
	nvrhi::IDevice* InDevice,
	uint32_t		InWidth,
	uint32_t		InHeight,
	ETextureFormat	InFormat,
	bool			InbHasStencil)
{
	HLVM_ENSURE_F(!TextureHandle, TXT("Texture already initialized"));
	HLVM_ENSURE_F(InDevice, TXT("Device is null"));
	HLVM_ENSURE_F(InWidth > 0, TXT("Width is zero"));
	HLVM_ENSURE_F(InHeight > 0, TXT("Height is zero"));
	HLVM_ENSURE_F(InFormat == ETextureFormat::D16 || InFormat == ETextureFormat::D24S8 || InFormat == ETextureFormat::D32 || InFormat == ETextureFormat::D32S8,
		TXT("Invalid depth format"));

	Device = InDevice;
	Width = InWidth;
	Height = InHeight;
	bHasStencil = InbHasStencil;
	Format = InFormat;

	nvrhi::TextureDesc Desc;
	Desc.setDimension(nvrhi::TextureDimension::Texture2D);
	Desc.setFormat(hlvm_rhi::ConvertTextureFormat(InFormat));
	Desc.setWidth(Width);
	Desc.setHeight(Height);
	Desc.setInitialState(nvrhi::ResourceStates::DepthWrite | nvrhi::ResourceStates::DepthRead);
	Desc.setKeepInitialState(true);

	TextureHandle = Device->createTexture(Desc);
	HLVM_ENSURE_F(TextureHandle, TXT("Failed to create depth target texture"));

	return true;
}

void FDepthTarget::Clear(
	nvrhi::ICommandList* CommandList,
	float				 Depth,
	uint8_t				 Stencil)
{
	HLVM_ENSURE_F(TextureHandle, TXT("Texture not initialized"));
	HLVM_ENSURE_F(CommandList, TXT("CommandList is null"));

	if (bHasStencil)
	{
		CommandList->clearDepthStencilTexture(TextureHandle, nvrhi::AllSubresources, true, Depth, false, Stencil);
	}
	else
	{
		CommandList->clearDepthStencilTexture(TextureHandle, nvrhi::AllSubresources, true, Depth, false, 0);
	}
}

void FDepthTarget::SetDebugName(const TCHAR* Name)
{
	DebugName = Name;
}
