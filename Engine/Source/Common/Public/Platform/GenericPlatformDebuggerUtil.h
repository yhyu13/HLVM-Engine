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
	virtual ~FGenericPlatformDebuggerUtil() = default;

	inline static bool IsDebuggerPresent()
	{
		return s_instance->IsDebuggerPresentInternal();
	}

	inline static FCharStringView GetStackTrace()
	{
		backward::StackTrace st;
		st.load_here(32);
		backward::Printer  p;
		std::ostringstream ss;
		p.print(st, ss);
		// TODO: maybe re-implement st load_here with parameter to skip first n frames,
		//  so that we can skip the stack trace of inner backward callings
		return FCharStringView(MoveTemp(ss.str()));
	}

protected:
	virtual bool IsDebuggerPresentInternal() = 0;

	static std::unique_ptr<FGenericPlatformDebuggerUtil> s_instance;
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