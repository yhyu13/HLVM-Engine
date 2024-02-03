/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "GenericPlatform.h"

#ifndef HLVM_ALLOW_DEBUGGER_EVEN_IN_RELEASE
	#define HLVM_ALLOW_DEBUGGER_EVEN_IN_RELEASE 0
#endif

class GenericPlatformDebuggerUtil
{
public:
	virtual ~GenericPlatformDebuggerUtil() = default;

	static bool IsDebuggerPresent()
	{
		return s_instance->IsDebuggerPresentInternal();
	}

protected:
	virtual bool IsDebuggerPresentInternal() = 0;

	static std::unique_ptr<GenericPlatformDebuggerUtil> s_instance;
};

#define HLVM_IS_DEBUGGER_PRESENT() GenericPlatformDebuggerUtil::IsDebuggerPresent()
#define HLVM_TRY_DEBUG_BREAK()          \
	do                                  \
	{                                   \
		if (HLVM_IS_DEBUGGER_PRESENT()) \
		{                               \
			HLVM_DEBUG_BREAK();         \
		}                               \
	}                                   \
	while (0)