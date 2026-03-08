/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#include "DeviceManagerVk.h"

#if HLVM_VULKAN_RENDERER

// =============================================================================
// FACTORY IMPLEMENTATION
// =============================================================================

TUniquePtr<FDeviceManager> FDeviceManager::Create(nvrhi::GraphicsAPI api)
{
	switch (api)
	{
		case nvrhi::GraphicsAPI::D3D11:
			return nullptr;
		case nvrhi::GraphicsAPI::D3D12:
			return nullptr;
		case nvrhi::GraphicsAPI::VULKAN:
		default:
			break;
	}
	return TUniquePtr<FDeviceManager>(new FDeviceManagerVk());
}

// =============================================================================
// PUBLIC INTERFACE IMPLEMENTATION
// =============================================================================

nvrhi::IDevice* FDeviceManagerVk::GetDevice() const
{
	if (m_ValidationLayer)
	{
		return m_ValidationLayer;
	}
	return m_NvrhiDevice;
}

const char* FDeviceManagerVk::GetRendererString() const
{
	return m_RendererString.c_str();
}

nvrhi::GraphicsAPI FDeviceManagerVk::GetGraphicsAPI() const
{
	return nvrhi::GraphicsAPI::VULKAN;
}

// =============================================================================
// DEBUG CALLBACK
// =============================================================================

VKAPI_ATTR vk::Bool32 VKAPI_CALL FDeviceManagerVk::DebugCallback(
	vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	vk::DebugUtilsMessageTypeFlagsEXT /*messageType*/,
	const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void*										  pUserData)
{
	const FDeviceManagerVk* manager = static_cast<const FDeviceManagerVk*>(pUserData);

	if (manager)
	{
		const auto& ignored = manager->DeviceParams.IgnoredVulkanValidationMessageLocations;
		// Note: location not available in DebugUtils, would need to parse message or use DebugReport
	}

	if (messageSeverity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)
	{
		HLVM_LOG(LogRHI, err, TXT("[Vulkan] ERROR: {}"), TO_TCHAR_CSTR(pCallbackData->pMessage));
	}
	else if (messageSeverity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
	{
		HLVM_LOG(LogRHI, warn, TXT("[Vulkan] WARNING: {}"), TO_TCHAR_CSTR(pCallbackData->pMessage));
	}
	else if (messageSeverity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo)
	{
		HLVM_LOG(LogRHI, info, TXT("[Vulkan] INFO: {}"), TO_TCHAR_CSTR(pCallbackData->pMessage));
	}

	return VK_FALSE;
}

// =============================================================================
// STUBS / EMPTY IMPLEMENTATIONS FOR INTERFACE
// =============================================================================

bool FDeviceManagerVk::CreateWindowDeviceAndSwapChain(const IWindow::Properties& Params)
{
	// 1 . Create window
	// Log
	HLVM_LOG(LogRHI, debug, TXT("Creating window with properties:\n{}"), Params.ToString());
	WindowHandle = MAKE_UNIQUE(FGLFW3VulkanWindow, Params);
	if (WindowHandle)
	{
		HLVM_LOG(LogRHI, debug, TXT("FGLFW3VulkanWindow created!"));
	}
	else
	{
		HLVM_LOG(LogRHI, err, TXT("Failed to create window with properties:\n{}"), Params.ToString());
		return false;
	}
	return CreateDeviceAndSwapChain();
}

void FDeviceManagerVk::Shutdown()
{
	DestroyDeviceAndSwapChain();
}

void FDeviceManagerVk::GetDPIScaleInfo(float& OutScaleX, float& OutScaleY) const
{
	HLVM_NOT_IMPLEMENTED();
	// TODO: Implement DPI scaling query
	OutScaleX = 1.0f;
	OutScaleY = 1.0f;
}

void FDeviceManagerVk::UpdateWindowSize(const FUInt2& Params)
{
	DeviceParams.BackBufferWidth = Params.x;
	DeviceParams.BackBufferHeight = Params.y;
	ResizeSwapChain();
}

void FDeviceManagerVk::SetVSyncMode(TINT32 VSyncMode)
{
	DeviceParams.VSyncMode = VSyncMode;
	// Requires swapchain recreationation to apply new present mode
	ResizeSwapChain();
}
#endif
