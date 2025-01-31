/**
* Copyright (c) 2025. MIT License. All rights reserved.
 */

#include "Window/Vulkan/GLFW3Vulkan.h"

VkSurfaceKHR GLFW3Vulkan::CreateSurface(VkInstance instance)
{
	if (instance == VK_NULL_HANDLE || !Window)
	{
		return VK_NULL_HANDLE;
	}

	VkSurfaceKHR surface;
#if !HLVM_BUILD_RELEASE
	// If enable vulkan validation, we should use VkAllocationCallbacks in the 3rd argument
	VkResult errCode = glfwCreateWindowSurface(instance, Window, nullptr, &surface);
#else
	VkResult errCode = glfwCreateWindowSurface(instance, Window, nullptr, &surface);
#endif

	if (errCode != VK_SUCCESS)
	{
		return nullptr;
	}
	return surface;
}

bool GLFW3Vulkan::GetDisplayPresentInfo(VkDisplayPresentInfoKHR* info, std::uint32_t src_width, std::uint32_t src_height) const
{
	// Only stretch mode needs to supply a VkDisplayPresentInfoKHR
	if (Properties.Mode != IWindow::EMode::FullscreenStretch || !info)
	{
		return false;
	}

	info->sType      = VK_STRUCTURE_TYPE_DISPLAY_PRESENT_INFO_KHR;
	info->pNext      = nullptr;
	info->srcRect    = {{0, 0}, {src_width, src_height}};
	info->dstRect    = {{0, 0}, {Display.FullExtent.Width,
									Display.FullExtent.Height}};
	info->persistent = false;

	return true;
}
