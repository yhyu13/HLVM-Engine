/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "Core/Assert.h"
#include "Core/Object/RefCountPtr.h"
#include "RHI/DynamicRHI.h"

DECLARE_LOG_CATEGORY(LogVulkan)

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan_core.h>
#include <vulkan/vk_enum_string_helper.h>

#define VULKAN_API_VERSION VK_API_VERSION_1_3

// vulkan feature definition start---------------------------------------------------------------------------------------------------------------------------------------

/// @brief Since we used glfw, no need to use VK display api (unless we want to take over window management)
#define VULKAN_DISPLAY_KHR (VK_KHR_display && 0)

/// @brief Enable Vulkan renderpass 2 (vk api not supported in libvulkan.so.1 on linux platform)
#define VULKAN_RENDERPASS2 (VK_KHR_create_renderpass2 && 0)

/// @brief Use VMA for memory management
#define VULKAN_USE_VMA 1
#if VULKAN_USE_VMA
	// https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/quick_start.html#quick_start_initialization
	// As we load vk api dynamically, we disable static and dynamic function definitions inside vma and use our own
	#define VMA_STATIC_VULKAN_FUNCTIONS 0
	#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#endif

// vulkan option definition start---------------------------------------------------------------------------------------------------------------------------------------

/// @brief Enable validation layers
#define VULKAN_ENABLE_VALIDATION_LAYERS !HLVM_BUILD_RELEASE

/// @brief whether to keep old swapchain when recreating it, might not work on android platform
#define VULKAN_SWAPCHAIN_KEEP_OLD 1

/// @brief whether to use image fence for swapchain, might not work on android platform
#define VULKAN_SWAPCHAIN_USE_IMAGE_FENCE 1

/// @brief whether to use input attachment shader read, might fix texture flickering on some device
#define VULKAN_INPUT_ATTACHMENT_SHADER_READ 0

// vulkan helper definition start---------------------------------------------------------------------------------------------------------------------------------------

/// @brief Helper macro to convert VkResult to TCHAR string
#define VULKAN_RESULT_TO_TCHAR(x) TO_TCHAR_CSTR(string_VkResult(x))

/// @brief Helper macro to convert VkFormat to TCHAR string
#define VULKAN_FORMAT_TO_TCHAR(x) TO_TCHAR_CSTR(string_VkFormat(x))

#define VULKAN_TYPE_TO_FSTRING(Type, Value) FString::Format(TXT("{} {}"), TXT("Type"), S_C(TUINT32, Value))
#define VULKAN_FLAGS_TO_FSTRING(Type, Value) FString::Format(TXT("{} {}"), TXT("Type"), S_C(TUINT32, Value))

/// @brief Helper macro to test the result of Vulkan calls which can return an error. (HLVM_ENSURE_F)
#define VULKAN_ENSURE(x)                                                                                                                \
	do                                                                                                                                  \
	{                                                                                                                                   \
		VkResult _result = (x);                                                                                                         \
		HLVM_ENSURE_F(_result == VK_SUCCESS, TXT("Vulkan call {} failed with error: {}"), STRTIFY(x), VULKAN_RESULT_TO_TCHAR(_result)); \
	}                                                                                                                                   \
	while (0)

/// @brief Helper macro to test the result of Vulkan calls which can return an error. (HLVM_ASSERT_F)
#define VULKAN_ASSERT(x)                                                                                                                \
	do                                                                                                                                  \
	{                                                                                                                                   \
		VkResult _result = (x);                                                                                                         \
		HLVM_ASSERT_F(_result == VK_SUCCESS, TXT("Vulkan call {} failed with error: {}"), STRTIFY(x), VULKAN_RESULT_TO_TCHAR(_result)); \
	}                                                                                                                                   \
	while (0)

namespace VulkanRHI
{
	template <class T>
	void ZeroVulkanStruct(T* Struct, TINT32 VkStructureType)
	{
		static_assert(!std::is_pointer_v<T>, "Don't use a pointer!");
		static_assert(STRUCT_OFFSET(T, sType) == 0, "Assumes sType is the first member in the Vulkan type!");
		static_assert(sizeof(T::sType) == sizeof(TINT32), "Assumed sType is compatible with int32!");
		// Horrible way to coerce the compiler to not have to know what T::sType is so we can have this header not have to include vulkan.h
		TINT32* Type = R_C(TINT32*, &Struct->sType);
		*Type = VkStructureType;
		FMemory::Memzero(R_C(TUINT8*, Struct) + sizeof(TINT32), sizeof(T) - sizeof(TINT32));
	};
}

DECLARE_LOG_CATEGORY(LogVulkanRHI)
