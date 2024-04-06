/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "GlobalDefinition.h"
#include "Platform/PlatformDefinition.h"

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
HLVM_EXTERN_VAR bool GGperfEnabled;
#endif

HLVM_EXTERN_VAR int GVerbosity;
