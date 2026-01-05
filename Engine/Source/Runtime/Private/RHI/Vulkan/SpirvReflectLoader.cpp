/**
* Copyright (c) 2025. MIT License. All rights reserved.
*/

#pragma once

#include "RHI/Vulkan/VulkanDefinition.h"

#if VULKAN_USE_SPIRV_REFLECT
#define SPIRV_REFLECT_ENABLE_ASSERTS
///usr/include/spirv-reflect/spirv_reflect.c
	#include <spirv-reflect/spirv_reflect.c>
#endif
