/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#include "Renderer/Window/GLFW3/GLFW3VulkanWindow.h"

#if HLVM_WINDOW_USE_VULKAN

FGLFW3VulkanWindow::FGLFW3VulkanWindow(const IWindow::Properties& InProperties)
	: FGLFW3Window(InProperties)
{
	Type = EWindowType::GLFW3Vulkan;

	// 初始化Vulkan加载器
	hlvm_vk::InitVulkanLoaderAPIOnce();

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

	Window = glfwCreateWindow(S_C(int, InProperties.Extent.x),
		S_C(int, InProperties.Extent.y),
		InProperties.Title,
		nullptr,
		nullptr); // 创建窗口
	HLVM_ENSURE_F(Window, TXT("Failed to create GLFW window"));
	HLVM_LOG(LogGLFW3Window, debug, TXT("GLFW3Vulkan Init"));
}

FGLFW3VulkanWindow::~FGLFW3VulkanWindow()
{
	HLVM_LOG(LogGLFW3Window, debug, TXT("GLFW3Vulkan Destroy"));
}

VkSurfaceKHR FGLFW3VulkanWindow::CreateSurface(VkInstance instance)
{
	if (instance == VK_NULL_HANDLE || !Window)
	{
		return VK_NULL_HANDLE;
	}

	VkSurfaceKHR surface;
	VULKAN_ENSURE(glfwCreateWindowSurface(instance, Window, nullptr, &surface));
	return surface;
}

TVector<std::string> FGLFW3VulkanWindow::GetRequiredExtensions()
{
	TUINT32 glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	TVector<std::string> Exts(glfwExtensions, glfwExtensions + glfwExtensionCount);
	return Exts;
}

#endif
