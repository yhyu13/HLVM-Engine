/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "AsyncConfig.h"
#include "WorkStealThreadPool.h"
#include "WorkStealFiberPool.h"

#include <future>

HLVM_ENUM(EAsyncMode, TUINT8,
	PoolOrderless, // Add to default work steal thread pool (task order not guaranteed! So make sure tasks are not order dependent when launching)
	PoolOrdered,
	StandAlone // Startup a new thread
);

class FAsync
{
public:
	/**
	 * Launch a task
	 * @tparam F
	 * @tparam Args
	 * @param mode If using pool, task will be launched in a thread from the default work steal thread pool, otherwise a new thread will be created.
				   WARNING: If using pool, task launching order is not guaranteed! So make sure task indexing passed in is copy on value!
	 * @param f
	 * @param args
	 * @return
	 */
	template <typename F, typename... Args>
	HLVM_NODISCARD HLVM_STATIC_FUNC auto Launch(EAsyncMode mode, F&& f, Args&&... args)
	{
		if (mode == EAsyncMode::PoolOrderless)
		{
			return FWorkStealThreadPool::Get()->EnqueueTask(FwdTemp<F>(f), FwdTemp<Args>(args)...);
		}
		else // Standalone Thread
		{
			using TaskRetType = std::invoke_result_t<F, Args...>;
			using TaskType = std::packaged_task<TaskRetType()>;

			auto task = TaskType(std::bind(FwdTemp<F>(f), FwdTemp<Args>(args)...));
			auto future = task.get_future();
			// No need manage launching thread, so detach
			std::thread(MoveTemp(task)).detach();
			return MoveTemp(future);
		}
	}
};
