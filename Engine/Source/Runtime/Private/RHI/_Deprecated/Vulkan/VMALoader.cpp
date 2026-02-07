/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

/*
 * Inspired by https://gitee.com/sumcai/MiniVulkanTriangle to load vulkan api during runtime
 */

#include "RHI/_Deprecated/Vulkan/VulkanDefinition.h"

#if VULKAN_USE_VMA
// Reference to setup custom loading single header vma
// https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/quick_start.html#quick_start_initialization
	#define VMA_IMPLEMENTATION
	#include <vk_mem_alloc.h>
#endif
