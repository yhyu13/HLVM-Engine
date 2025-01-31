/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "RHI/Vulkan/VulkanLoader.h"
#include "Window/GLFW3/GLFW3Window.h"

class GLFW3Vulkan final : public GLFW3Window
{
private:
	// Local structure for holding display candidate information
	struct FDisplayCandidate
	{
		VkDisplayKHR				  display;
		VkDisplayPropertiesKHR		  display_props;
		VkDisplayModePropertiesKHR	  mode;
		VkDisplayPlaneCapabilitiesKHR caps;
		uint32_t					  plane_index;
		uint32_t					  stack_index;
		FExtent						  FullExtent;
	};

public:
	NOCOPYMOVE(GLFW3Vulkan)

	GLFW3Vulkan(const FProperties& InProperties)
		: GLFW3Window(InProperties)
	{
		VulkanLoader::LoadOnce();
		Type = EWindowType::GLFW3Vulkan;
	}

	/**
	 * @brief Gets a handle from the platform's Vulkan surface
	 * @param instance A Vulkan instance
	 * @returns A VkSurfaceKHR handle, for use by the application
	 */
	VkSurfaceKHR CreateSurface(VkInstance instance);

	/**
	 * @brief Get the display present info for the window if needed
	 *
	 * @param info Filled in when the method returns true
	 * @param src_width The width of the surface being presented
	 * @param src_height The height of the surface being presented
	 * @return true if the present info was filled in and should be used
	 * @return false if the extra present info should not be used. info is left untouched.
	 */
	virtual bool GetDisplayPresentInfo(VkDisplayPresentInfoKHR* info,
		TUINT32 src_width, TUINT32 src_height) const;

private:
	FDisplayCandidate Display;
};
