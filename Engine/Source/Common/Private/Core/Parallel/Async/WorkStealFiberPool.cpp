/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Core/Parallel/Async/WorkStealFiberPool.h"

#if HLVM_ENABLE_FIBER_POOL
	#include "Platform/GenericPlatformThreadUtil.h"

DELCARE_LOG_CATEGORY(LogWorkStealFiberPool);

FWorkStealFiberPool* FWorkStealFiberPool::Get()
{
	// CAUTION: Since FWorkStealFiberPool calls FGenericPlatformThreadUtil::SetThreadsWithAffinity internally,
	// FGenericPlatformThreadUtil already depends on has global static singleton,
	// If SPool is also global static, it could lead to Static Initialization Order Fiasco https://en.cppreference.com/w/cpp/language/siof
	static FWorkStealFiberPool sPool{};
	return &sPool;
}

FWorkStealFiberPool::FWorkStealFiberPool(const FThreadAffinityMode& AffinityMode)
{
	#if HLVM_FIBER_POOL_USE_WORKSTEAL
	// Work steal pool only allow one instance of pool object, warning user if multiple instances are to be constructed
	if (!sInitialized)
	{
		sInitialized = true;
	}
	else
	{
		HLVM_LOG(LogWorkStealFiberPool, err, TXT("Fiber thread pool under work steal cannot have more than 1 instance"));
	}
	#endif

	HLVM_ENSURE(AffinityMode.Valid(), TXT("AffinityMode not valid {}"), AffinityMode.ToString());

	// TODO : support other thread affinity mode
	auto Config2 = S_C(const FThreadAffinityMode2*, AffinityMode);
	mCount = Config2->NumThreads;

	// Then create threads
	TVector<boost::thread*> Threads;
	for (uint32_t i = 0; i < mCount; ++i)
	{
		auto Func = [this
	#if HLVM_DEBUG_THREAD_UTIL
						,
						index = i
	#endif
		] {
			if constexpr (HLVM_DEBUG_THREAD_UTIL)
			{
				HLVM_LOG(LogWorkStealFiberPool, trace, TXT("Fiber {} created"), index);
			}
			// https://github.com/moneroexamples/fiberpool/blob/master/include/FiberPool.hpp
	#if HLVM_FIBER_POOL_USE_WORKSTEAL
			// work_stealing sheduling is much faster
			// than work_shearing, but it does not
			// allow for modifying number of threads
			// at runtime. Therefore if one uses
			// DefaultFiberPool, no other instance
			// of the fiber pool can be created
			// as this would change the number of
			// worker threads
			boost::fibers::use_scheduling_algorithm<
				boost::fibers::algo::work_stealing>(
				mCount, true);
	#else
			// it is slower but, can vary number of
			// worker threads at runtime. So you can
			// use DefaultFiberPool in one part of
			// you application, and custom instance
			// of the fiber pool in other part.
			boost::fibers::use_scheduling_algorithm<
				boost::fibers::algo::shared_work>(true);
	#endif

			for (;;)
			{
				ProcType task;
				if (mQueue.PopFront<false>(task))
				{
					if constexpr (HLVM_DEBUG_THREAD_UTIL)
					{
						HLVM_LOG(LogWorkStealFiberPool, trace, TXT("Fiber {} stole job"), index);
					}
				}
				else
				{
					if (mQueue.ShouldStopPop())
					{
						break;
					}
					// There might be chances that queue is not stopped by still pop failed due to competition
					// In this case we must continue the thread
					continue;
				}
				boost::fibers::fiber(MoveTemp(task)).join();
			}
		};

		auto Thread = mThreads.create_thread(MoveTemp(Func));
		HLVM_ENSURE(Thread, TXT("Thread init failed {}"), i);
		Threads.push_back(Thread);
	}
	HLVM_ENSURE(FGenericPlatformThreadUtil::SetThreadsWithAffinity(Threads, AffinityMode), TXT("Thread affinity set failed with"));
}

FWorkStealFiberPool::~FWorkStealFiberPool()
{
	sInitialized = false;
	mQueue.SignalStop();
	mThreads.join_all();
}
#endif
