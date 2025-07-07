/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

/**
 * Inspired by Flax Engine :
 * https://github.com/yhyu13/FlaxEngine/blob/b2f9da4113a7c80586ce3c0e7a916a40f0c63f04/Source/Engine/Profiler/
 * https://flaxengine.com/licensing
 */

#include "Utility/Profiler/ProfilerCPU.h"

#if HLVM_COMPILE_WITH_PROFILER

/**
 * TLS Wrapper for FTrackedThread, used later as tls object to hold FTrackedThread
 */
class FThreadPtrTLS
{
public:
	NOCOPYMOVE(FThreadPtrTLS)
	FThreadPtrTLS() = default;
	~FThreadPtrTLS()
	{
		if (mThread)
		{
			LOCK_GUARD_FLAG(FProfilerCPU::GetAtomicFlagS());
			/**
			 * Remove thread from tracked threads on tls destruction
			 */
			std::remove_if(FProfilerCPU::TrackedThreads.begin(), FProfilerCPU::TrackedThreads.end(),
				[this](const std::shared_ptr<FProfilerCPU::FTrackedThread>& thread) {
					return thread.get() == mThread;
				});
		}
	}

	HLVM_INLINE_FUNC FProfilerCPU::FTrackedThread* Get() const
	{
		return mThread;
	}

	FProfilerCPU::FTrackedThread* Set(std::shared_ptr<FProfilerCPU::FTrackedThread>&& thread)
	{
		mThread = thread.get();
		{
			/**
			 * Add thread to tracked threads on tls creation
			 */
			LOCK_GUARD_FLAG(FProfilerCPU::GetAtomicFlagS());
			FProfilerCPU::TrackedThreads.push_back(MoveTemp(thread));
		}
		return mThread;
	}

	operator bool() const
	{
		return mThread != nullptr;
	}

private:
	FProfilerCPU::FTrackedThread* mThread;
};
HLVM_THREAD_LOCAL_VAR HLVM_STATIC_VAR FThreadPtrTLS CurrentThread;

FProfilerCPU::FTrackedThread::FTrackedThread(const TCHAR* name)
{
	mThreadName = name;
	mEventBuffer.resize(8192);
}

size_t FProfilerCPU::FTrackedThread::BeginEvent(const TCHAR* name)
{
	const double  time = GlobalTimerFromStart.MarkMilli();
	FTrackedEvent e;
	e.Name = name;
	e.Start = time;
	e.End = 0;
	e.Depth = mCurrentEventDepth++;
	e.NativeMemoryAllocation = 0;
	mEventBuffer.push_back(e);
	return mEventBuffer.size() - 1;
}

void FProfilerCPU::FTrackedThread::EndEvent(size_t index)
{
	const double   time = GlobalTimerFromStart.MarkMilli();
	FTrackedEvent& e = mEventBuffer[index];
	e.End = time;
	--mCurrentEventDepth;
}

void FProfilerCPU::FTrackedThread::EndEvent(bool SetAllUnhandledEvents)
{
	const double time = GlobalTimerFromStart.MarkMilli();
	if (!SetAllUnhandledEvents)
	{
		FTrackedEvent& e = mEventBuffer.back();
		e.End = time;
	}
	else
	{
		for (size_t i = mEventBuffer.size() - 1; i > 0; --i)
		{
			FTrackedEvent& e = mEventBuffer[i];
			if (e.End == 0)
			{
				e.End = time;
			}
		}
	}
	--mCurrentEventDepth;
}

TVector<FProfilerCPU::FTrackedEvent> FProfilerCPU::FTrackedThread::ExtractEvents()
{
	TVector<FProfilerCPU::FTrackedEvent> Ret;
	Ret.reserve(mEventBuffer.size());
	/**
	 * The circular buffer is separated into two arrays, so we need to copy both
	 */
	const auto FirstHalf = mEventBuffer.array_one();
	const auto SecondHalf = mEventBuffer.array_two();
	std::copy(FirstHalf.first, FirstHalf.first + FirstHalf.second, std::back_inserter(Ret));
	std::copy(SecondHalf.first, SecondHalf.first + SecondHalf.second, std::back_inserter(Ret));
	// Clear the circular buffer after copy
	mEventBuffer.clear();
	return Ret;
}

bool														  FProfilerCPU::bEnabled{ true }; // Static
TUINT64														  FProfilerCPU::FrameCount{ 0 };  // Static
TSmallVector64<std::shared_ptr<FProfilerCPU::FTrackedThread>> FProfilerCPU::TrackedThreads{}; // Static

bool FProfilerCPU::IsProfilingCurrentThread()
{
	return bEnabled && CurrentThread.Get() != nullptr;
}

FProfilerCPU::FTrackedEvent* FProfilerCPU::GetCurrentThreadActiveEvent()
{
	return bEnabled && CurrentThread.Get() ? &CurrentThread.Get()->mEventBuffer.back() : nullptr;
}

size_t FProfilerCPU::BeginEvent()
{
	return BeginEvent(TXT("Anonymous"));
}

size_t FProfilerCPU::BeginEvent(const TCHAR* name)
{
	if (!bEnabled)
	{
		return INVALID_INDEX_SIZE_T();
	}
	else
	{
		FTrackedThread* thread = CurrentThread.Get();
		if (thread == nullptr)
		{
			auto threadName = FString::Format(TXT("{:#x}"), GCurrentTID64);
			thread = CurrentThread.Set(std::make_shared<FTrackedThread>(*threadName));
		}
		return thread->BeginEvent(name);
	}
}

void FProfilerCPU::EndEvent(size_t index)
{
	if (!bEnabled)
	{
		return;
	}
	FTrackedThread* thread = CurrentThread.Get();
	if (thread && index != INVALID_INDEX_SIZE_T())
	{
		thread->EndEvent(index);
	}
}

void FProfilerCPU::EndEvent()
{
	if (!bEnabled)
	{
		return;
	}
	FTrackedThread* thread = CurrentThread.Get();
	if (thread)
	{
		thread->EndEvent(false);
	}
}

void FProfilerCPU::Dispose()
{
	bEnabled = false;
}

void FProfilerCPU::OnMemMalloc(void* ptr, size_t size)
{
	if (!bEnabled)
	{
		return;
	}
	if (!ptr || size == 0)
	{
		// bad alloc? Ignore it
		return;
	}

	#if HLVM_PROFILER_USE_TRACY
	// Track memory allocation in Tracy
	if (GbTracyEnabled)
	{
		TracySecureAlloc(ptr, size);
	}
	#endif

	// Register allocation during the current CPU event (TODO : should we reduce this value when free?)
	if (FProfilerCPU::FTrackedEvent* LastEvent = GetCurrentThreadActiveEvent();
		LastEvent)
	{
		LastEvent->NativeMemoryAllocation += size;
	}
}

void FProfilerCPU::OnMemFree(void* ptr)
{
	if (!bEnabled)
	{
		return;
	}
	if (!ptr)
	{
		// bad free? Ignore it
		return;
	}

	#if HLVM_PROFILER_USE_TRACY
	// Track memory allocation in Tracy
	if (GbTracyEnabled)
	{
		TracySecureFree(ptr);
	}
	#endif
}

void FProfilerCPU::OnFrameBegin()
{
	FrameCount++;
	#if HLVM_PROFILER_USE_TRACY
	FrameMarkStart(nullptr);
	#endif
}

void FProfilerCPU::OnFrameEnd()
{
	#if HLVM_PROFILER_USE_TRACY
	FrameMarkEnd(nullptr);
	#endif
}

#endif
