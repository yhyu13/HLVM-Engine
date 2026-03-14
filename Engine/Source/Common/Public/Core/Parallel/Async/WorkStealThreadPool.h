/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "TaskQueue.h"

#include <boost/thread/thread.hpp>

class FWorkStealThreadPool
{
private:
	using ProcType = std::function<void(void)>;
	using QueueType = TTaskQueue<ProcType>;
	using QueuesType = TVector<std::unique_ptr<QueueType>>;
	using ThreadsType = boost::thread_group;

public:
	NOCOPYMOVE(FWorkStealThreadPool)
	explicit FWorkStealThreadPool(const FThreadAffinityMode& AffinityMode = FThreadAffinityMode::NormalAllPhysicalCores());
	~FWorkStealThreadPool();

	static FWorkStealThreadPool* Get();

	template <typename F, typename... Args>
	HLVM_NODISCARD auto EnqueueTask(F&& f, Args&&... args)
	{
		return EnqueueTask(ETaskPriority::Default, FwdTemp<F>(f), FwdTemp<Args>(args)...);
	}

	/*
	 * @brief Enqueue a task to the worker pool
	 * @param Priority The priority of the task
	 * @param f The task to enqueue
	 * @return A future that will be ready when the task is complete
	 * @note This function will check if the current thread is valid to use the worker pool to prevent deadlocks
	 * induced by thread draining.
	 */
	template <typename F, typename... Args>
	HLVM_NODISCARD auto EnqueueTask(ETaskPriority Priority, F&& f, Args&&... args)
	{
		using TaskRetType = std::invoke_result_t<F, Args...>;
		using TaskType = std::packaged_task<TaskRetType()>;

		// Check if the current thread is valid to use the worker pool
		HLVM_ENSURE_F(FWorkerPoolTIDUtil::IsThreadValidToUseWorkerPool(), TXT("IsThreadValidToUseWorkerPool false for thread {}"), GCurrentTID64);

		auto task = new TaskType(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
		auto result = task->get_future();
		// Since "task" is a pointer type, it is fine to just copy by value
		auto	   work = [task]() { (*task)(); delete task; };
		const auto index = (mJobIndex.fetch_add(1, std::memory_order_relaxed) % mCount);
		mQueues[index]->Push(Priority, MoveTemp(work));
		return MoveTemp(result);
	}

	/**
	 * @brief Enqueue a task to the worker pool if the queue is empty
	 * @param Priority The priority of the task
	 * @param f The task to enqueue
	 * @return A future and a boolean indicating whether the task was enqueued
	 */
	template <typename F, typename... Args>
	HLVM_NODISCARD auto EnqueueTaskIfEmpty(ETaskPriority Priority, F&& f, Args&&... args)
	{
		using TaskRetType = std::invoke_result_t<F, Args...>;
		using TaskType = std::packaged_task<TaskRetType()>;
		using RetType = std::pair<std::future<TaskRetType>, bool>;

		auto task = new TaskType(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
		auto result = task->get_future();
		// Since "task" is a pointer type, it is fine to just copy by value
		auto	   work = [task]() { (*task)(); delete task; };
		for (uint32_t i = 0; i < mCount; ++i)
		{
			if (mQueues[i]->IsEmpty(Priority))
			{
				if (mQueues[i]->PushIfEmpty(Priority, MoveTemp(work)))
				{
					return RetType{ MoveTemp(result), true };
				}
				else
				{
					delete task;
					return RetType({}, false);
				}
			}
		}
		return RetType({}, false);
	}

	uint32_t NumThreads() const
	{
		return mCount;
	}

private:
	QueuesType				  mQueues;
	ThreadsType				  mThreads;
	uint32_t				  mCount;
	std::atomic_uint_fast32_t mJobIndex = { 0 };
};
