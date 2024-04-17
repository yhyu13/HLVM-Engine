/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

/**
 * Inspired by Flax Engine :
 * https://github.com/yhyu13/FlaxEngine/blob/b2f9da4113a7c80586ce3c0e7a916a40f0c63f04/Source/Engine/Profiler/
 * https://flaxengine.com/licensing
 */

#ifndef HLVM_COMPILE_WITH_PROFILER
	#define HLVM_COMPILE_WITH_PROFILER !HLVM_SHIPPING
#endif

/**
 * Macro that use tracy code
 */
#ifndef HLVM_PROFILER_USE_TRACY
	#define HLVM_PROFILER_USE_TRACY 1 && HLVM_COMPILE_WITH_PROFILER
#endif
#if HLVM_PROFILER_USE_TRACY && !HLVM_COMPILE_WITH_PROFILER
	#error "HLVM_PROFILER_USE_TRACY is enabled but HLVM_COMPILE_WITH_PROFILER is disabled"
#endif

#if HLVM_PROFILER_USE_TRACY
/**
 * Global var to turn tracy code on/off in runtime
 */
HLVM_INLINE_VAR bool GbTracyEnabled = false;
#endif

// TODO : integrate minitrace with profiler interface
/**
 * Macro that use minitrace code
 */
#ifndef HLVM_PROFILER_USE_MINITRACE
	#define HLVM_PROFILER_USE_MINITRACE 0 && HLVM_COMPILE_WITH_PROFILER
#endif
#if HLVM_PROFILER_USE_MINITRACE && !HLVM_COMPILE_WITH_PROFILER
	#error "HLVM_PROFILER_USE_MINITRACE is enabled but HLVM_COMPILE_WITH_PROFILER is disabled"
#endif

#if HLVM_PROFILER_USE_MINITRACE
/**
 * Global var to turn minitrace code on/off in runtime
 */
HLVM_INLINE_VAR bool GbMiniTracEnabled = false;
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
