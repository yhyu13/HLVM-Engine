/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#include "Core/Assert.h"
#include "Core/Log.h"
#include "Platform/GenericPlatformStackTrace.h"
#include "Platform/GenericPlatformDebuggerUtil.h"
#include "Core/Mallocator/StackMallocator.h"

#ifndef HLVM_STACK_TRACE_DEPTH
	#define HLVM_STACK_TRACE_DEPTH 8
#endif

namespace hlvm_private
{
	/**
	 * Assert stack mallocator, adjust reserved memory size to your needs
	 */
	HLVM_STATIC_VAR TStackMallocator<64 * 1024, false, true, false, false, false> stack_mallocator{};
	IMallocator*																  AssertionStackMallocator{ &stack_mallocator }; // extern	    // extern
	FAtomicFlagNC																  AssertionStackLock{};

	void InitAssertionStackMallocator() // extern
	{
		// Reset stack mallocator each time init is called to wap out any previous allocations
		// This is totally valid since allocation last time on assertion is already handled and gone obsolete
		stack_mallocator.Reset();
	}

	HLVM_NORETURN HLVM_NOINLINE_FUNC void hlvm_internal_assert(const TCHAR* Expression, const FString* Message, const TCHAR* File, int Line)
	{
		// Sanity check that we are using stack mallocator
		assert(GMallocatorTLS == AssertionStackMallocator);
		{
#if HLVM_BUILD_DEBUG
			// In Debug mode we skip 1 frames to get proper stack trace
			// Adjust these values to get proper stack trace
			constexpr size_t SkipStackNum = 1;
			constexpr size_t MaxStackDepth = HLVM_STACK_TRACE_DEPTH;
#else
			// In RelWithDebInfo mode we skip 1 frame to get proper stack trace
			// Adjust these values to get proper stack trace
			constexpr size_t SkipStackNum = 1;
			constexpr size_t MaxStackDepth = HLVM_STACK_TRACE_DEPTH;
#endif
			// Deliberate use *new* here manually control lifetime
			auto Stack = new FStdString{ FGenericPlatformStackTrace::GetStackTrace(
				SkipStackNum, MaxStackDepth) }; // Explicitly call stack trace here to get proper stack trace
			auto LogMsg = new FString{
				FString::Format(TXT("{1} with '{2}' at {3}:{4}\n{0}"), **Stack, Expression, **Message, File,
					Line)
			};

			// Free memory in order to leave more space before logging
			delete Stack;
			delete Message;

			// Log message
			HLVM_LOG(LogAssert, critical, **LogMsg);

			delete LogMsg;

			// Swap back to original GMallocator
			SwapMallocator();
		}

		// Try to debug break if debugger attached for easier debugging
		HLVM_TRY_DEBUG_BREAK();

		// Use original GMallocator to copy stack allocated msg back to heap
		throw std::runtime_error(TO_CHAR_STR(Expression));
	}
} // namespace hlvm_private
