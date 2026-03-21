
/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#pragma once
#include "ParallelDefinition.h"
#include "Lock.h"
#include "Core/Assert.h"
#include "Template/ReferenceTemplate.tpp"

/**
 * Fixed size concurrent queue, inspired by boost fiber channel_buffer
 */
template <typename T,
	TUINT16 N>
class TFixedSizeQueue : public FAtomicFlagNC
{
private:
	/**
	 * Structure for the internal linked list.
	 */
	struct QueueNode
	{
		QueueNode() = default;
		explicit QueueNode(T&& InItem) noexcept
			: mItem(FwdTemp<T>(InItem))
		{
		}
		T mItem;
	};

public:
	using ValueType = T;

	NOCOPYMOVE(TFixedSizeQueue);
	TFixedSizeQueue() = default;
	~TFixedSizeQueue() noexcept = default;

	/**
	 *  Copy item to queue, block on full
	 */
	template <bool bTryPush = false>
	bool Push(const T& item) noexcept
		requires(std::is_copy_constructible_v<T>)
	{
		// Block on full
		while (IsFullInternal() && !bStopFlagByUser)
		{
			if constexpr (bTryPush)
			{
				return false;
			}
			else
			{
				std::this_thread::yield();
				std::unique_lock<std::mutex> lock(mMutexPush);
				mCVPush.wait(lock, [] {
					return true;
				});
			}
		}

		if (bStopFlagByUser)
			HLVM_UNLIKELY
			{
				return false;
			}
		else
			HLVM_LIKELY
			{
				LOCK_GUARD_NC();
				PushInternal(MoveTemp(QueueNode(CopyTemp(item))));
				return true;
			}
	}

	/**
	 *  Move item to queue, block on full
	 */
	template <bool bTryPush = false>
	bool Push(T&& item) noexcept
		requires(std::is_move_constructible_v<T>)
	{
		// Block on full
		while (IsFullInternal() && !bStopFlagByUser)
		{
			if constexpr (bTryPush)
			{
				return false;
			}
			else
			{
				std::this_thread::yield();
				std::unique_lock<std::mutex> lock(mMutexPush);
				mCVPush.wait(lock, [] {
					return true;
				});
			}
		}

		if (bStopFlagByUser)
			HLVM_UNLIKELY
			{
				return false;
			}
		else
			HLVM_LIKELY
			{
				LOCK_GUARD_NC();
				PushInternal(MoveTemp(QueueNode(MoveTemp(item))));
				return true;
			}
	}

	/**
	 * Make sure that the queue is not empty before calling this function.
	 * @return T&
	 */
	T& PeekFront() noexcept
	{
		LOCK_GUARD_NC();
		HLVM_ASSERT_F(!IsEmptyInternal(), TXT("Queue Tail is null"));
		return QueueNodes[mTail].mItem;
	}

	/**
	 * @tparam bTryPop No blocking and will return false on empty, otherwise return poped result.
	 */
	template <bool bTryPop = true>
	bool PopFront(T& ret) noexcept
	{
		while (IsEmptyInternal() && !bStopFlagByUser)
		{
			/**
			 * If only try pop, we should immediate exit with false on empty queue
			 */
			if constexpr (bTryPop)
			{
				return false;
			}
			else
			{
				std::this_thread::yield();
				{
					std::unique_lock<std::mutex> lock(mMutexPop);
					mCVPop.wait(lock, [] {
						return true;
					});
				}
			}
		}

		LOCK_GUARD_NC();
		if (!IsEmptyInternal())
		{
			QueueNode* PopedNode = &QueueNodes[mTail];
			{
				// Step1 assign value
				if constexpr (std::is_move_constructible_v<T>)
				{
					ret = MoveTemp(PopedNode->mItem);
				}
				else if constexpr (std::is_copy_constructible_v<T>)
				{
					ret = CopyTemp(PopedNode->mItem);
				}
				else
				{
					HLVM_ASSERT_F(false, TXT("Type {} must be move or copy constructible"), TO_TCHAR_CSTR(typeid(T).name()));
				}

				// Step2 Move tail
				mTail = (mTail + 1) % N;

				// Step3 Decrease count
				mCount.fetch_sub(1, std::memory_order_relaxed);

				// Notify push
				mCVPush.notify_one();

				return true;
			}
		}

		/**
		 * return false on empty queue
		 */
		return false;
	}

	/**
	 * This method is not for stopping criteria,
	 * User should use while(!Queue.ShouldStopPop()) to check queue should pop or not.
	 */
	bool IsEmpty() const noexcept
	{
		return IsEmptyInternal();
	}

	/**
	 * This method is not for stopping criteria,
	 * User should use while(!Queue.ShouldStopPop()) to check queue should pop or not.
	 * @return size_t Number of elements in the queue
	 */
	size_t Num() const noexcept
	{
		return mCount.load(std::memory_order_relaxed);
	}

	/**
	 * Call signal stop after all push finished,
	 * so that poping will not be blocked until queue is popped to empty
	 */
	void SignalStop() noexcept
	{
		LOCK_GUARD_NC();
		bStopFlagByUser = true;
	}

	/**
	 * Use should stop poping instead of IsEmpty in the poping for loop condition
	 */
	bool ShouldStopPop() const noexcept
	{
		LOCK_GUARD_NC();
		return bStopFlagByUser && IsEmptyInternal();
	}

private:
	bool IsEmptyInternal() const noexcept
	{
		return mCount.load(std::memory_order_relaxed) == 0;
	}

	bool IsFullInternal() const noexcept
	{
		return mCount.load(std::memory_order_relaxed) == N;
	}

	void PushInternal(QueueNode&& NewNode) noexcept
	{
		// Step1 assign value
		QueueNodes[mHead] = MoveTemp(NewNode);

		// Step2 Move head
		mHead = (mHead + 1) % N;

		// Step3 Increase count
		mCount.fetch_add(1, std::memory_order_relaxed);

		// Notify the poping thread
		mCVPop.notify_one();
	}

private:
	QueueNode QueueNodes[N]{};

	/** Holds a index to the head (back) of the list. */
	TUINT16 mHead{ 0 };
	/** Holds a index to the tail (front) of the list. */
	TUINT16 mTail{ 0 };

	/** Mutex for blocking push. */
	std::mutex				mMutexPush;
	std::condition_variable mCVPush;

	/** Mutex for blocking pop. */
	std::mutex				mMutexPop;
	std::condition_variable mCVPop;

	// Count number of elements in the queue
	std::atomic_uint_fast16_t mCount{ 0 };

	/** Whether the queue is quit by user. */
	BIT_FLAG(bStopFlagByUser){ false };
};
