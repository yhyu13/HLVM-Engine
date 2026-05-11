/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#include "DeviceManagerVk.h"
#include "Renderer/Common/FCommonRenderPasses.h"

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

// ImGui integration accessors
void* FDeviceManagerVk::GetVkInstance() const
{
	return static_cast<VkInstance>(instance.get());
}

void* FDeviceManagerVk::GetVkPhysicalDevice() const
{
	return static_cast<VkPhysicalDevice>(physicalDevice);
}

void* FDeviceManagerVk::GetVkDevice() const
{
	return static_cast<VkDevice>(device.get());
}

void* FDeviceManagerVk::GetGraphicsQueue() const
{
	return static_cast<VkQueue>(graphicsQueue);
}

void* FDeviceManagerVk::GetRenderPass() const
{
	// NVRHI uses dynamic rendering without traditional render passes
	// Return VK_NULL_HANDLE - ImGui will create its own if needed
	return VK_NULL_HANDLE;
}

TINT32 FDeviceManagerVk::GetGraphicsFamilyIndex() const
{
	return static_cast<TINT32>(m_GraphicsQueueFamily);
}

void* FDeviceManagerVk::GetImGuiDescriptorPool() const
{
	return static_cast<VkDescriptorPool>(m_ImGuiDescriptorPool.get());
}

TINT32 FDeviceManagerVk::GetImGuiQueueFamilyIndex() const
{
	return static_cast<TINT32>(m_GraphicsQueueFamily);
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
	// Wrap entire shutdown in try-catch to prevent std::terminate
	// if any step throws
	try
	{
		// 1 . Destroy render passes
		{
			m_vRenderPasses.clear();
		}

		// 2 . Shutdown common render passes (must happen before device destruction)
		FCommonRenderPasses::Shutdown();

		// 3 . Destroy device and swapchain
		DestroyDeviceAndSwapChain();
	}
	catch (const std::exception& e)
	{
		HLVM_LOG(LogRHI, err, TXT("Shutdown exception: {}"), TO_TCHAR_CSTR(e.what()));
	}
	catch (...)
	{
		HLVM_LOG(LogRHI, err, TXT("Shutdown unknown exception"));
	}
}

void* FDeviceManagerVk::GetGLFWWindow() const
{
	if (!WindowHandle)
	{
		return nullptr;
	}
	FGLFW3Window* glfwWindow = static_cast<FGLFW3Window*>(WindowHandle.get());
	return glfwWindow->GetGLFWWindow();
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

// =============================================================================
// IGLFWInputCallbacks - Route input to render passes
// =============================================================================

bool FDeviceManagerVk::OnKey(int key, int scancode, int action, int mods)
{
	for (auto& pass : m_vRenderPasses)
	{
		if (pass->KeyboardUpdate(key, scancode, action, mods))
		{
			return true; // Event consumed
		}
	}
	return false;
}

bool FDeviceManagerVk::OnChar(unsigned int unicode, int mods)
{
	for (auto& pass : m_vRenderPasses)
	{
		if (pass->KeyboardCharInput(unicode, mods))
		{
			return true;
		}
	}
	return false;
}

bool FDeviceManagerVk::OnMousePos(double xpos, double ypos)
{
	for (auto& pass : m_vRenderPasses)
	{
		if (pass->MousePosUpdate(xpos, ypos))
		{
			return true;
		}
	}
	return false;
}

bool FDeviceManagerVk::OnScroll(double xoffset, double yoffset)
{
	for (auto& pass : m_vRenderPasses)
	{
		if (pass->MouseScrollUpdate(xoffset, yoffset))
		{
			return true;
		}
	}
	return false;
}

bool FDeviceManagerVk::OnMouseButton(int button, int action, int mods)
{
	for (auto& pass : m_vRenderPasses)
	{
		if (pass->MouseButtonUpdate(button, action, mods))
		{
			return true;
		}
	}
	return false;
}

void FDeviceManagerVk::SetInputCallbacks(GLFWwindow* window)
{
	glfwSetKeyCallback(window, [](GLFWwindow* w, int key, int scancode, int action, int mods) {
		FDeviceManagerVk* manager = static_cast<FDeviceManagerVk*>(glfwGetWindowUserPointer(w));
		if (manager) manager->OnKey(key, scancode, action, mods);
	});
	glfwSetCharCallback(window, [](GLFWwindow* w, unsigned int unicode) {
		FDeviceManagerVk* manager = static_cast<FDeviceManagerVk*>(glfwGetWindowUserPointer(w));
		if (manager) manager->OnChar(unicode, 0);
	});
	glfwSetCursorPosCallback(window, [](GLFWwindow* w, double xpos, double ypos) {
		FDeviceManagerVk* manager = static_cast<FDeviceManagerVk*>(glfwGetWindowUserPointer(w));
		if (manager) manager->OnMousePos(xpos, ypos);
	});
	glfwSetScrollCallback(window, [](GLFWwindow* w, double xoffset, double yoffset) {
		FDeviceManagerVk* manager = static_cast<FDeviceManagerVk*>(glfwGetWindowUserPointer(w));
		if (manager) manager->OnScroll(xoffset, yoffset);
	});
	glfwSetMouseButtonCallback(window, [](GLFWwindow* w, int button, int action, int mods) {
		FDeviceManagerVk* manager = static_cast<FDeviceManagerVk*>(glfwGetWindowUserPointer(w));
		if (manager) manager->OnMouseButton(button, action, mods);
	});
}

#endif
