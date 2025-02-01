/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#include "Window/Vulkan/GLFW3Vulkan.h"

DECLARE_LOG_CATEGORY(LogGLFW3Vulkan)

FGLFW3Vulkan::FGLFW3Vulkan(const IWindow::FProperties& InProperties)
	: FGLFW3Window(InProperties)
{
	HLVM_LOG(LogGLFW3Vulkan, info, TXT("GLFW3Vulkan Init"));
	VulkanLoader::LoadOnce();
	Type = EWindowType::GLFW3Vulkan;

	glfwInit(); // 初始化图形库框架：Graphics Libraries Framework

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // GLFW会默认创建OpenGL的Context，但是，我们使用的是Vulkan，所以需要取消
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);	  // 禁止窗口拉伸，涉及SwapChain重建

	Window = glfwCreateWindow(S_C(int, InProperties.Extent.Width),
		S_C(int, InProperties.Extent.Height),
		InProperties.Title,
		nullptr,
		nullptr); // 创建窗口
}

VkSurfaceKHR FGLFW3Vulkan::CreateSurface(VkInstance instance)
{
	if (instance == VK_NULL_HANDLE || !Window)
	{
		return VK_NULL_HANDLE;
	}

	VkSurfaceKHR surface;
#if !HLVM_BUILD_RELEASE
	// If enable vulkan validation, we should use VkAllocationCallbacks in the 3rd argument
	VK_ENSURE(glfwCreateWindowSurface(instance, Window, nullptr, &surface));
#else
	VK_ENSURE(glfwCreateWindowSurface(instance, Window, nullptr, &surface));
#endif
	return surface;
}
