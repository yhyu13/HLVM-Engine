/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "Window/GLFW3/GLFW3Window.h"

// Only include when HLVM_WINDOW_USE_VULKAN is true
static_assert(HLVM_WINDOW_USE_VULKAN);

class FGLFW3VulkanWindow final : public FGLFW3Window
{
public:
	NOCOPYMOVE(FGLFW3VulkanWindow)

	FGLFW3VulkanWindow() = delete;
	explicit FGLFW3VulkanWindow(const Properties& InProperties);
	virtual ~FGLFW3VulkanWindow() override;

	/**
	 * @brief Gets a handle from the platform's Vulkan surface
	 * @param instance A Vulkan instance
	 * @returns A VkSurfaceKHR handle, for use by the application
	 */
	VkSurfaceKHR CreateSurface(VkInstance instance);

	/**
	 * @brief Gets the required extensions for Vulkan
	 * @return A vector of extension names
	 */
	TVector<FString> GetRequiredExtensions() const;
};
