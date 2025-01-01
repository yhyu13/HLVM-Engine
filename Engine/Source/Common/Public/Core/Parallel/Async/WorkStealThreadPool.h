/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "TaskQueue.h"

#include <boost/thread/thread.hpp>

class FWorkStealThreadPool
{
public:
	NOCOPYMOVE(FWorkStealThreadPool)
	explicit FWorkStealThreadPool(const FThreadAffinityMode& AffinityMode = FThreadAffinityMode::BgTwoPhysicalCores());
	~FWorkStealThreadPool();

	static FWorkStealThreadPool* Get();

	template <typename F, typename... Args>
	HLVM_NODISCARD auto EnqueueTask(F&& f, Args&&... args)
	{
		return EnqueueTask(ETaskPriority::Default, FwdTemp<F>(f), FwdTemp<Args>(args)...);
	}

	template <typename F, typename... Args>
	HLVM_NODISCARD auto EnqueueTask(ETaskPriority Priority, F&& f, Args&&... args)
	{
		using TaskRetType = std::invoke_result_t<F, Args...>;
		using TaskType = std::packaged_task<TaskRetType()>;

		auto	   task = new TaskType(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
		auto	   result = task->get_future();
		auto	   work = [task]() { (*task)(); delete task; };
		const auto index = (mJobIndex.fetch_add(1, std::memory_order_relaxed) % mCount);
		mQueues[index]->Push(Priority, MoveTemp(work));
		return MoveTemp(result);
	}

	uint32_t NumThreads() const
	{
		return mCount;
	}

private:
	using ProcType = std::function<void(void)>;
	using QueueType = TTaskQueue<ProcType>;
	using QueuesType = TVector<std::unique_ptr<QueueType>>;
	using ThreadsType = boost::thread_group;

	QueuesType				  mQueues;
	ThreadsType				  mThreads;
	uint32_t				  mCount;
	std::atomic_uint_fast32_t mJobIndex = { 0 };
};
