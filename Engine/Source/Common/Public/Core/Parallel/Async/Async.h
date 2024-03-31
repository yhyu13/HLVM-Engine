/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "AsyncConfig.h"
#include "WorkStealThreadPool.h"
#include "WorkStealFiberPool.h"

#include <future>

HLVM_ENUM(EAsyncMode, TUINT8,
	ThreadPool, // Add to default work steal thread pool
	Thread		// Startup a new thread
);

class FAsync
{
public:
	template <typename F, typename... Args>
	HLVM_NODISCARD HLVM_STATIC_FUNC auto Launch(EAsyncMode mode, F&& f, Args&&... args)
	{
		if (mode == EAsyncMode::ThreadPool)
		{
			return FWorkStealThreadPool::Get()->EnqueuTask(FwdTemp<F>(f), FwdTemp<Args>(args)...);
		}
		else // Thread
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
