/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "Utility/Profiler/ProfilerDefinition.h"

#if HLVM_PROFILER_USE_TRACY
	/**
	 * Force Enable callstack
	 */
	#define TRACY_CALLSTACK 8

	#if defined(__clang__) || defined(__GNUC__)
		#define TracyFunction __PRETTY_FUNCTION__
	#elif defined(_MSC_VER)
		#define TracyFunction __FUNCSIG__
	#endif

	#include <tracy/Tracy.hpp>
	#include <tracy/TracyC.h>

#endif
