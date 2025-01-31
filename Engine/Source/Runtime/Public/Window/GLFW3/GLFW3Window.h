/**
* Copyright (c) 2025. MIT License. All rights reserved.
*/

#pragma once

#include "Window/IWindow.h"

#if HLVM_WINDOW_USE_VULKAN
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
#include <GLFW/glfw3.h>
#pragma clang diagnostic pop

class GLFW3Window : public IWindow
{
public:
	NOCOPYMOVE(GLFW3Window)

	GLFW3Window(const FProperties& InProperties);
	virtual ~GLFW3Window() override;

	bool ShouldClose() override;
	void ProcessEvents() override;
	void Close() override;

	TFP32 GetDPIScaleFactor() const override;
	TFP32 GetContentScaleFactor() const override;

protected:
	GLFWwindow* Window;
};
