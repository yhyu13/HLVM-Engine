/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Core/Assert.h"

[[noreturn]] void hlvm_internal_assert(const TCHAR* Expression, FString&& Message, const TCHAR* File, int Line)
{
	const FCharStringView& StackTrace = FGenericPlatformDebuggerUtil::GetStackTrace();
	const FString&		   msg = FString::Format(TXT("{1} with '{2}' at {3}:{4}\n{0}"), *StackTrace, Expression,
				Message, File, Line);
	HLVM_LOG(LogAssert, critical, *msg);
	HLVM_TRY_DEBUG_BREAK();
	throw std::runtime_error(msg.ToCharStr());
}
