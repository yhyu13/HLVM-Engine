/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "Core/Assert.h"
#include "Renderer/RHI/RHIDefinition.h"

#ifndef VK_NO_PROTOTYPE
	#define VK_NO_PROTOTYPE
#endif

#if defined(WIN32) || defined(_WIN32) || defined(_WIN32_) || defined(WIN64) || defined(_WIN64) || defined(_WIN64_)
	#define VULKAN_LIB "vulkan-1.dll"
#elif defined(ANDROID) || defined(_ANDROID_)
	#define VULKAN_LIB "libvulkan.so"
#else
	#define VULKAN_LIB "libvulkan.so.1"
#endif

// First load vulkan hpp with dynamic dispatch (aka VK_NO_PROTOTYPE)
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
static_assert(VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1, "VULKAN_HPP_DISPATCH_LOADER_DYNAMIC must be defined to 1");

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch-enum"
#include <vulkan/vk_enum_string_helper.h>
#pragma clang diagnostic pop

// load nvrhi after vulkan stuff
#include <nvrhi/vulkan.h>
#include <nvrhi/validation.h>

// vulkan feature definition start---------------------------------------------------------------------------------------------------------------------------------------

/// @brief Use VMA for memory management
#define VULKAN_USE_VMA 1

// vulkan option definition start---------------------------------------------------------------------------------------------------------------------------------------

/// @brief Enable validation layers
#define VULKAN_ENABLE_VALIDATION_LAYERS !HLVM_BUILD_RELEASE

// vulkan helper definition start---------------------------------------------------------------------------------------------------------------------------------------

/// @brief Helper macro to convert VkResult to TCHAR string
#define VULKAN_RESULT_TO_TCHAR(x) TO_TCHAR_CSTR(string_VkResult(x))

/// @brief Helper macro to convert VkFormat to TCHAR string
#define VULKAN_FORMAT_TO_TCHAR(x) TO_TCHAR_CSTR(string_VkFormat(x))

#define VULKAN_TYPE_TO_FSTRING(_Type, _Value) FString::Format(TXT("{} {}"), STRTIFY(_Type), S_C(TUINT32, _Value))
#define VULKAN_FLAGS_TO_FSTRING(_Type, _Value) FString::Format(TXT("{} {}"), STRTIFY(_Type), S_C(TUINT32, _Value))
#define VULKAN_ENUM_TO_FSTRING(x) FString(magic_enum::enum_name(x).data())

/// @brief Helper macro to test the result of Vulkan calls which can return an error. (HLVM_ENSURE_F)
#define VULKAN_ENSURE(x)                                                                                                       \
	do                                                                                                                         \
	{                                                                                                                          \
		VkResult _result = (x);                                                                                                \
		HLVM_ENSURE_F(_result == VK_SUCCESS, TXT("Vulkan ensure {} failed: {}"), STRTIFY(x), VULKAN_RESULT_TO_TCHAR(_result)); \
	}                                                                                                                          \
	while (0)

/// @brief Helper macro to test the result of Vulkan calls which can return an error. (HLVM_ASSERT_F)
#define VULKAN_ASSERT(x)                                                                                                       \
	do                                                                                                                         \
	{                                                                                                                          \
		VkResult _result = (x);                                                                                                \
		HLVM_ASSERT_F(_result == VK_SUCCESS, TXT("Vulkan assert {} failed: {}"), STRTIFY(x), VULKAN_RESULT_TO_TCHAR(_result)); \
	}                                                                                                                          \
	while (0)
