/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "Core/Parallel/Lock.h"
#include "Core/Parallel/ConcurrentQueue.h"

class FWorkStealThreadPool
{
public:
	NOCOPYMOVE(FWorkStealThreadPool)

	explicit FWorkStealThreadPool(uint32_t NumThreads = std::thread::hardware_concurrency() / 2);
	~FWorkStealThreadPool();

	static FWorkStealThreadPool* Get();

	template <typename F, typename... Args>
	HLVM_NODISCARD auto EnqueuTask(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>>
	{
		using TaskRetType = std::invoke_result_t<F, Args...>;
		using TaskType = std::packaged_task<TaskRetType()>;

		const auto index = (mJobIndex.fetch_add(1, std::memory_order_relaxed) % mCount);
		auto	   task = std::make_shared<TaskType>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
		auto	   result = task->get_future();
		auto	   work = [_task = MoveTemp(task)]() { (*_task)(); };
		mQueues[index]->Push(MoveTemp(work));
		return result;
	}

	uint32_t NumThreads() const { return mCount; }

private:
	constexpr inline static int K = { 2 };

	using ProcType = std::function<void(void)>;
	using QueueType = TConcurrentQueue<ProcType, EConcurrentQueueMode::Mpmc, true>;
	using QueuesType = TVector<std::unique_ptr<QueueType>>;
	using ThreadsType = TVector<std::thread>;

	QueuesType				  mQueues;
	ThreadsType				  mThreads;
	uint32_t				  mCount;
	std::atomic_uint_fast32_t mJobIndex = { 0 };
};
