/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Core/Assert.h"
#include "Core/Log.h"
#include "Platform/GenericPlatformDebuggerUtil.h"
#include "Core/Mallocator/StackMallocator.h"

namespace hlvm_private
{
	HLVM_STATIC_VAR TStackMallocator<64 * 1024, false, true, false, false, false> StackMallocator;
	IMallocator*																  AssertStackMallocator = &StackMallocator;

	void InitAssertStackMallocator()
	{
		// Reset stack mallocator each time init is called to wap out any previous allocations
		// This is totally valid since allocation last time on assertion is already handled and gone obsolete
		StackMallocator.Reset();
	}

	HLVM_NORETURN HLVM_NOINLINE_FUNC void hlvm_internal_assert(const TCHAR* Expression, FString&& Message, const TCHAR* File, int Line)
	{
		// Sanity check that we are using stack mallocator
		assert(GMallocatorTLS == AssertStackMallocator);
#if HLVM_BUILD_DEBUG
		// In Debug mode we skip first 3 frames to get proper stack trace
		constexpr size_t SkipStackNum = 3;
#else
		// In RelWithDebInfo mode we skip first frame to get proper stack trace
		constexpr size_t SkipStackNum = 1;
#endif
		// Deliberate new here to avoid free on destrcutor as destructors are called
		// where stack mallocator is already swapped out by the end of this frame
		FStdString* Stack = new FStdString{ FGenericPlatformDebuggerUtil::GetStackTrace(SkipStackNum) }; // Explicitly call stack trace here to get proper stack trace
		FString*	msg = new FString{ FString::Format(TXT("{1} with '{2}' at {3}:{4}\n{0}"), **Stack, Expression, *Message, File,
			   Line) };

		// Log message
		HLVM_LOG(LogAssert, critical, **msg);

		// Try to debug break if debugger attached for easier debugging
		HLVM_TRY_DEBUG_BREAK();

		// Swap back to original GMallocator
		SwapMallocator();

		// Use original GMallocator to copy stack allocated msg back to heap
		throw std::runtime_error(std::string(msg->ToCharStr()));
	}
} // namespace hlvm_private
