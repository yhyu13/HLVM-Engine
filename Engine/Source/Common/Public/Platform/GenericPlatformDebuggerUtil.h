/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "GenericPlatform.h"
#include "Core/String.h"

#include <backward.hpp>

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
	inline static bool IsDebuggerPresent()
	{
		return sInstance->InternalIsDebuggerPresent();
	}

	/**
	 * Generic Platform method that get the stack trace string
	 * @param skip number of frame to skip, counting from bottom
	 * @return FStdString of the stack trace
	 */
	inline static FStdString GetStackTrace(size_t skip = 0)
	{
		backward::StackTrace st;
		st.load_here(32);
		st.skip_n_firsts(1 + skip); // Skip the first frame of backward to get our frame
		std::ostringstream os;
		backward::Printer  p;
		p.print(st, os);
		return FStdString(MoveTemp(os.str())); // TODO, consider stack string instead of malloc string
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
