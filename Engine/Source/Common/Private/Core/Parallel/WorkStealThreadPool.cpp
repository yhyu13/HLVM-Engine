/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Core/Parallel/WorkStealThreadPool.h"

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
	}
}

FWorkStealThreadPool::~FWorkStealThreadPool()
{
	for (auto& queue : mQueues)
	{
		queue->SignalStop();
	}
	for (auto& thread : mThreads)
	{
		thread.join();
	}
}
