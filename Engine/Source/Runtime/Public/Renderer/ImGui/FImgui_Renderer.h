/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 *
 * ImGui Renderer Header
 *
 * Base class for building ImGui UIs using NVRHI.
 * Adapted from NVIDIA Donut framework.
 */

#pragma once

#include <memory>
#include <vector>

#include <imgui.h>

#include <Renderer/ImGui/FImgui_NVRHI.h>
#include <Renderer/IRenderPass.h>

class FImgui_Renderer : public IRenderPass
{
public:
    using IRenderPass::IRenderPass;
    explicit FImgui_Renderer(FDeviceManager* deviceManager);
    virtual ~FImgui_Renderer() override;

    bool Initialize(nvrhi::IDevice* device, std::shared_ptr<FShaderFactory> shaderFactory);
    void Shutdown();

    using IRenderPass::KeyboardUpdate;
    using IRenderPass::KeyboardCharInput;
    using IRenderPass::MousePosUpdate;
    using IRenderPass::MouseScrollUpdate;
    using IRenderPass::MouseButtonUpdate;
    using IRenderPass::Animate;
    using IRenderPass::Render;
    using IRenderPass::BackBufferResizing;
    using IRenderPass::BackBufferResized;
    using IRenderPass::DisplayScaleChanged;

    virtual bool KeyboardUpdate(int key, int scancode, int action, int mods) override;
    virtual bool KeyboardCharInput(unsigned int unicode, int mods) override;
    virtual bool MousePosUpdate(double xpos, double ypos) override;
    virtual bool MouseScrollUpdate(double xoffset, double yoffset) override;
    virtual bool MouseButtonUpdate(int button, int action, int mods) override;

    virtual void Animate(float elapsedTimeSeconds) override;
    virtual void Render(nvrhi::IFramebuffer* framebuffer) override;
    virtual void BackBufferResizing() override;
    virtual void BackBufferResized(const uint32_t width, const uint32_t height, const uint32_t sampleCount) override;
    virtual void DisplayScaleChanged(float scaleX, float scaleY) override;

    FImgui_NVRHI& GetImguiNVRHI() { return ImguiNVRHI; }

protected:
    virtual void buildUI() = 0;

private:
    FImgui_NVRHI ImguiNVRHI;
    bool bImguiFrameOpened = false;
};
