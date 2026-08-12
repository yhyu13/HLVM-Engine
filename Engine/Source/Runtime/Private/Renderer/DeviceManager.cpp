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

// =============================================================================
// RENDER PASS MANAGEMENT
// =============================================================================

void FDeviceManager::AddRenderPassToFront(TSharedPtr<IRenderPass> pass)
{
	HLVM_ASSERT(pass != nullptr);
	HLVM_ASSERT(!m_bIsRendering && "Cannot modify render pass list during Render()");

	m_vRenderPasses.insert(m_vRenderPasses.begin(), pass);
}

void FDeviceManager::AddRenderPassToBack(TSharedPtr<IRenderPass> pass)
{
	HLVM_ASSERT(pass != nullptr);
	HLVM_ASSERT(!m_bIsRendering && "Cannot modify render pass list during Render()");

	m_vRenderPasses.push_back(pass);
}

void FDeviceManager::RemoveRenderPass(TSharedPtr<IRenderPass> pass)
{
	HLVM_ASSERT(pass != nullptr);
	HLVM_ASSERT(!m_bIsRendering && "Cannot modify render pass list during Render()");

	for (auto it = m_vRenderPasses.begin(); it != m_vRenderPasses.end(); ++it)
	{
		if (*it == pass)
		{
			m_vRenderPasses.erase(it);
			return;
		}
	}
}

void FDeviceManager::RunMessageLoop()
{
	FGLFW3Window* glfwWindow = static_cast<FGLFW3Window*>(WindowHandle.get());

	float previousTime = static_cast<float>(glfwGetTime());

	while (!glfwWindow->ShouldClose())
	{
		// Calculate delta time
		float currentTime = static_cast<float>(glfwGetTime());
		float deltaTime = currentTime - previousTime;
		previousTime = currentTime;

		// Poll GLFW events (keyboard, mouse, etc.)
		glfwWindow->ProcessEvents();

		// Call Animate on all render passes
		for (auto& pass : m_vRenderPasses)
		{
			pass->Animate(deltaTime);
		}

		// Render each pass
		m_bIsRendering = true;
		HLVM_ENSURE(BeginFrame());
		{
			nvrhi::IFramebuffer* framebuffer = GetCurrentFramebuffer();
			for (auto& pass : m_vRenderPasses)
			{
				pass->Render(framebuffer);
			}
		}
		HLVM_ENSURE(EndFrame());
		HLVM_ENSURE(Present());
		// TODO (2026-08-10): removing this waitForIdle() allows frame overlap
		// (~42 ms/frame in the half-res ReSTIR test) but exposes a real engine
		// bug: the acquire semaphores are reused before their pending signal
		// completes (VUID-vkAcquireNextImageKHR-semaphore-01779). Fix the
		// acquire-semaphore tracking (per-index event query) before enabling
		// overlap. Keeping the wait for now — correctness first.
		GetDevice()->waitForIdle(); // TODO : Or frame controller wait for limited amount of time
		m_bIsRendering = false;
	}
	GetDevice()->waitForIdle();
}

void FDeviceManager::StopMessageLoop()
{
	FGLFW3Window* glfwWindow = static_cast<FGLFW3Window*>(WindowHandle.get());
	glfwWindow->SetShouldClose();
}
