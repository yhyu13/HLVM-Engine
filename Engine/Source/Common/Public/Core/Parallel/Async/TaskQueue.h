
/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "Core/Parallel/ConcurrentQueue.h"
#include "AsyncConfig.h"

/**
 * Queue specialized for pushing and poping prioritized tasks
 */
template <typename TaskType>
class TTaskQueue
{
public:
	NOCOPYMOVE(TTaskQueue)
	TTaskQueue() = default;
	~TTaskQueue() = default;

	/**
	 * Push tasks with Default priority
	 */
	template <bool bTryPush = false>
	bool Push(TaskType&& Task) noexcept
		requires(std::is_move_constructible_v<TaskType>)
	{
		return Push(ETaskPriority::Default, FwdTemp<TaskType>(Task));
	}

	/**
	 * Push tasks with specific priority
	 */
	template <bool bTryPush = false>
	bool Push(ETaskPriority Priority, TaskType&& Task) noexcept
		requires(std::is_move_constructible_v<TaskType>)
	{
		const bool bPushed = mTaskQueues[HLVM_ENUM_VALUE(Priority)].template Push<bTryPush>(MoveTemp(Task));
		if (bPushed)
		{
			mCV.notify_one();
		}
		return bPushed;
	}

	/**
	 * Pop tasks with specific priority
	 */
	template <bool bTryPop = true>
	bool PopFront(ETaskPriority Priority, TaskType& Task) noexcept
	{
		if (mTaskQueues[Priority].template PopFront<bTryPop>(Task))
		{
			return true;
		}
		return false;
	}

	/**
	 * Pop tasks from higher priority to lower
	 */
	template <bool bTryPop = true>
	bool PopFront(TaskType& Task) noexcept
	{
	TRY_POP:
		// Regardless "bTryPop" is true or not, try to pop from higher priority to lower
		for (int i = ETaskPriority_NUM - 1; i >= 0; --i)
		{
			if (mTaskQueues[i].template PopFront<true>(Task))
			{
				return true;
			}
		}

		if constexpr (!bTryPop)
		{
			// Yield and then block until task queue is not empty
			std::this_thread::yield();
			{
				std::unique_lock<std::mutex> lock(mMutex);
				mCV.wait(lock, [] {
					return true;
				});
			}
			// if not stop pop, try pop again
			if (!ShouldStopPop())
			{
				goto TRY_POP;
			}
		}
		return false;
	}

	/**
	 * Check if should stop pop, only stop when all priority queue should stop pop
	 * @return true if should stop pop
	 */
	bool ShouldStopPop() const noexcept
	{
		bool bShouldStop = true;
		for (int i = ETaskPriority_NUM - 1; i >= 0; --i)
		{
			bShouldStop &= mTaskQueues[i].ShouldStopPop();
			if (!bShouldStop)
			{
				break;
			}
		}
		return bShouldStop;
	}

	void SignalStop() noexcept
	{
		for (int i = ETaskPriority_NUM - 1; i >= 0; --i)
		{
			mTaskQueues[i].SignalStop();
		}
	}

private:
	TConcurrentQueue<TaskType, EConcurrentQueueMode::Mpmc, false> mTaskQueues[ETaskPriority_NUM];

	/** mMutex for blocking pop. */
	std::mutex				mMutex;
	std::condition_variable mCV;
};
