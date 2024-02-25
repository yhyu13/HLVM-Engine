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
HLVM_TLS_VAR HLVM_INLINE_VAR const std::thread::id HLVM_CURRENT_THREAD_ID = std::this_thread::get_id();
