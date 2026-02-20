/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

 // Reference https://github.com/RobertBeckebans/RBDOOM-3-BFG/blob/4310fbd200b578014b22dce5fa82a48977eb149a/neo/sys/DeviceManager.cpp

#include "Renderer/DeviceManager.h"

void FDeviceManager::GetWindowDimensions(TUINT32& width, TUINT32& height) const
{
	width = DeviceParams.BackBufferWidth;
	height = DeviceParams.BackBufferHeight;
}

RHI::EGpuVendorId FDeviceManager::GetGPUVendor(TUINT32 vendorID) const
{
	return RHI::VenderId2Enum(vendorID);
}

void FDeviceManager::BackBufferResizing()
{
	// TODO
	// Framebuffer::Shutdown();
}

void FDeviceManager::BackBufferResized()
{
	// TODO
	//	if (tr.IsInitialized())
	//	{
	//		Framebuffer::ResizeFramebuffers();
	//	}
}

nvrhi::IFramebuffer* FDeviceManager::GetCurrentFramebuffer()
{
	return GetFramebuffer(GetCurrentBackBufferIndex());
}

nvrhi::IFramebuffer* FDeviceManager::GetFramebuffer(uint32_t /*index*/)
{
	// TODO
	//	if (index < (uint32_t)globalFramebuffers.swapFramebuffers.Num())
	//	{
	//		return globalFramebuffers.swapFramebuffers[index]->GetApiObject();
	//	}

	return nullptr;
}

FNVRHIMessageCallback& FNVRHIMessageCallback::GetInstance()
{
	static FNVRHIMessageCallback instance;
	return instance;
}

void FNVRHIMessageCallback::message(nvrhi::MessageSeverity severity, const char* messageText)
{
	switch (severity)
	{
		case nvrhi::MessageSeverity::Info:
			HLVM_LOG(LogRHI, info, TO_TCHAR_CSTR(messageText));
			break;
		case nvrhi::MessageSeverity::Warning:
			HLVM_LOG(LogRHI, warn, TO_TCHAR_CSTR(messageText));
			break;
		case nvrhi::MessageSeverity::Error:
			HLVM_LOG(LogRHI, err, TO_TCHAR_CSTR(messageText));
			break;
		case nvrhi::MessageSeverity::Fatal:
		default:
			HLVM_LOG(LogRHI, critical, TO_TCHAR_CSTR(messageText));
			break;
	}
}
