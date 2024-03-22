/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#ifndef HLVM_ENABLE_FIBER_POOL
	#define HLVM_ENABLE_FIBER_POOL 0
#endif

/**
 * Fiber pool segfaults for no apparent reasons, so disable it
 */
#if HLVM_ENABLE_FIBER_POOL

	#include "Core/Parallel/ConcurrentQueue.h"
	#include "AsyncConfig.h"

	#include <boost/fiber/future/future.hpp>
	#include <boost/fiber/future/packaged_task.hpp>
	#include <boost/fiber/operations.hpp>
	#include <boost/fiber/algo/shared_work.hpp>
	#include <boost/fiber/algo/work_stealing.hpp>
	#include <boost/fiber/fiber.hpp>
	#include <boost/thread/thread.hpp>

	#ifndef HLVM_FIBER_POOL_USE_WORKSTEAL
		#define HLVM_FIBER_POOL_USE_WORKSTEAL 1
	#endif

class FWorkStealFiberPool
{
public:
	NOCOPYMOVE(FWorkStealFiberPool)
	explicit FWorkStealFiberPool(const FThreadAffinityMode& AffinityMode = AllPhysicalCores);
	~FWorkStealFiberPool();

	static FWorkStealFiberPool* Get();

	template <typename F, typename... Args>
	HLVM_NODISCARD auto EnqueuTask(F&& f, Args&&... args)
	{
		using TaskRetType = std::invoke_result_t<F, Args...>;
		using TaskType = boost::fibers::packaged_task<TaskRetType()>;

		auto task = std::make_shared<TaskType>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
		auto result = task->get_future();
		auto work = [_task = MoveTemp(task)]() { (*_task)(); };
		mQueue.Push(MoveTemp(work));
		return MoveTemp(result);
	}

	template <typename F, typename... Args>
	void EnqueuDetached(F&& f, Args&&... args)
	{
		using TaskRetType = std::invoke_result_t<F, Args...>;
		using TaskType = boost::fibers::packaged_task<TaskRetType()>;

		auto task = std::make_shared<TaskType>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
		auto work = [_task = MoveTemp(task)]() { (*_task)(); };
		mQueue.Push(MoveTemp(work));
	}

	uint32_t NumThreads() const
	{
		return mCount;
	}

private:
	using ThreadsType = boost::thread_group;
	using ProcType = std::function<void(void)>;
	using QueueType = TConcurrentQueue<ProcType, EConcurrentQueueMode::Mpmc, true>;

	#if HLVM_FIBER_POOL_USE_WORKSTEAL
	// Work steal pool only allow one instance of pool object, warning user if multiple instances are to be constructed
	HLVM_STATIC_VAR HLVM_INLINE_VAR bool sInitialized{ false };
	#endif

	QueueType	mQueue;
	ThreadsType mThreads;
	uint32_t	mCount;
};

#endif
