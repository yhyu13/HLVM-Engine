/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#include "Core/Parallel/Async/WorkStealThreadPool.h"
#include "Platform/GenericPlatformThreadUtil.h"
#include "Utility/ScopedTimer.h"

DECLARE_LOG_CATEGORY(LogWorkStealThreadPool);

FWorkStealThreadPool* FWorkStealThreadPool::Get()
{
	// CAUTION: Since FWorkStealThreadPool calls FGenericPlatformThreadUtil::SetThreadsWithAffinity internally,
	// FGenericPlatformThreadUtil already depends on has global static singleton,
	// If SPool is also global static, it could lead to Static Initialization Order Fiasco https://en.cppreference.com/w/cpp/language/siof
	static FWorkStealThreadPool sPool{};
	return &sPool;
}

FWorkStealThreadPool::FWorkStealThreadPool(const FThreadAffinityMode& AffinityMode)
{
	HLVM_ENSURE_F(AffinityMode.Valid(), TXT("AffinityMode not valid {}"), AffinityMode.ToString());

	// TODO : support other thread affinity mode
	auto Config2 = S_C(const FThreadAffinityMode2*, AffinityMode);
	mCount = Config2->NumThreads;

	// First create queues
	mQueues.reserve(mCount);
	for (uint32_t i = 0; i < mCount; ++i)
	{
		mQueues.emplace_back(new QueueType());
	}
	mQueues.resize(mCount);

	// Then create threads
	TVector<boost::thread*> Threads{};
	Threads.reserve(mCount);
	for (uint32_t i = 0; i < mCount; ++i)
	{
		auto Func = [this, index = i] {
			if constexpr (HLVM_DEBUG_THREAD_UTILITY)
			{
				HLVM_LOG(LogWorkStealThreadPool, trace, TXT("Thread {} created"), index);
			}
			// Register TID as worker thread
			FWorkerPoolTIDUtil::TIDRegisterScoped tidRegister{};

			auto& Queue = mQueues[index];
			for (;;)
			{
				ProcType task;
				/**
				 * Try steal work from all threads' queues
				 */
				for (uint32_t n = 0; n < mCount; ++n)
				{
					auto stolenFromIndex = (index + n) % mCount;
					if (mQueues[stolenFromIndex]->PopFront<true>(task))
					{
						if constexpr (HLVM_DEBUG_THREAD_UTILITY)
						{
							HLVM_LOG(LogWorkStealThreadPool, trace, TXT("Thread {} stole job from {}"), index, stolenFromIndex);
						}
						break;
					}
				}
				/**
				 * If steal fails, wait on current thread's queue
				 */
				if (!task)
				{
					if (Queue->PopFront<false>(task))
					{
						if constexpr (HLVM_DEBUG_THREAD_UTILITY)
						{
							HLVM_LOG(LogWorkStealThreadPool, trace, TXT("Thread {} no stole job but wait on self"), index);
						}
					}
					else
					{
						// If should stop, stop the thread
						if (Queue->ShouldStopPop())
						{
							break;
						}
						// There might be chances that queue is not stopped by still pop failed due to competition
						// In this case we must continue the thread
						continue;
					}
				}
				HLVM_ASSERT_F(task, TXT("Task is null"));

				if constexpr (HLVM_DEBUG_THREAD_UTILITY)
				{
					FScopedTimerLog Timer(FString::Format(TXT("Thread {} exec task"), index));
					task();
				}
				else
				{
					task();
				}
				/*
				 * Collect memory for multithreading to avoid delayed freeing
				 */
				GMallocatorTLS->Collect();
			}
		};

		auto Thread = mThreads.create_thread(MoveTemp(Func));
		HLVM_ENSURE_F(Thread, TXT("Thread init failed {}"), i);
		Threads.emplace_back(Thread);
	}
	Threads.resize(mCount);

	HLVM_ENSURE_F(FGenericPlatformThreadUtil::SetThreadsWithAffinity(Threads, AffinityMode), TXT("Thread affinity set failed with"));
}

FWorkStealThreadPool::~FWorkStealThreadPool()
{
	for (auto& queue : mQueues)
	{
		queue->SignalStop();
	}
	mThreads.join_all();
}
