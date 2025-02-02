/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#include "Window/Vulkan/GLFW3Vulkan.h"

DECLARE_LOG_CATEGORY(LogGLFW3Vulkan)

FGLFW3Vulkan::FGLFW3Vulkan(const IWindow::FProperties& InProperties)
	: FGLFW3Window(InProperties)
{
	HLVM_LOG(LogGLFW3Vulkan, debug, TXT("GLFW3Vulkan Init"));
	Type = EWindowType::GLFW3Vulkan;

	VulkanLoader::LoadOnce();

	glfwInit(); // 初始化图形库框架：Graphics Libraries Framework
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // GLFW会默认创建OpenGL的Context，但是，我们使用的是Vulkan，所以需要取消
	if (InProperties.Resizable)
	{
		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
	}
	else
	{
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); // 禁止窗口拉伸，涉及SwapChain重建
	}

	Window = glfwCreateWindow(S_C(int, InProperties.Extent.Width),
		S_C(int, InProperties.Extent.Height),
		InProperties.Title,
		nullptr,
		nullptr); // 创建窗口
	HLVM_ENSURE(Window, TXT("Failed to create GLFW window"));
}

FGLFW3Vulkan::~FGLFW3Vulkan()
{
	HLVM_LOG(LogGLFW3Vulkan, debug, TXT("GLFW3Vulkan Destroy"));
}

VkSurfaceKHR FGLFW3Vulkan::CreateSurface(VkInstance instance)
{
	if (instance == VK_NULL_HANDLE || !Window)
	{
		return VK_NULL_HANDLE;
	}

	VkSurfaceKHR surface;
	VK_ENSURE(glfwCreateWindowSurface(instance, Window, VkCPUAllocator, &surface));
	return surface;
}
