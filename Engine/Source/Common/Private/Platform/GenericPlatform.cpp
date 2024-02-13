/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Platform/GenericPlatform.h"

DEFINE_LOG_CATEGORY(LogGenericPlatform)

#if defined(PLATFORM_WINDOWS)
DEFINE_LOG_CATEGORY(LogWindowsPlatform)
#elif defined(PLATFORM_LINUXGNU)
DEFINE_LOG_CATEGORY(LogLinuxGNUPlatform)
#else
	#error "Not implemented for uknown platform"
#endif
