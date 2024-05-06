/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "GlobalDefinition.h"
#include "Platform/PlatformDefinition.h"

#include <atomic>

#if defined(__x86_64__)
	/**
	 * https://en.cppreference.com/w/cpp/atomic/atomic_thread_fence
	 * On x86 (including x86-64), atomic_thread_fence functions issue no CPU instructions and only affect compile-time code motion, except for std::atomic_thread_fence(std::memory_order::seq_cst).
	 */
	#define HLVM_ATOMIC_THREAD_FENCE() std::atomic_thread_fence(std::memory_order_acq_rel)
#else
	#error "HLVM_ATOMIC_THREAD_FENCE Unsupported architecture! Solve me!"
#endif

/**
 * thread local storage declared here
 */
HLVM_THREAD_LOCAL_VAR HLVM_INLINE_VAR const std::thread::id GCurrentTID = std::this_thread::get_id();
HLVM_THREAD_LOCAL_VAR HLVM_INLINE_VAR const TUINT64			GCurrentTID64 = *R_C(const TUINT64*, &GCurrentTID);
static_assert(sizeof(std::thread::id) == sizeof(uint64_t), "std::thread::id is not 8 bytes!");

/**
 * Enable/Disable thread debug utilities
 */
#ifndef HLVM_DEBUG_THREAD_UTIL
	#define HLVM_DEBUG_THREAD_UTIL 0 // HLVM_BUILD_DEBUG
#endif
