/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "GenericPlatform.h"
#include "Core/String.h"

#ifndef HLVM_ALLOW_DEBUGGER_EVEN_IN_RELEASE
	#define HLVM_ALLOW_DEBUGGER_EVEN_IN_RELEASE 0
#endif

class FGenericPlatformDebuggerUtil
{
public:
	NOCOPYMOVE(FGenericPlatformDebuggerUtil)
	FGenericPlatformDebuggerUtil() = default;
	virtual ~FGenericPlatformDebuggerUtil() = default;

	/**
	 * Generic Platform method that check if any debugger is attached, might be slow depdending on implementation
	 */
	HLVM_INLINE_FUNC static bool IsDebuggerPresent()
	{
		return sInstance->InternalIsDebuggerPresent();
	}

	/**
	 * Generic Platform method that get the stack trace string
	 * @param skip number of frame to skip, counting from bottom
	 * @param max_depth  max number of frame to get
	 * @return FStdString of the stack trace
	 */
	HLVM_NOINLINE_FUNC static FStdString GetStackTrace(size_t skip = 0, size_t max_depth = 10);

protected:
	virtual bool InternalIsDebuggerPresent() = 0;

	HLVM_STATIC_VAR FGenericPlatformDebuggerUtil* sInstance;
};

#define HLVM_IS_DEBUGGER_PRESENT() FGenericPlatformDebuggerUtil::IsDebuggerPresent()
#define HLVM_TRY_DEBUG_BREAK()          \
	do                                  \
	{                                   \
		if (HLVM_IS_DEBUGGER_PRESENT()) \
		{                               \
			HLVM_DEBUG_BREAK();         \
		}                               \
	}                                   \
	while (0)
