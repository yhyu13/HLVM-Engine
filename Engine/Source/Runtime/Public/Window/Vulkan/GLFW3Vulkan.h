/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "RHI/Vulkan/VulkanLoader.h"
#include "Window/GLFW3/GLFW3Window.h"
// Only include when HLVM_WINDOW_USE_VULKAN is true
static_assert(HLVM_WINDOW_USE_VULKAN);

class FGLFW3Vulkan final : public FGLFW3Window
{
public:
	NOCOPYMOVE(FGLFW3Vulkan)

	FGLFW3Vulkan() = delete;
	explicit FGLFW3Vulkan(const FProperties& InProperties);
	virtual ~FGLFW3Vulkan() override;

	/**
	 * @brief Gets a handle from the platform's Vulkan surface
	 * @param instance A Vulkan instance
	 * @returns A VkSurfaceKHR handle, for use by the application
	 */
	VkSurfaceKHR CreateSurface(VkInstance instance);
};
