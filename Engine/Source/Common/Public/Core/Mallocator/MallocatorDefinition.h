/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

/**
 * Override new and delete operator
 */
#ifndef HLVM_MALLOC_OVERRIDE
	#define HLVM_MALLOC_OVERRIDE 1
#endif

/**
 * Use stack allocator as general propose allocator
 * CAUTION : not recommanded, as long life time object e.g. share ptr counter, could lead to crash on free
 * turn off by default
 */
#ifndef HLVM_MALLOC_USE_GENERAL_PURPOSE_STACK_ALLOCATOR
	#define HLVM_MALLOC_USE_GENERAL_PURPOSE_STACK_ALLOCATOR 0
#endif
