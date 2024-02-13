/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "GlobalDefinition.h"
#include "Platform/PlatformDefinition.h"

#include <atomic>
#define ATOMIC_THREAD_FENCE() std::atomic_thread_fence(std::memory_order_acq_rel)
