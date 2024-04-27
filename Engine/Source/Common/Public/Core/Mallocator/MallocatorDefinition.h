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

// TODO : Finish macro switch between mimallocatorTLS and stdMallocator
#ifndef HLVM_MALLOC_USE_MIMALLOC_OVER_STD
	#define HLVM_MALLOC_USE_MIMALLOC_OVER_STD 1
#endif
