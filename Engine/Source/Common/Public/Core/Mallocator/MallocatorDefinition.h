/**
 * Copyright (c) 2026. MIT License. All rights reserved.
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

/**
 * Use mimalloc2 over mimalloc for default allocator.
 * Mimalloc2 is our own implementation of mimalloc which is header only.
 * Public/Core/Mallocator/Mi/MiMalloc.h
 *
 * Problem1 : internal stack allocator static init order after some static variables in the program
 * Problem2 : thread local GMiMallocator2TLS freed before some static shared ptr freed in the end of the program
 */
#ifndef HLVM_MALLOC_USE_MIMALLOC2_OVER_STD
	#define HLVM_MALLOC_USE_MIMALLOC2_OVER_STD (0 && !HLVM_MALLOC_USE_MIMALLOC_OVER_STD)
#endif
