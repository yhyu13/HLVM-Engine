/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "GlobalDefinition.h"
#include "Platform/PlatformDefinition.h"

#include <atomic>
#define ATOMIC_THREAD_FENCE() std::atomic_thread_fence(std::memory_order_acq_rel)

/**
 * thread local storage declared here
 */
HLVM_TLS_VAR HLVM_INLINE_VAR const std::thread::id GCurrentThreadID = std::this_thread::get_id();
static_assert(sizeof(std::thread::id) == sizeof(uint64_t), "std::thread::id is not 8 bytes!");

/**
 * Enable/Disable thread debug utilities
 */
#ifndef HLVM_DEBUG_THREAD_UTIL
	#define HLVM_DEBUG_THREAD_UTIL 0 // HLVM_BUILD_DEBUG
#endif
