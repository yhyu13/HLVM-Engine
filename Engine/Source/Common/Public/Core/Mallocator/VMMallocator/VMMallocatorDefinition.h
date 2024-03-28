/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "Core/Mallocator/MallocatorDefinition.h"

#ifndef HLVM_SMALL_ALLOC_THRESHOLD
	#define HLVM_SMALL_ALLOC_THRESHOLD 240 // Must be smaller than 256
#endif

#ifndef HLVM_SMALL_ALLOC_ALIGNMENT
	#define HLVM_SMALL_ALLOC_ALIGNMENT 16 // Every binned allocator manage a multiplier of alignment
#endif
