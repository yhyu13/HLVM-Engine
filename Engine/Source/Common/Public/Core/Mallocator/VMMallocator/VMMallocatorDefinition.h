/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "Core/Mallocator/MallocatorDefinition.h"

#ifndef HLVM_VMA_SMALL_ALLOC_ALIGNMENT
	#define HLVM_VMA_SMALL_ALLOC_ALIGNMENT (16) // Every binned allocator manage a multiplier of alignment
#endif
static_assert((HLVM_VMA_SMALL_ALLOC_ALIGNMENT & (HLVM_VMA_SMALL_ALLOC_ALIGNMENT - 1)) == 0, "Small alloc alignment must be power of 2");

#ifndef HLVM_VMA_SMALL_ALLOC_THRESHOLD
	#define HLVM_VMA_SMALL_ALLOC_THRESHOLD (240) // Must be <= 256 - HLVM_VMA_SMALL_ALLOC_ALIGNMENT
#endif
static_assert(HLVM_VMA_SMALL_ALLOC_THRESHOLD <= 256 - HLVM_VMA_SMALL_ALLOC_ALIGNMENT
		&& HLVM_VMA_SMALL_ALLOC_THRESHOLD % HLVM_VMA_SMALL_ALLOC_ALIGNMENT == 0,
	"Small alloc threshold must be <= 256 - HLVM_VMA_SMALL_ALLOC_ALIGNMENT and must be a multiple of HLVM_VMA_SMALL_ALLOC_ALIGNMENT");

#ifndef HLVM_VMA_SMALL_HEAP_SIZE
	#define HLVM_VMA_SMALL_HEAP_SIZE (1 << 11) // Must be power of 2
#endif
static_assert((HLVM_VMA_SMALL_HEAP_SIZE & (HLVM_VMA_SMALL_HEAP_SIZE - 1)) == 0, "Small heap size must be power of 2");

#ifndef HLVM_VMA_LARGE_HEAP_SIZE
	#define HLVM_VMA_LARGE_HEAP_SIZE (1 << 25) // Must be power of 2
#endif
static_assert((HLVM_VMA_LARGE_HEAP_SIZE & (HLVM_VMA_LARGE_HEAP_SIZE - 1)) == 0, "Default heap size must be power of 2");

#ifndef HLVM_VMA_GENERIC_PENDING_FREE_LIST_SIZE
	#define HLVM_VMA_GENERIC_PENDING_FREE_LIST_SIZE 64 // Set to 1 to disable pending free list
#endif
static_assert(HLVM_VMA_GENERIC_PENDING_FREE_LIST_SIZE < 255, "Generic pending free list size better be < 255 to avoid gc panic");

#ifndef HLVM_VMA_LOCAL_PENDING_FREE_LIST_SIZE
	#define HLVM_VMA_LOCAL_PENDING_FREE_LIST_SIZE 64 // Set to 1 to disable pending free list
#endif
static_assert(HLVM_VMA_LOCAL_PENDING_FREE_LIST_SIZE < 255, "Local pending free list size better be < 255 to avoid gc panic");

#ifndef HLVM_VMA_NONLOCAL_PENDING_FREE_LIST_SIZE
	#define HLVM_VMA_NONLOCAL_PENDING_FREE_LIST_SIZE 64 // Set to 1 to disable pending free list
#endif
static_assert(HLVM_VMA_NONLOCAL_PENDING_FREE_LIST_SIZE < 255, "Non-local pending free list size better be < 255 to avoid gc panic");
