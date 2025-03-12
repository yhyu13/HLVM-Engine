/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "Core/Assert.h"
#include "Core/Object/RefCountPtr.h"

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan_core.h>
#include <vulkan/vk_enum_string_helper.h>

#define VULKAN_API_VERSION VK_API_VERSION_1_3

/// @brief Since we used glfw, no need to use VK display api (unless we want to take over window management)
#define VULKAN_USE_DISPLAY_KHR (VK_KHR_display && 0)

/// @brief Use VMA for memory management
#define VULKAN_USE_VMA 1
#if VULKAN_USE_VMA
	// https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/quick_start.html#quick_start_initialization
	// As we load vk api dynamically, we disable static and dynamic function definitions inside vma and use our own
	#define VMA_STATIC_VULKAN_FUNCTIONS 0
	#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#endif

/// @brief Enable validation layers
#define VULKAN_ENABLE_VALIDATION_LAYERS !HLVM_BUILD_RELEASE

/// @brief Helper macro to convert VkResult to TCHAR string
#define VULKAN_RESULT_TO_TCHAR(x) TO_TCHAR_CSTR(string_VkResult(x))

/// @brief Helper macro to test the result of Vulkan calls which can return an error. (HLVM_ENSURE_F)
#define VULKAN_ENSURE(x)                                                                                                                  \
	do                                                                                                                                \
	{                                                                                                                                 \
		VkResult _result = (x);                                                                                                       \
		HLVM_ENSURE_F(_result == VK_SUCCESS, TXT("Vulkan call {} failed with error: {}"), STRTIFY(x), VULKAN_RESULT_TO_TCHAR(_result)); \
	}                                                                                                                                 \
	while (0)

/// @brief Helper macro to test the result of Vulkan calls which can return an error. (HLVM_ASSERT_F)
#define VULKAN_ASSERT(x)                                                                                                                  \
	do                                                                                                                                \
	{                                                                                                                                 \
		VkResult _result = (x);                                                                                                       \
		HLVM_ASSERT_F(_result == VK_SUCCESS, TXT("Vulkan call {} failed with error: {}"), STRTIFY(x), VULKAN_RESULT_TO_TCHAR(_result)); \
	}                                                                                                                                 \
	while (0)

/// @brief whether to keep old swapchain when recreating it, might not work on android platform
#define VULKAN_SWAPCHAIN_KEEP_OLD 1

/// @brief whether to use image fence for swapchain, might not work on android platform
#define VULKAN_SWAPCHAIN_USE_IMAGE_FENCE 1

class FVulkanTexture;
class FVulkanQueue;
class FVulkanView;
