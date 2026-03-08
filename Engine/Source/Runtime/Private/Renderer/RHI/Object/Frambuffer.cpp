/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 *
 *  Framebuffer Implementation
 */

#include "Renderer/RHI/Object/Frambuffer.h"

/*-----------------------------------------------------------------------------
   FFramebuffer Implementation
-----------------------------------------------------------------------------*/

bool FFramebuffer::Initialize(nvrhi::IDevice* InDevice)
{
	HLVM_ENSURE_F(!FramebufferHandle, TXT("Framebuffer already initialized"));
	HLVM_ENSURE_F(InDevice, TXT("Device is null"));

	Device = InDevice;
	return true;
}

void FFramebuffer::AddColorAttachment(const FFramebufferAttachment& Attachment)
{
	HLVM_ENSURE_F(Attachment.Texture, TXT("Attachment texture is null"));

	ColorAttachments.Add(Attachment);

	// Update dimensions from first attachment
	if (ColorAttachments.Num() == 1 && Attachment.Texture)
	{
		const auto Info = Attachment.Texture->getDesc();
		Width = Info.width;
		Height = Info.height;
	}
}

void FFramebuffer::SetDepthAttachment(const FFramebufferAttachment& Attachment)
{
	HLVM_ENSURE_F(Attachment.Texture, TXT("Depth attachment texture is null"));

	DepthAttachment = Attachment;

	// Update dimensions if not set
	if (Width == 0 || Height == 0)
	{
		const auto Info = Attachment.Texture->getDesc();
		Width = Info.width;
		Height = Info.height;
	}
}

bool FFramebuffer::CreateFramebuffer()
{
	HLVM_ENSURE_F(Device, TXT("Device not initialized"));
	HLVM_ENSURE_F(ColorAttachments.Num() > 0, TXT("No color attachments"));

	// Build framebuffer descriptor
	nvrhi::FramebufferDesc Desc;

	// Add color attachments
	for (const auto& Attachment : ColorAttachments)
	{
		nvrhi::FramebufferAttachment ColorAttach;
		ColorAttach.setTexture(Attachment.Texture.Get());
		ColorAttach.setMipLevel(Attachment.MipLevel);
		ColorAttach.setArraySlice(Attachment.ArraySlice);

		Desc.addColorAttachment(ColorAttach);
	}

	// Add depth attachment if present
	if (DepthAttachment.Texture)
	{
		nvrhi::FramebufferAttachment DepthAttach;
		DepthAttach.setTexture(DepthAttachment.Texture.Get());
		DepthAttach.setMipLevel(DepthAttachment.MipLevel);
		DepthAttach.setArraySlice(DepthAttachment.ArraySlice);

		Desc.setDepthAttachment(DepthAttach);
	}

	// Create framebuffer
	FramebufferHandle = Device->createFramebuffer(Desc);
	HLVM_ENSURE_F(FramebufferHandle, TXT("Failed to create framebuffer"));

	// Set default viewport and scissor
	SetViewport(0, 0, static_cast<TFLOAT>(Width), static_cast<TFLOAT>(Height));
	SetScissor(0, 0, Width, Height);

	return true;
}

void FFramebuffer::SetViewport(TFLOAT X, TFLOAT Y, TFLOAT InWidth, TFLOAT InHeight, TFLOAT MinDepth, TFLOAT MaxDepth)
{
	Viewport.minX = X;
	Viewport.minY = Y;
	Viewport.maxX = X + InWidth;
	Viewport.maxY = Y + InHeight;
	Viewport.minZ = MinDepth;
	Viewport.maxZ = MaxDepth;
}

void FFramebuffer::SetScissor(TINT32 X, TINT32 Y, TUINT32 InWidth, TUINT32 InHeight)
{
	Scissor.minX = X;
	Scissor.minY = Y;
	Scissor.maxX = X + static_cast<TINT32>(InWidth);
	Scissor.maxY = Y + static_cast<TINT32>(InHeight);
}

void FFramebuffer::SetDebugName(const TCHAR* Name)
{
	DebugName = Name;
}

/*-----------------------------------------------------------------------------
   FFramebufferManager Implementation
-----------------------------------------------------------------------------*/

void FFramebufferManager::Initialize(nvrhi::IDevice* InDevice)
{
	HLVM_ENSURE_F(InDevice, TXT("Device is null"));
	Device = InDevice;
}

FFramebuffer* FFramebufferManager::CreateFramebuffer(const TCHAR* Name)
{
	HLVM_ENSURE_F(Device, TXT("Manager not initialized"));
	HLVM_ENSURE_F(Name, TXT("Name is null"));

	// Check if already exists
	if (FramebufferPool.Contains(Name))
	{
		HLVM_LOG(LogRHI, warn, TXT("FFramebufferManager::CreateFramebuffer - Framebuffer '{}' already exists"), Name);
		return FramebufferPool[Name].get();
	}

	// Create new framebuffer
	auto Framebuffer = TUniquePtr<FFramebuffer>(new FFramebuffer());
	Framebuffer->Initialize(Device);
	Framebuffer->SetDebugName(Name);

	// Add to pool
	FFramebuffer* RawPtr = Framebuffer.get();
	FramebufferPool.Add(Name, MoveTemp(Framebuffer));

	return RawPtr;
}

FFramebuffer* FFramebufferManager::FindFramebuffer(const TCHAR* Name)
{
	HLVM_ENSURE_F(Name, TXT("Name is null"));

	if (FramebufferPool.Contains(Name))
	{
		return FramebufferPool[Name].get();
	}

	return nullptr;
}

void FFramebufferManager::RemoveFramebuffer(const TCHAR* Name)
{
	HLVM_ENSURE_F(Name, TXT("Name is null"));

	FramebufferPool.Remove(Name);
}

void FFramebufferManager::RemoveAllFramebuffers()
{
	FramebufferPool.Empty();
}

FFramebufferManager::~FFramebufferManager()
{
	RemoveAllFramebuffers();
}
