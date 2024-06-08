
/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "ParallelDefinition.h"
#include "Platform/GenericPlatformAtomicPointer.h"
#include "Core/Mallocator/PMR.h"

#include "Core/Assert.h"
#include "Template/ReferenceTemplate.tpp"

/**
 * Enumerates concurrent queue modes.
 */
enum class EConcurrentQueueMode : TUINT8
{
	Spsc, // Single Producer Single Consumer
	Mpsc, // Multiple Producer Single Consumer
	Mpmc, // Multiple Producer Multiple Consumer
};

namespace hlvm_private
{
	/**
	 * Actually use atomic pointer is significantly faster (2x) than using raw ptr,
	 * Try turn this on/off and compare TestParallel benchmark
	 */
#define QUEUE_NODE_USE_ATOMIC_PTR 1

	/**
	 * Structure for the internal linked list.
	 */
	template <typename T>
	struct MS_ALIGN(HLVM_MALLOC_ALIGNMENT) TQueueNode
	{
		TQueueNode() = default;
		explicit TQueueNode(T&& InItem) noexcept
			: mItem(FwdTemp<T>(InItem))
		{
		}
#if QUEUE_NODE_USE_ATOMIC_PTR
		TAtomicPointer<TQueueNode*> mNextNode;
#else
		TQueueNode* mNextNode{ nullptr };
#endif
		T mItem;
	} GCC_ALIGN(HLVM_MALLOC_ALIGNMENT);
} // namespace hlvm_private

/**
 * Lock-free concurrent queue, inspired by Unreal Engine's TQueue
 */
template <typename T,
	EConcurrentQueueMode Mode = EConcurrentQueueMode::Mpmc,
	bool				 bCountSize = false,
	// Default to std::allocator to use new/delete
	CPMRMallocator<hlvm_private::TQueueNode<T>> AllocatorType = TPMRStd<hlvm_private::TQueueNode<T>>>
class TConcurrentQueue
{
#define IS_MP (Mode == EConcurrentQueueMode::Mpsc || Mode == EConcurrentQueueMode::Mpmc)
#define IS_SC (Mode == EConcurrentQueueMode::Mpsc || Mode == EConcurrentQueueMode::Spsc)
	/*
	 *  Concurrent Queue : Emulation
	 *  mHead = mTail
	 *
	 *  Push 1
		OldHead1=null NewNode1=SomePtr
		Push 2
		OldHead2=null NewNode2=SomePtr

		Push 2 Interlock step1
		OldHead2 = mHead = mTail
		mHead = NewNode2

		Push 1 Interlock step1
		OldHead1 = mHead = NewNode2
		mHead = NewNode1

		Push 2 Interlock step2
		OldHead2->Next = mTail->Next = NewNode2

		Pop
		mTail->Next is poped
		delete mTail
		mTail = mTail->Next
		mTail->Item = Empty

		Push 1 Interlock step2
		OldHead1->Next = NewNode2->Next = mTail->Next = NewNode1
	 */

public:
	using ValueType = T;
	using QueueNode = hlvm_private::TQueueNode<T>;

	NOCOPYMOVE(TConcurrentQueue)
	TConcurrentQueue()
	{
		mHead = mTail = std::construct_at(R_C(QueueNode*, Mallocator.allocate()));
		HLVM_ASSERT(mHead.IsLockFree(), TXT("TAtomicPointer is not lock free"));
	}

	~TConcurrentQueue() noexcept
	{
		while (QueueNode* temp = mTail)
		{
			mTail = mTail->mNextNode;
			HLVM_ATOMIC_THREAD_FENCE();
			std::destroy_at(temp);
			Mallocator.deallocate(temp);
		}
	}

	/**
	 * ConcurrentQueue Push always success, so there is no different between TryPush or not,
	 * It is just compilance with other Queue interface
	 */
	template <bool bTryPush = false>
	bool Push(const T& item) noexcept
		requires(std::is_copy_constructible_v<T>)
	{
		if (bStopFlagByUser)
			HLVM_UNLIKELY
			{
				return false;
			}
		else
			HLVM_LIKELY
			{
				auto NewNode = std::construct_at(R_C(QueueNode*, Mallocator.allocate()), CopyTemp(item));
				PushInternal(NewNode);
				return true;
			}
	}

	/**
	 * ConcurrentQueue Push always success, so there is no different between TryPush or not
	 * It is just compilance with other Queue interface
	 */
	template <bool bTryPush = false>
	bool Push(T&& item) noexcept
		requires(std::is_move_constructible_v<T>)
	{
		if (bStopFlagByUser)
			HLVM_UNLIKELY
			{
				return false;
			}
		else
			HLVM_LIKELY
			{
				auto NewNode = std::construct_at(R_C(QueueNode*, Mallocator.allocate()), MoveTemp(item));
				PushInternal(NewNode);
				return true;
			}
	}

