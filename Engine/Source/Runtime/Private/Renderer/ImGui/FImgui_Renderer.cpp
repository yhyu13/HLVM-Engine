/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 *
 * ImGui Renderer Implementation
 *
 * Base class for building ImGui UIs using NVRHI.
 * Adapted from NVIDIA Donut framework.
 */

#include "Renderer/ImGui/FImgui_Renderer.h"
#include "Renderer/FShaderFactory.h"

#include <Core/Log.h>

DECLARE_LOG_CATEGORY(LogImGui)

FImgui_Renderer::FImgui_Renderer(FDeviceManager* deviceManager)
    : IRenderPass(deviceManager)
{
	ImGui::CreateContext();
}

FImgui_Renderer::~FImgui_Renderer()
{
	ImguiNVRHI.Shutdown();
	ImGui::DestroyContext();
}

void FImgui_Renderer::Shutdown()
{
	ImguiNVRHI.Shutdown();
}

bool FImgui_Renderer::Initialize(nvrhi::IDevice* device, std::shared_ptr<FShaderFactory> shaderFactory)
{
	if (!ImguiNVRHI.Initialize(device, shaderFactory))
	{
		HLVM_LOG(LogImGui, err, TXT("FImgui_Renderer::Initialize: Failed to initialize ImGui NVRHI"));
		return false;
	}

	if (!ImguiNVRHI.UpdateFontTexture())
	{
		HLVM_LOG(LogImGui, err, TXT("FImgui_Renderer::Initialize: Failed to update font texture"));
		return false;
	}

	// Set ImGui display size to a reasonable default
	// This is required before ImGui::NewFrame() is called in Animate()
	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize = ImVec2(800.0f, 600.0f);

	HLVM_LOG(LogImGui, info, TXT("FImgui_Renderer::Initialize: Successfully initialized"));
	return true;
}

bool FImgui_Renderer::KeyboardUpdate(int key, int scancode, int action, int mods)
{
	(void)key;
	(void)scancode;
	(void)action;
	(void)mods;
	ImGuiIO& io = ImGui::GetIO();
	if (key >= 0 && key < IM_ARRAYSIZE(io.KeysDown))
	{
		io.KeysDown[key] = (action != 0);
	}
	return ImGui::GetIO().WantCaptureKeyboard;
}

bool FImgui_Renderer::KeyboardCharInput(unsigned int unicode, int mods)
{
	(void)mods;
	ImGuiIO& io = ImGui::GetIO();
	io.AddInputCharacter(unicode);
	return ImGui::GetIO().WantCaptureKeyboard;
}

bool FImgui_Renderer::MousePosUpdate(double xpos, double ypos)
{
	ImGuiIO& io = ImGui::GetIO();
	io.MousePos = ImVec2(static_cast<float>(xpos), static_cast<float>(ypos));
	return ImGui::GetIO().WantCaptureMouse;
}

bool FImgui_Renderer::MouseScrollUpdate(double xoffset, double yoffset)
{
	ImGuiIO& io = ImGui::GetIO();
	io.MouseWheelH += static_cast<float>(xoffset);
	io.MouseWheel += static_cast<float>(yoffset);
	return ImGui::GetIO().WantCaptureMouse;
}

bool FImgui_Renderer::MouseButtonUpdate(int button, int action, int mods)
{
	(void)mods;
	ImGuiIO& io = ImGui::GetIO();
	if (button >= 0 && button < IM_ARRAYSIZE(io.MouseDown))
	{
		io.MouseDown[button] = (action != 0);
	}
	return ImGui::GetIO().WantCaptureMouse;
}

void FImgui_Renderer::Animate(float elapsedTimeSeconds)
{
	ImGuiIO& io = ImGui::GetIO();
	io.DeltaTime = elapsedTimeSeconds;

	bImguiFrameOpened = true;
	ImGui::NewFrame();

	buildUI();

	ImGui::EndFrame();
	ImGui::Render();
}

void FImgui_Renderer::Render(nvrhi::IFramebuffer* framebuffer)
{
	if (bImguiFrameOpened)
	{
		// Update DisplaySize from framebuffer dimensions if available
		nvrhi::FramebufferInfoEx fbInfo = framebuffer->getFramebufferInfo();
		if (fbInfo.width > 0 && fbInfo.height > 0)
		{
			ImGuiIO& io = ImGui::GetIO();
			io.DisplaySize = ImVec2(static_cast<float>(fbInfo.width), static_cast<float>(fbInfo.height));
		}

		ImguiNVRHI.Render(framebuffer);
	}
}

void FImgui_Renderer::BackBufferResizing()
{
	ImguiNVRHI.BackBufferResizing();
}

void FImgui_Renderer::BackBufferResized(const uint32_t width, const uint32_t height, const uint32_t sampleCount)
{
	IRenderPass::BackBufferResized(width, height, sampleCount);
	ImguiNVRHI.BackBufferResizing();
}

void FImgui_Renderer::DisplayScaleChanged(float scaleX, float scaleY)
{
	ImGuiIO& io = ImGui::GetIO();
	io.DisplayFramebufferScale = ImVec2(scaleX, scaleY);
}
