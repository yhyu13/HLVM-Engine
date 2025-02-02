/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan_core.h>
#include <vulkan/vk_enum_string_helper.h>

/// @brief Since we used glfw, no need to use VK display api (unless we want to take over window management)
#define USE_VK_DISPLAY (VK_KHR_display && 0)

#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#include <vk_mem_alloc.h>

/// @brief Helper macro to test the result of Vulkan calls which can return an error.
#define VK_ENSURE(x)                                                                                \
	do                                                                                             \
	{                                                                                              \
		VkResult _result = (x);                                                                     \
		if (_result != VK_SUCCESS)                                                                  \
		{                                                                                          \
			HLVM_ENSURE(false, TXT("Vulkan call failed with error: {}"), TO_TCHAR_CSTR(string_VkResult(_result))); \
		}                                                                                          \
	}                                                                                              \
	while (0)

