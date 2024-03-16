/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Core/Parallel/Async/WorkStealThreadPool.h"

static FWorkStealThreadPool SPool;

FWorkStealThreadPool* FWorkStealThreadPool::Get()
{
	return &SPool;
}

FWorkStealThreadPool::FWorkStealThreadPool(uint32_t NumThreads)
	: mCount(NumThreads)
{
	for (uint32_t i = 0; i < NumThreads; ++i)
	{
		mQueues.emplace_back(new QueueType());
	}
	for (uint32_t i = 0; i < NumThreads; ++i)
	{
#if HLVM_THREAD_USE_BOOST
		HLVM_ENSURE(mThreads.create_thread(
						[this, index = i] {
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
						}),
			TXT("Thread creation failed at {}, NumThreads {}"), i, NumThreads);
#else
		mThreads.emplace_back(
			[this, index = i] {
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
			});
#endif
	}
}

FWorkStealThreadPool::~FWorkStealThreadPool()
{
	for (auto& queue : mQueues)
	{
		queue->SignalStop();
	}
#if HLVM_THREAD_USE_BOOST
	mThreads.join_all();
#else
	for (auto& thread : mThreads)
	{
		thread.join();
	}
#endif
}