	/**
	 * This is not atomic! Use with caution!
	 * Also make sure that the queue is not empty before calling this function.
	 * @return T&
	 */
	T& PeekFront() noexcept
	{
		HLVM_ASSERT(mTail->mNextNode, TXT("Queue Tail is null"));
		return mTail->mNextNode->mItem;
	}

	/**
	 * @tparam bTryPop No blocking and will return false on empty, otherwise return poped result.
	 */
	template <bool bTryPop = true>
	bool PopFront(T& ret) noexcept
	{
		while (Empty() && !bStopFlagByUser)
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
					std::unique_lock<std::mutex> lock(mMutex);
					mCV.wait(lock, [] {
						return true;
					});
				}
			}
		}

		if (QueueNode* PopedNode = mTail->mNextNode)
		{
			if constexpr (IS_SC)
			{
				// Step1 swap tail pointer
				QueueNode* old_tail = mTail;
				mTail = PopedNode;

				// Step2 assign value
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
					HLVM_ASSERT(false, TXT("Type {} must be move or copy constructible"), TO_TCHAR_STR(typeid(T).name()));
				}

				// Step3 delete old tail
				std::destroy_at(old_tail);
				Mallocator.deallocate(old_tail);

				if constexpr (bCountSize)
				{
					mCount.fetch_sub(1, std::memory_order_relaxed);
				}
				return true;
			}
			else
			{
				QueueNode* old_tail = mTail;
				// Step1 swap tail pointer
				if (old_tail->mNextNode == PopedNode
					&& FGenericPlatformAtomicPointer::AtomicCompareExchange(&mTail, &old_tail, PopedNode))
				{
					// Step2 assign value
					if constexpr (std::is_move_constructible_v<T>)
					{
						ret = MoveTemp(PopedNode->mItem);
					}
					else if (std::is_copy_constructible_v<T>)
					{
						ret = CopyTemp(PopedNode->mItem);
					}
					else
					{
						HLVM_ASSERT(false, TXT("Type {} must be move or copy constructible"), TO_TCHAR_STR(typeid(T).name()));
					}

					// Step3 delete old tail
					std::destroy_at(old_tail);
					Mallocator.deallocate(old_tail);

					if constexpr (bCountSize)
					{
						mCount.fetch_sub(1, std::memory_order_relaxed);
					}
					return true;
				}
			}
		}

		/**
		 * return false on empty queue
		 */
		return false;
	}

	bool Empty() const noexcept
	{
		return mTail->mNextNode == nullptr;
	}

	/**
	 * This method is for debugging propose.
	 * User should use while(!Queue.ShouldStopPop()) to check queue should pop or not.
	 * @return size_t Number of elements in the queue
	 */
	size_t Num() const noexcept
		requires(bCountSize)
	{
		return mCount.load(std::memory_order_relaxed);
	}

	/**
	 * Use should call singla stop after all push finished,
	 * so that poping will not be blocked until queue is popped to empty
	 */
	void SignalStop() noexcept
	{
		bStopFlagByUser = true;
	}

	/**
	 * Use should stop poping instead of Empty in the poping whle loop condition
	 */
	bool ShouldStopPop() const noexcept
	{
		return bStopFlagByUser && Empty();
	}

private:
	void PushInternal(QueueNode* NewNode) noexcept
	{
		QueueNode* old_head;
		if constexpr (IS_MP)
		{
			// Step1, swap pointer
			old_head = FGenericPlatformAtomicPointer::AtomicExchange(&mHead, NewNode);
			// Step2, chain pointer
#if QUEUE_NODE_USE_ATOMIC_PTR
			FGenericPlatformAtomicPointer::AtomicExchange(&old_head->mNextNode, NewNode);
#else
			HLVM_ATOMIC_THREAD_FENCE();
			old_head->mNextNode = NewNode;
#endif
		}
		else
		{
			// Step1, swap pointer
			old_head = mHead;
			mHead = NewNode;

			// Step2, chain pointer
			// Prevent compiler reordering step2 into step1
			HLVM_ATOMIC_THREAD_FENCE();
			old_head->mNextNode = NewNode;
		}

		if constexpr (bCountSize)
		{
			mCount.fetch_add(1, std::memory_order_relaxed);
		}

		mCV.notify_one(); // Notify the poping thread
	}

private:
	/** Holds a pointer to the head (back) of the list. */
	HLVM_CACHE_ALIGN TAtomicPointer<QueueNode*> mHead;
	/** Holds a pointer to the tail (front) of the list. */
	TAtomicPointer<QueueNode*> mTail;

	/** mMutex for blocking pop. */
	std::mutex				mMutex;
	std::condition_variable mCV;

	/** Whether the queue is quit by user. */
	volatile bool bStopFlagByUser{ false };

	/** Size of the queue. */
	std::atomic_uint_fast32_t mCount{ 0 };

	AllocatorType Mallocator;

#undef IS_MP
#undef IS_SC
#undef QUEUE_NODE_USE_ATOMIC_PTR
};
