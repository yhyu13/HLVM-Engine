/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Global.h"

/**
 * Default to timer reset on start up
 */
FTimer GlobalTimerFromStart{ true };

/**
 * Default to empty string
 */
FString GExecutableName{};

#if HLVM_ALLOW_GPERF
/**
 * Default to false
 */
bool GGperfEnabled = false;
#endif

/**
 * Default to -1 so that any verbosity is allowed
 */
int GLogVerbosity = -1;

/**
 * Default to variable map
 */
boost::program_options::variables_map GVariableMap;
