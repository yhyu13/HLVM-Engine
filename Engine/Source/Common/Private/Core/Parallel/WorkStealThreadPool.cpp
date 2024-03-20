/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Core/Parallel/Async/WorkStealThreadPool.h"
#include "Platform/GenericPlatformThreadUtil.h"

FWorkStealThreadPool* FWorkStealThreadPool::Get()
{
	// CAUTION: Since FWorkStealThreadPool calls FGenericPlatformThreadUtil::SetThreadsWithAffinity internally,
	// FGenericPlatformThreadUtil already depends on has global static singleton,
	// If SPool is also global static, it could lead to Static Initialization Order Fiasco https://en.cppreference.com/w/cpp/language/siof
	static FWorkStealThreadPool sPool{};
	return &sPool;
}

FWorkStealThreadPool::FWorkStealThreadPool(const FThreadAffinityMode& ThreadConfig)
{
	HLVM_ENSURE(ThreadConfig.Valid(), TXT("ThreadConfig not valid {}"), ThreadConfig.ToString());

	// TODO : support other thread affinity mode
	auto Config2 = S_C(const FThreadAffinityMode2*, ThreadConfig);
	mCount = Config2->NumThreads;

	// First create queues
	for (uint32_t i = 0; i < mCount; ++i)
	{
		mQueues.emplace_back(new QueueType());
	}
	// Then create threads
	TVector<boost::thread*> Threads;
	for (uint32_t i = 0; i < mCount; ++i)
	{
		auto Func = [this, index = i] {
			for (;;)
			{
				ProcType task;
				for (uint32_t n = 0; n < mCount * K; ++n)
				{
					if (mQueues[(index + n) % mCount]->PopFront<true>(task))
					{
						break;
					}
				}
				if (!task && !mQueues[index]->PopFront<false>(task))
				{
					break;
				}
				task();
			}
		};
		auto Thread = mThreads.create_thread(MoveTemp(Func));
		HLVM_ENSURE(Thread, TXT("Thread init failed {}"), i);
		Threads.push_back(Thread);
	}
	HLVM_ENSURE(FGenericPlatformThreadUtil::SetThreadsWithAffinity(Threads, ThreadConfig), TXT("Thread affinity set failed with"));
}

FWorkStealThreadPool::~FWorkStealThreadPool()
{
	for (auto& queue : mQueues)
	{
		queue->SignalStop();
	}
	mThreads.join_all();
}
