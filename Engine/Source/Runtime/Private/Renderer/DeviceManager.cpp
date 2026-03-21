/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

// Reference
// https://github.com/RobertBeckebans/RBDOOM-3-BFG/blob/4310fbd200b578014b22dce5fa82a48977eb149a/neo/sys/DeviceManager.cpp
// https://github.com/NVIDIA-RTX/Donut/blob/2c1077673edb0e0d814c202e6ce8e502d245c2da/src/app/DeviceManager.cpp
#include "Renderer/DeviceManager.h"

void FDeviceManager::GetWindowDimensions(TUINT32& width, TUINT32& height) const
{
	width = DeviceParams.BackBufferWidth;
	height = DeviceParams.BackBufferHeight;
}

EGpuVendorID FDeviceManager::GetGPUVendor(TUINT32 vendorID) const
{
	return hlvm_rhi::VenderId2Enum(vendorID);
}

void FDeviceManager::BackBufferResizing()
{
	// Notify derived class that swapchain is about to be recreated
	OnBeforeSwapchainRecreate();
}

void FDeviceManager::BackBufferResized()
{
	// Notify derived class that swapchain has been recreated
	OnAfterSwapchainRecreate();
}

nvrhi::IFramebuffer* FDeviceManager::GetCurrentFramebuffer()
{
	return GetFramebuffer(GetCurrentBackBufferIndex());
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
