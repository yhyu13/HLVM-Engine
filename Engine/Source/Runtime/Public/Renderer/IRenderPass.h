/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 *
 * Render Pass Interface
 *
 * Abstract base class for render passes in the HLVM rendering pipeline.
 * Adapted from NVIDIA Donut framework.
 */

#pragma once

#include <cstdint>
#include <memory>

namespace nvrhi
{
class IFramebuffer;
class IDevice;
} // namespace nvrhi

class FDeviceManager;

class IRenderPass : public std::enable_shared_from_this<IRenderPass>
{
public:
    explicit IRenderPass(FDeviceManager* deviceManager)
        : m_DeviceManager(deviceManager)
    {
    }

    virtual ~IRenderPass() = default;

    virtual void SetLatewarpOptions() { }
    virtual bool ShouldAnimateUnfocused() { return false; }
    virtual bool ShouldRenderUnfocused() { return false; }

    // If this function returns 'true', and the device manager has a depth buffer
    // (DeviceCreationParameters::depthBufferFormat != UNKNOWN), the Render(...) function will be called
    // with a framebuffer that has a depth attachment.
    // Otherwise, the framebuffer will only have a color attachment - which is useful for UI rendering.
    virtual bool SupportsDepthBuffer() { return true; }

    virtual void Render(nvrhi::IFramebuffer* framebuffer) { (void)framebuffer; }
    virtual void Animate(float fElapsedTimeSeconds) { (void)fElapsedTimeSeconds; }
    virtual void BackBufferResizing() { }
    virtual void BackBufferResized(const uint32_t width, const uint32_t height, const uint32_t sampleCount) 
    { 
        (void)width; 
        (void)height; 
        (void)sampleCount; 
    }

    // Called before Animate() when a DPI change was detected
    virtual void DisplayScaleChanged(float scaleX, float scaleY) 
    { 
        (void)scaleX; 
        (void)scaleY; 
    }

    // all of these pass in GLFW constants as arguments
    // see http://www.glfw.org/docs/latest/input.html
    // return value is true if the event was consumed by this render pass, false if it should be passed on
    virtual bool KeyboardUpdate(int key, int scancode, int action, int mods) 
    { 
        (void)key; 
        (void)scancode; 
        (void)action; 
        (void)mods; 
        return false; 
    }
    virtual bool KeyboardCharInput(unsigned int unicode, int mods) 
    { 
        (void)unicode; 
        (void)mods; 
        return false; 
    }
    virtual bool MousePosUpdate(double xpos, double ypos) 
    { 
        (void)xpos; 
        (void)ypos; 
        return false; 
    }
    virtual bool MouseScrollUpdate(double xoffset, double yoffset) 
    { 
        (void)xoffset; 
        (void)yoffset; 
        return false; 
    }
    virtual bool MouseButtonUpdate(int button, int action, int mods) 
    { 
        (void)button; 
        (void)action; 
        (void)mods; 
        return false; 
    }
    virtual bool JoystickButtonUpdate(int button, bool pressed) 
    { 
        (void)button; 
        (void)pressed; 
        return false; 
    }
    virtual bool JoystickAxisUpdate(int axis, float value) 
    { 
        (void)axis; 
        (void)value; 
        return false; 
    }

    [[nodiscard]] FDeviceManager* GetDeviceManager() const { return m_DeviceManager; }
    // Note: GetDevice() and GetFrameIndex() are defined in IRenderPass.cpp
    // because they require FDeviceManager to be fully defined
    [[nodiscard]] nvrhi::IDevice* GetDevice() const;
    [[nodiscard]] uint32_t GetFrameIndex() const;

private:
    FDeviceManager* m_DeviceManager = nullptr;
};
