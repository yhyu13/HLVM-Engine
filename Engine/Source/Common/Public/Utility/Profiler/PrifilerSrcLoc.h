/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

/**
 * Inspired by Flax Engine :
 * https://github.com/yhyu13/FlaxEngine/blob/b2f9da4113a7c80586ce3c0e7a916a40f0c63f04/Source/Engine/Profiler/
 * https://flaxengine.com/licensing
 */

#include "Utility/Profiler/ProfilerDefinition.h"

#if HLVM_PROFILER_USE_TRACY
	#include <common/TracySystem.hpp>
#endif

struct FProfilerSrcLoc
{
	const TCHAR* Name;
	const TCHAR* Function;
	const TCHAR* File;
	TUINT32		 Line;

#if HLVM_PROFILER_USE_TRACY
	/**
	 * Convert to tracy::SourceLocationData when tracy is used
	 */
	operator tracy::SourceLocationData()
	{
		return tracy::SourceLocationData{
			.name = TO_CHAR_STR(Name),
			.function = TO_CHAR_STR(Function),
			.file = TO_CHAR_STR(File),
			.line = Line,
			.color = 0
		};
	}
#endif
};
