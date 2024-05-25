/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "GlobalDefinition.h"
#include "Platform/PlatformDefinition.h"

#ifndef HLVM_MALLOC_VALIDATION
	#define HLVM_MALLOC_VALIDATION !HLVM_BUILD_RELEASE
#endif

#ifndef HLVM_MALLOC_ALIGNMENT
	#define HLVM_MALLOC_ALIGNMENT 8
#endif

/**
 * Use mimalloc over std for default allocator in many cases, check where this flags are used for details!
 * @CAUTION: this flag is not used in HLVM_COMMON_DYNAMIC_LINKED mode
 * bc of heap management in mimalloc library is faulty and it is not safe to use it in dynamic linked mode
 */
#ifndef HLVM_MALLOC_USE_MIMALLOC_OVER_STD
	#define HLVM_MALLOC_USE_MIMALLOC_OVER_STD (1 && !HLVM_COMMON_DYNAMIC_LINKED)
#endif
