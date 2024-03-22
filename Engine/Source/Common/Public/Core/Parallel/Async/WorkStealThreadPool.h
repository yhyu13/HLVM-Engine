/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "Core/Parallel/ConcurrentQueue.h"
#include "AsyncConfig.h"

#include <boost/thread/thread.hpp>

class FWorkStealThreadPool
{
public:
	NOCOPYMOVE(FWorkStealThreadPool)
	explicit FWorkStealThreadPool(const FThreadAffinityMode& AffinityMode = AllPhysicalCores);
	~FWorkStealThreadPool();

	static FWorkStealThreadPool* Get();

	template <typename F, typename... Args>
	HLVM_NODISCARD auto EnqueuTask(F&& f, Args&&... args)
	{
		using TaskRetType = std::invoke_result_t<F, Args...>;
		using TaskType = std::packaged_task<TaskRetType()>;

		auto	   task = new TaskType(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
		auto	   result = task->get_future();
		auto	   work = [task]() { (*task)(); delete task; };
		const auto index = (mJobIndex.fetch_add(1, std::memory_order_relaxed) % mCount);
		mQueues[index]->Push(MoveTemp(work));
		return MoveTemp(result);
	}

	template <typename F, typename... Args>
	void EnqueuDetached(F&& f, Args&&... args)
	{
		using TaskRetType = std::invoke_result_t<F, Args...>;
		using TaskType = std::packaged_task<TaskRetType()>;

		auto task = TaskType(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
		std::thread(MoveTemp(task)).detach();
	}

	uint32_t NumThreads() const
	{
		return mCount;
	}

private:
	constexpr inline static int K = { 2 };

	using ProcType = std::function<void(void)>;
	using QueueType = TConcurrentQueue<ProcType, EConcurrentQueueMode::Mpmc, true>;
	using QueuesType = TVector<std::unique_ptr<QueueType>>;
	using ThreadsType = boost::thread_group;

	QueuesType				  mQueues;
	ThreadsType				  mThreads;
	uint32_t				  mCount;
	std::atomic_uint_fast32_t mJobIndex = { 0 };
};
