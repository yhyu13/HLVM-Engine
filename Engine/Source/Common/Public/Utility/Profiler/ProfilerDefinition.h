/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#pragma once

/**
 * Inspired by Flax Engine :
 * https://github.com/yhyu13/FlaxEngine/blob/b2f9da4113a7c80586ce3c0e7a916a40f0c63f04/Source/Engine/Profiler/
 * https://flaxengine.com/licensing
 */

#ifndef HLVM_PROFILER_COMPILE
	#define HLVM_PROFILER_COMPILE (1 && !HLVM_SHIPPING_CODE)
#endif

#if HLVM_PROFILER_COMPILE
	#define HLVM_PROFILER_EXEC(...) __VA_ARGS__
#else
	#define HLVM_PROFILER_EXEC(...) void(0)
#endif

/**
 * Macro that use tracy code
 */
#ifndef HLVM_PROFILER_USE_TRACY
	#define HLVM_PROFILER_USE_TRACY (1 && HLVM_PROFILER_COMPILE)
#endif
#if HLVM_PROFILER_USE_TRACY && !HLVM_PROFILER_COMPILE
	#error "HLVM_PROFILER_USE_TRACY is enabled but HLVM_PROFILER_COMPILE is disabled"
#endif

#if HLVM_PROFILER_USE_TRACY
/**
 * Global var to turn tracy code on/off in runtime
 */
HLVM_INLINE_VAR bool GbTracyEnabled = true;
#endif

// TODO : integrate minitrace with profiler interface
/**
 * Macro that use minitrace code
 */
#ifndef HLVM_PROFILER_USE_MINITRACE
	#define HLVM_PROFILER_USE_MINITRACE (0 && HLVM_PROFILER_COMPILE)
#endif
#if HLVM_PROFILER_USE_MINITRACE && !HLVM_PROFILER_COMPILE
	#error "HLVM_PROFILER_USE_MINITRACE is enabled but HLVM_PROFILER_COMPILE is disabled"
#endif

#if HLVM_PROFILER_USE_MINITRACE
/**
 * Global var to turn minitrace code on/off in runtime
 */
HLVM_INLINE_VAR bool GbMiniTracEnabled = true;
#endif

#if HLVM_PROFILER_USE_MINITRACE + HLVM_PROFILER_USE_TRACY > 1
	#error "HLVM_PROFILER_USE_MINITRACE + HLVM_PROFILER_USE_TRACY > 1, but only one profiler backend can exists"
#endif

// Shortcut macros for profiling a single code block execution on CPU
// Use ZoneTransient for Tracy for code that can be hot-reloaded (eg. in Editor) or if name can be a variable
// Transient source location simply pass in source location data as a parameter but not set them as static constexpr variable
// By not using transient, we can mediate profiler performance impact
#ifndef HLVM_PROFILER_USE_TRANSIENT_SOURCE_LOCATION
	#define HLVM_PROFILER_USE_TRANSIENT_SOURCE_LOCATION 0
#endif
