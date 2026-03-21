/**
* Copyright (c) 2026. MIT License. All rights reserved.
*/

#pragma once

#include "Renderer/RHI/_Deprecated/Vulkan/VulkanDefinition.h"

#if VULKAN_USE_SPIRV_REFLECT
#define SPIRV_REFLECT_ENABLE_ASSERTS
///usr/include/spirv-reflect/spirv_reflect.c
	#include <spirv-reflect/spirv_reflect.c>
#endif
