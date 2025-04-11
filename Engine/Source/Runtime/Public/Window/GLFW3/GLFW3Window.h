/**
* Copyright (c) 2025. MIT License. All rights reserved.
*/

#pragma once

#include "Window/IWindow.h"

#if HLVM_WINDOW_USE_VULKAN
	// We have to preload vulkan header before glfw to enable glfw's vulkan api
	#include "RHI/Vulkan/VulkanLoader.h"
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
// Do not define GLFW_INCLUDE_VULKAN since we have already included vulkan header above
#include <GLFW/glfw3.h>
#pragma clang diagnostic pop

class FGLFW3Window : public IWindow
{
public:
	NOCOPYMOVE(FGLFW3Window)
	FGLFW3Window() = delete;
	explicit FGLFW3Window(const Properties& InProperties);
	virtual ~FGLFW3Window() override;

	virtual bool ShouldClose() override;
	virtual void ProcessEvents() override;
	virtual void Close() override;

	virtual TFP32 GetDPIScaleFactor() const override;
	virtual TFP32 GetContentScaleFactor() const override;

protected:
	GLFWwindow* Window;
};
