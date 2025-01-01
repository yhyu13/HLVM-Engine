/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "GenericPlatform.h"
#include "Core/String.h"

class FGenericPlatformDebuggerUtil
{
public:
	NOCOPYMOVE(FGenericPlatformDebuggerUtil)
	FGenericPlatformDebuggerUtil() = default;
	virtual ~FGenericPlatformDebuggerUtil() = default;

	/**
	 * Generic Platform method that check if any debugger is attached, might be slow depdending on implementation
	 */
	HLVM_INLINE_FUNC HLVM_STATIC_FUNC bool IsDebuggerPresent()
	{
		return sInstance->InternalIsDebuggerPresent();
	}

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
