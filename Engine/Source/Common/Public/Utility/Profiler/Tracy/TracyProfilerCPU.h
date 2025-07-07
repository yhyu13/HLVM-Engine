/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

/**
 * Inspired by Flax Engine :
 * https://github.com/yhyu13/FlaxEngine/blob/b2f9da4113a7c80586ce3c0e7a916a40f0c63f04/Source/Engine/Profiler/
 * https://flaxengine.com/licensing
 */

#include "TracyDefinition.h"

#if HLVM_COMPILE_WITH_PROFILER
	#if HLVM_PROFILER_USE_TRACY

		#include "Utility/Profiler/ProfilerSrcLoc.h"

		// Undef empty macros for other profiler and instead define new ones for tracy
		#undef HLVM_PROFILE_CPU
		#undef HLVM_PROFILE_CPU_NAMED
		#undef HLVM_PROFILE_CPU_SRC_LOC
		#undef HLVM_PROFILE_CPU_NO_TRACK_EVENT
		#undef HLVM_PROFILE_CPU_NAMED_NO_TRACK_EVENT
		#undef HLVM_PROFILE_CPU_SRC_LOC_NO_TRACK_EVENT

		#if HLVM_PROFILER_USE_TRANSIENT_SOURCE_LOCATION
			#define HLVM_PROFILE_CPU_NO_TRACK_EVENT() \
				ZoneTransient(___tracy_scoped_zone, true)
			#define HLVM_PROFILE_CPU_NAMED_NO_TRACK_EVENT(name) \
				ZoneTransientN(___tracy_scoped_zone, name, true)
		#else
			#define HLVM_PROFILE_CPU_NO_TRACK_EVENT() \
				ZoneNamed(___tracy_scoped_zone, GbTracyEnabled)
			#define HLVM_PROFILE_CPU_NAMED_NO_TRACK_EVENT(name) \
				ZoneNamedN(___tracy_scoped_zone, name, GbTracyEnabled)
		#endif

		#define HLVM_PROFILE_CPU()             \
			HLVM_PROFILE_CPU_NO_TRACK_EVENT(); \
			FScopeEventCPU ProfileBlockCPU(__FUNCTION__)
		#define HLVM_PROFILE_CPU_NAMED(name)             \
			HLVM_PROFILE_CPU_NAMED_NO_TRACK_EVENT(name); \
			FScopeEventCPU ProfileBlockCPU(name)

		#define HLVM_PROFILE_CPU_SRC_LOC_NO_TRACK_EVENT(srcLoc) \
			tracy::ScopedZone ___tracy_scoped_zone(static_cast<tracy::SourceLocationData>(srcLoc), GbTracyEnabled)
		#define HLVM_PROFILE_CPU_SRC_LOC(srcLoc)             \
			HLVM_PROFILE_CPU_SRC_LOC_NO_TRACK_EVENT(srcLoc); \
			FScopeEventCPU ProfileBlockCPU((srcLoc).name)

	#endif
#endif
