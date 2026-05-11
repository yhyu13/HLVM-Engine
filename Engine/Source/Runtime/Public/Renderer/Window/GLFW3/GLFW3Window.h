/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#pragma once

#include "Renderer/Window/IWindow.h"

#if HLVM_WINDOW_USE_VULKAN
	// We have to preload vulkan header before glfw to enable glfw's vulkan api
	#include "Renderer/RHI/Vulkan/VulkanLoader.h"
#endif

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdocumentation"
#pragma clang diagnostic ignored "-Wzero-as-null-pointer-constant"
#pragma clang diagnostic ignored "-Wold-style-cast"
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
#pragma clang diagnostic ignored "-Wmissing-noreturn"
#pragma clang diagnostic ignored "-Wcast-function-type-strict"
#pragma clang diagnostic ignored "-Wunused-parameter"
#define GLFW_INCLUDE_NONE
#if HLVM_WINDOW_USE_VULKAN
	#define GLFW_INCLUDE_VULKAN
#endif
// Do not define GLFW_INCLUDE_VULKAN since we have already included vulkan header above
#include <GLFW/glfw3.h>
#pragma clang diagnostic pop

DECLARE_LOG_CATEGORY(LogGLFW3Window)

/**
 * IGLFWInputCallbacks - Interface for GLFW input event handling
 * Implemented by classes that want to receive GLFW input events.
 */
class IGLFWInputCallbacks
{
public:
	virtual ~IGLFWInputCallbacks() = default;

	// Keyboard events - return true if consumed
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

	// Mouse events - return true if consumed
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

	// Joystick events - return true if consumed
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

	// Set the GLFW window to use for callbacks
	virtual void SetInputCallbacks(GLFWwindow* window) = 0;
};

class FGLFW3Window : public IWindow, public IGLFWInputCallbacks
{
public:
	NOCOPYMOVE(FGLFW3Window);
	FGLFW3Window() = delete;
	explicit FGLFW3Window(const Properties& InProperties);
	virtual ~FGLFW3Window() override;

	virtual bool ShouldClose() override;
	virtual void SetShouldClose() override;
	virtual void SetTitle(const FString& InTitle) override;
	virtual void ProcessEvents() override;

	virtual TFP32 GetDPIScaleFactor() const override;
	virtual TFP32 GetContentScaleFactor() const override;

	// IGLFWInputCallbacks
	virtual void SetInputCallbacks(GLFWwindow* window) override;

	// Native window access for ImGui integration
	GLFWwindow* GetGLFWWindow() const { return Window; }

protected:
	GLFWwindow* Window;
};
