/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Global.h"

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
int GVerbosity = -1;
