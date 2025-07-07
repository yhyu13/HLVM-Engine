/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

/**
 * Inspired by Flax Engine :
 * https://github.com/yhyu13/FlaxEngine/blob/b2f9da4113a7c80586ce3c0e7a916a40f0c63f04/Source/Engine/Profiler/
 * https://flaxengine.com/licensing
 */

#include "ProfilerDefinition.h"

#if HLVM_COMPILE_WITH_PROFILER

	#include "Core/Container/ContainerDefinition.h"
	#include "Core/Mallocator/PMR.h"
	#include "Core/Parallel/Lock.h"

/**
 * Provides CPU performance measuring methods.
 */
class FProfilerCPU : public FAtomicFlagS<FProfilerCPU>
{
public:
	/**
	 * Represents single CPU profiling event data.
	 */
	PACK(struct FTrackedEvent {
		/**
		 * The start time (in milliseconds).
		 */
		TFP64 Start;

		/**
		 * The end time (in milliseconds).
		 */
		TFP64 End;

		/**
		 * The native dynamic memory allocation size during this event (excluding the child events). Given value is in bytes.
		 */
		TUINT32 NativeMemoryAllocation;

		/**
		 * The event depth. Value 0 is used for the root event.
		 */
		TUINT32 Depth;

		/**
		 * Name of the event with finite size
		 */
		TCharArray<23> Name;
	});

	/**
	 * Thread registered for profiling. Holds events data with read/write support.
	 */
	class FTrackedThread
	{
	public:
		FTrackedThread(const TCHAR* name);

		/**
		 * Gets the name of the tracked thread
		 */
		HLVM_INLINE_FUNC const FString& GetName() const
		{
			return mThreadName;
		}

		/**
		 * Begins the event running on a this thread.
		 * @param name The event name.
		 * @return The event token.
		 */
		size_t BeginEvent(const TCHAR* name);

		/**
		 * Ends the event running on a this thread.
		 * @param index The event index returned by the BeginEvent method.
		 */
		void EndEvent(size_t index);

		/**
		 *  Ends the last event running on a this thread.
		 *  @param SetAllUnhandledEvents Also set end timestamp to all previous events that have not end timestamp
		 */
		void EndEvent(bool SetAllUnhandledEvents = false);

	public:
		/**
		 * Extracts all events from the buffer and clears the buffer.
		 * Note, this method is not thread safe in theory,
		 * but since we don't care about data race,
		 * we can just ignore the thread safety after all.
		 * @return The events.
		 */
		TVector<FTrackedEvent> ExtractEvents();

	private:
		friend class FProfilerCPU;

		FString mThreadName;
		TUINT32 mCurrentEventDepth = 0;

		/**
		 * The events buffer is going to init into a fixed size, e.g. 8192
		 */
		TRingBuffer<FTrackedEvent> mEventBuffer;
	};

public:
	/**
	 * Determines whether the current (calling) thread is being profiled by the service (it may has no active profile block but is registered).
	 */
	HLVM_STATIC_FUNC bool IsProfilingCurrentThread();

	/**
	 * Gets the current thread (profiler service shadow object).
	 */
	HLVM_STATIC_FUNC FTrackedEvent* GetCurrentThreadActiveEvent();

	/**
	 * Static method to calls current thread's BeginEvent internally
	 * @return The event token.
	 */
	HLVM_STATIC_FUNC size_t BeginEvent();

	/**
	 * Static method to calls current thread's BeginEvent internally
	 * @param name The event name.
	 * @return The event token.
	 */
	HLVM_STATIC_FUNC size_t BeginEvent(const TCHAR* name);

	/**
	 * Ends the event running on current thread.
	 * @param index The event index returned by the BeginEvent method.
	 */
	HLVM_STATIC_FUNC void EndEvent(size_t index);

	/**
	 *  Ends the last event running on current thread.
	 */
	HLVM_STATIC_FUNC void EndEvent();

	/**
	 * Releases resources. Calls to the profiling API after Dispose are not valid.
	 */
	HLVM_STATIC_FUNC void Dispose();

	/**
	 * Record malloc event
	 */
	HLVM_STATIC_FUNC void OnMemMalloc(void* ptr, size_t size);

	/**
	 * Record free event
	 */
	HLVM_STATIC_FUNC void OnMemFree(void* ptr);

	/**
	 * Begin a frame
	 */
	HLVM_STATIC_FUNC void OnFrameBegin();

