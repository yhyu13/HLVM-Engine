/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Core/Assert.h"
#include "Core/Log.h"
#include "Platform/GenericPlatformDebuggerUtil.h"

namespace hlvm_private
{
	HLVM_NORETURN HLVM_NOINLINE_FUNC void hlvm_internal_assert(const TCHAR* Expression, FString&& Message, const TCHAR* File, int Line)
	{
		const FStdString& Stack = FGenericPlatformDebuggerUtil::GetStackTrace(); // Explicitly call stack trace here to get proper stack trace
		const FString&	  msg = FString::Format(TXT("{1} with '{2}' at {3}:{4}\n{0}"), *Stack, Expression, Message, File,
			   Line);
		HLVM_LOG(LogAssert, critical, *msg);
		HLVM_TRY_DEBUG_BREAK();
		throw std::runtime_error(msg.ToCharStr());
	}
} // namespace hlvm_private
