
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
		auto& Queue = mTaskQueues[HLVM_E2VALUE(Priority)];
		const bool bPushed = Queue.template Push<bTryPush>(MoveTemp(Task));
		if (bPushed)
		{
			mCV.notify_one();
		}
		return bPushed;
	}

	/**
	 * Push tasks with Default priority
	 */
	template <bool bTryPush = false>
	bool PushIfEmpty(TaskType&& Task) noexcept
		requires(std::is_move_constructible_v<TaskType>)
	{
		return PushIfEmpty(ETaskPriority::Default, FwdTemp<TaskType>(Task));
	}

	/**
	 * Push tasks with specific priority only if queue is empty
	 */
	template <bool bTryPush = false>
	bool PushIfEmpty(ETaskPriority Priority, TaskType&& Task) noexcept
		requires(std::is_move_constructible_v<TaskType>)
	{
		auto& Queue = mTaskQueues[HLVM_E2VALUE(Priority)];
		if (Queue.IsEmpty())
		{
			const bool bPushed = Queue.template Push<bTryPush>(MoveTemp(Task));
			if (bPushed)
			{
				// If push success, but we are in a race condition, pop one and return false
				if (Queue.NumGreaterThanOne())
				{
					TaskType _wasted;
					Queue.PopFront(_wasted);
					return false;
				}

				mCV.notify_one();
			}
			return bPushed;
		}
		return false;
	}

	/**
	 * Pop tasks with specific priority
	 */
	template <bool bTryPop = true>
	bool PopFront(ETaskPriority Priority, TaskType& Task, std::chrono::milliseconds Timeout = std::chrono::milliseconds::zero()) noexcept
	{
		auto& Queue = mTaskQueues[HLVM_E2VALUE(Priority)];
		if (Queue.template PopFront<bTryPop>(Task, Timeout))
		{
			return true;
		}
		return false;
	}

	/**
	 * Pop tasks from higher priority to lower
	 */
	template <bool bTryPop = true>
	bool PopFront(TaskType& Task, std::chrono::milliseconds Timeout = std::chrono::milliseconds::zero()) noexcept
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
				if (Timeout > std::chrono::milliseconds::zero())
				{
					auto endTime = std::chrono::steady_clock::now() + Timeout;
					auto res = mCV.wait_until(lock, endTime);
					if (res == std::cv_status::timeout)
					{
						// Add Timeout logic here
						return false;
					}
				}
				else
				{
					mCV.wait(lock, [] {
						return true;
					});
				}
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

	bool IsEmpty(ETaskPriority Priority) const noexcept
	{
		auto& Queue = mTaskQueues[HLVM_E2VALUE(Priority)];
		return Queue.IsEmpty();
	}

private:
	TConcurrentQueue<TaskType, EConcurrentQueueMode::Mpmc, false> mTaskQueues[ETaskPriority_NUM];

	/** mMutex for blocking pop. */
	std::mutex				mMutex;
	std::condition_variable mCV;
};
