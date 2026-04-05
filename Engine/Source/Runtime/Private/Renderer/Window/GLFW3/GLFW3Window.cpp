/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#include "Renderer/Window/GLFW3/GLFW3Window.h"

FGLFW3Window::FGLFW3Window(const Properties& InProperties)
{
	Property = InProperties;
	HLVM_LOG(LogGLFW3Window, debug, TXT("GLFW3Window Init"));
}

FGLFW3Window::~FGLFW3Window()
{
	HLVM_LOG(LogGLFW3Window, debug, TXT("GLFW3Window Destroy"));
	glfwDestroyWindow(Window); // 销毁窗口
	glfwTerminate();
}

void FGLFW3Window::SetShouldClose()
{
	glfwSetWindowShouldClose(Window, GLFW_TRUE);
}

bool FGLFW3Window::ShouldClose()
{
	return glfwWindowShouldClose(Window);
}

void FGLFW3Window::ProcessEvents()
{
	glfwPollEvents();
}

TFP32 FGLFW3Window::GetDPIScaleFactor() const
{
	auto primary_monitor = glfwGetPrimaryMonitor();
	auto vidmode = glfwGetVideoMode(primary_monitor);

	int width_mm, height_mm;
	glfwGetMonitorPhysicalSize(primary_monitor, &width_mm, &height_mm);

	// As suggested by the GLFW monitor guide
	static const TFP32 inch_to_mm = 25.0f;
	static const TFP32 win_base_density = 96.0f;

	auto dpi = S_C(TUINT32, S_C(TFP32, vidmode->width) / (S_C(TFP32, width_mm) / inch_to_mm));
	auto dpi_factor = S_C(TFP32, dpi) / win_base_density;
	return dpi_factor;
}

TFP32 FGLFW3Window::GetContentScaleFactor() const
{
	int fb_width, fb_height;
	glfwGetFramebufferSize(Window, &fb_width, &fb_height);
	int win_width, win_height;
	glfwGetWindowSize(Window, &win_width, &win_height);

	// We could return a 2D result here instead of a scalar,
	// but non-uniform scaling is very unlikely, and would
	// require significantly more changes in the IMGUI integration
	return S_C(TFP32, fb_width) / S_C(TFP32, win_width);
}

void FGLFW3Window::SetInputCallbacks(GLFWwindow* window)
{
	// Store window for later use if needed
	// Note: GLFW callbacks are registered by FDeviceManagerVk via its own SetInputCallbacks
	(void)window;
}
