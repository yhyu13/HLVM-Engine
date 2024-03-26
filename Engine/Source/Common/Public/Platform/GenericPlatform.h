/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "PlatformDefinition.h"

#include "Core/Log.h"

DECLARE_LOG_CATEGORY(LogGenericPlatform)

#if defined(PLATFORM_WINDOWS)
DECLARE_LOG_CATEGORY(LogWindowsPlatform)
#elif defined(PLATFORM_LINUXGNU)
DECLARE_LOG_CATEGORY(LogLinuxGNUPlatform)
#else
	#error "Not implemented for uknown platform"
#endif