	/**
	 * End a frame
	 */
	HLVM_STATIC_FUNC void OnFrameEnd();

public:
	/**
	 * The profiling tools usage flag. Can be used to disable profiler. Engine turns it down before the exit and before platform startup.
	 */
	HLVM_STATIC_VAR bool bEnabled;

	/**
	 *  The current frame count.
	 */
	HLVM_STATIC_VAR TUINT64 FrameCount;

private:
	friend class FThreadPtrTLS;

	/**
	 * The registered threads. Reading and writing to this variable should be guarded by the Lock method.
	 * e.g. FAtomicLockGuard Lock(FProfilerCPU::GetAtomicFlagS()); ...
	 */
	HLVM_STATIC_VAR TSmallVector64<std::shared_ptr<FTrackedThread>> TrackedThreads;
};

/**
 * Helper structure used to call BeginEvent/EndEvent within single code block.
 */
struct FScopeEventCPU
{
	size_t Index;

	FScopeEventCPU()
	{
		Index = FProfilerCPU::BeginEvent();
	}

	HLVM_INLINE_FUNC explicit FScopeEventCPU(const char* name)
	{
		Index = FProfilerCPU::BeginEvent(TO_TCHAR_CSTR(name));
	}

	HLVM_INLINE_FUNC explicit FScopeEventCPU(const TCHAR* name)
	{
		Index = FProfilerCPU::BeginEvent(name);
	}

	HLVM_INLINE_FUNC ~FScopeEventCPU()
	{
		FProfilerCPU::EndEvent(Index);
	}
};
#endif

/**
 * Override these in each profiler backend
 */
#if HLVM_COMPILE_WITH_PROFILER
	/**
	 * By default, cpu profiler will trigger event on every call,
	 * which has potential of recursive calling that evatually run out of stack.
	 * For low level cpu profiling, especially for memory allocation, use no event macro
	 * to avoid creating an tracking event
	 */
	#define HLVM_PROFILE_CPU_NO_TRACK_EVENT() ((void)0)
	#define HLVM_PROFILE_CPU_NAMED_NO_TRACK_EVENT(name) ((void)0)
	#define HLVM_PROFILE_CPU_SRC_LOC_NO_TRACK_EVENT(srcLoc) ((void)0)
	#define HLVM_PROFILE_CPU() FScopeEventCPU ProfileBlockCPU(__FUNCTION__)
	#define HLVM_PROFILE_CPU_NAMED(name) FScopeEventCPU ProfileBlockCPU(name)
	#define HLVM_PROFILE_CPU_SRC_LOC(srcLoc) FScopeEventCPU ProfileBlockCPU((srcLoc).name)
#else
	#define HLVM_PROFILE_CPU_NO_TRACK_EVENT() ((void)0)
	#define HLVM_PROFILE_CPU_NAMED_NO_TRACK_EVENT(name) ((void)0)
	#define HLVM_PROFILE_CPU_SRC_LOC_NO_TRACK_EVENT(srcLoc) ((void)0)
	#define HLVM_PROFILE_CPU() ((void)0)
	#define HLVM_PROFILE_CPU_NAMED(name) ((void)0)
	#define HLVM_PROFILE_CPU_SRC_LOC(srcLoc) ((void)0)
#endif

/**
 * Memory Allocation macro for tracking memory allocation
 * Usually no need to override
 */
#if HLVM_COMPILE_WITH_PROFILER
	#define HLVM_PROFILER_CPU_ON_MALLOC(ptr, size) FProfilerCPU::OnMemMalloc(ptr, size)
	#define HLVM_PROFILER_CPU_ON_FREE(ptr) FProfilerCPU::OnMemFree(ptr)
#else
	#define HLVM_PROFILER_CPU_ON_MALLOC(ptr, size) ((void)0)
	#define HLVM_PROFILER_CPU_ON_FREE(ptr) ((void)0)
#endif

#if HLVM_COMPILE_WITH_PROFILER
	#define HLVM_PROFILER_CPU_ONOFF(cond)                    \
		HLVM_SCOPED_VARIABLE(                                \
			ScopedProfilerCPU,                               \
			[]() -> void { FProfilerCPU::bEnabled = cond; }, \
			[]() -> void { FProfilerCPU::bEnabled = !cond; })
#else
	#define HLVM_PROFILER_CPU_ONOFF(cond) ((void)0)
#endif

#if HLVM_PROFILER_USE_TRACY
	#include "Tracy/TracyProfilerCPU.h"
#endif

// TODO Add minitrace support
