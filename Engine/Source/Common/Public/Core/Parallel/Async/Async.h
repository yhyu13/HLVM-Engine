/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#pragma once

#include "AsyncConfig.h"
#include "WorkStealThreadPool.h"
#include "Core/Parallel/Async/_Deprecated/WorkStealFiberPool.h"

#include <future>

DECLARE_LOG_CATEGORY(LogAsync)

HLVM_ENUM(EAsyncMode, TUINT8,
	// Add to default work steal thread pool (task execution order not guaranteed!
	// As parallelism does not work with sequential execution in mind. So make sure tasks and variables passed in are not order dependent when launching)
	// Especially loop index should be copied on value!
	PoolOrderlessExec,
	StandAlone // Startup a new thread
);

class FAsync
{
public:
	/**
	 * Launch a task
	 * @param mode If using pool, task will be launched in a thread from the default work steal thread pool, otherwise a new thread will be created.
				   WARNING: If using pool, task launching order is not guaranteed! So make sure task indexing passed in is copy on value!
	 * @return std::future
	 */
	template <typename F, typename... Args>
	HLVM_NODISCARD HLVM_STATIC_FUNC auto Launch(EAsyncMode mode, F&& f, Args&&... args)
	{
		bool bCanUsePool = mode == EAsyncMode::PoolOrderlessExec;
		if (bCanUsePool)
		{
			bCanUsePool = FWorkerPoolTIDUtil::IsThreadValidToUseWorkerPool();
			if (!bCanUsePool)
			{
				HLVM_LOG(LogAsync, critical, TXT("Launch cannot use pool, thread {} is not valid to use pool!"), GCurrentTID64);
			}
		}

		if (bCanUsePool)
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
