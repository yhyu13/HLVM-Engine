/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "GlobalDefinition.h"
#include "Platform/PlatformDefinition.h"

#include "Utility/Timer.h"
/**
 * A global timer that usually query for time elapsed since startup
 */
HLVM_EXTERN_VAR FTimer GlobalTimerFromStart;

/**
 * Executable name
 */
HLVM_EXTERN_VAR FString GExecutableName;

#ifdef PLATFORM_LINUXGNU
	#ifndef HLVM_ALLOW_GPERF
		#define HLVM_ALLOW_GPERF 1
	#endif
#else
	/**
	 * Disable gperf on non-linux platforms
	 */
	#ifdef HLVM_ALLOW_GPERF
		#undef HLVM_ALLOW_GPERF
	#endif
	#define HLVM_ALLOW_GPERF 0
#endif
#if HLVM_ALLOW_GPERF
/**
 * Gperf enabled
 */
HLVM_EXTERN_VAR bool GGperfEnabled;
#endif

/**
 * Log Verbosity level
 */
HLVM_EXTERN_VAR int GLogVerbosity;

#include <boost/program_options.hpp>
/**
 * Variable map that parses cmd line arguments
 */
HLVM_EXTERN_VAR boost::program_options::variables_map GVariableMap;
