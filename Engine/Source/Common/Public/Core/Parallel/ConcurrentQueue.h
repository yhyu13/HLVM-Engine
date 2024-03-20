
/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once
#include "ParallelDefinition.h"
#include "Platform/GenericPlatformAtomicPointer.h"
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

/**
 * Noncopyable lock-free concurrent queue, copy from Unreal Engine's TQueue
 */
template <typename T,
	EConcurrentQueueMode Mode = EConcurrentQueueMode::Mpmc,
	bool				 bBlockPopOnEmpty = true, // Applying block on pop when empty actually makes the queue performance 1.25 faster on the TestParallel benchmark
	bool				 bCountSize = false>
class TConcurrentQueue
{
#define IS_MP (Mode == EConcurrentQueueMode::Mpsc || Mode == EConcurrentQueueMode::Mpmc)
#define IS_SC (Mode == EConcurrentQueueMode::Mpsc || Mode == EConcurrentQueueMode::Spsc)
	/**
	 * Actually use atomic pointer is significantly faster (2x) than using raw ptr,
	 * Try turn this on/off and compare TestParallel benchmark
	 */
#define QUEUE_NODE_USE_ATOMIC_PTR 1

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
private:
	/**
	 * Structure for the internal linked list.
	 */
	struct QueueNode
	{
		QueueNode() = default;

		explicit QueueNode(const T& InItem) noexcept
			: mItem(InItem)
		{
		}

		explicit QueueNode(T&& InItem) noexcept
			: mItem(InItem)
		{
		}
#if QUEUE_NODE_USE_ATOMIC_PTR
		~QueueNode() noexcept
		{
			/**
			 * Call Release manually to avoid delete mNextNode
			 * which might trigger recursive delete on chained QueueNode* and their mNextNode)
			 */
			mNextNode.Release();
		}
		TAtomicPointer<QueueNode*> mNextNode;
#else
		QueueNode* mNextNode{ nullptr };
#endif
		T mItem;
	};

public:
	NOCOPYMOVE(TConcurrentQueue)

	TConcurrentQueue()
	{
		auto temp = new QueueNode();
		mHead = mTail = temp;

		if constexpr (bBlockPopOnEmpty)
		{
			mMutex = new std::mutex();
			mCV = new std::condition_variable();
		}
	}

	~TConcurrentQueue() noexcept
	{
		if constexpr (bBlockPopOnEmpty)
		{
			delete mMutex;
			delete mCV;
		}

		while (QueueNode* temp = mTail)
		{
			mTail = mTail->mNextNode;
			ATOMIC_THREAD_FENCE();
			delete temp;
		}
	}

	void Push(const T& item) noexcept
		requires(std::is_copy_constructible_v<T>)
	{
		PushInternal(new QueueNode(CopyTemp(item)));
	}

	void Push(T&& item) noexcept
		requires(std::is_move_constructible_v<T>)
	{
		PushInternal(new QueueNode(MoveTemp(item)));
	}

	/**
	 * This is not atomic! Use with caution!
	 * Also make sure that the queue is not empty before calling this function.
	 * @return T&
	 */
	T& PeekFront() const noexcept
	{
		HLVM_ASSERT(mTail->mNextNode, TXT("Queue Tail is null"));
		return mTail->mNextNode->mItem;
	}

	template <bool bTryPop = false>
	bool PopFront(T& ret) noexcept
	{
		if constexpr (bBlockPopOnEmpty)
		{
			while (Empty() && !mbStopFlagByUser)
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
					std::unique_lock<std::mutex> lock(*mMutex);
					mCV->wait(lock, [] {
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
				else if (std::is_copy_constructible_v<T>)
				{
					ret = CopyTemp(PopedNode->mItem);
				}
				else
				{
					assert(false);
				}

				// Step3 delete old tail
				delete old_tail;

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
						assert(false);
					}

					// Step3 delete old tail
					delete old_tail;

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
		return static_cast<size_t>(mCount.load(std::memory_order_relaxed));
	}

	/**
	 * Use should call singla stop after all push finished,
	 * so that poping will not be blocked until queue is popped to empty
	 */
	void SignalStop() noexcept
		requires(bBlockPopOnEmpty)
	{
		mbStopFlagByUser = true;
	}

	bool ShouldStopPop() const noexcept
	{
		if constexpr (bBlockPopOnEmpty)
		{
			return mbStopFlagByUser && Empty();
		}
		else
		{
			return Empty();
		}
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
			ATOMIC_THREAD_FENCE();
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
			ATOMIC_THREAD_FENCE();
			old_head->mNextNode = NewNode;
		}

		if constexpr (bCountSize)
		{
			mCount.fetch_add(1, std::memory_order_relaxed);
		}

		if constexpr (bBlockPopOnEmpty)
		{
			mCV->notify_one(); // Notify the poping thread
		}
	}

private:
	/** Holds a pointer to the head (back) of the list. */
	HLVM_CACHE_ALIGN TAtomicPointer<QueueNode*> mHead;
	/** Holds a pointer to the tail (front) of the list. */
	TAtomicPointer<QueueNode*> mTail;

	/** mMutex for blocking pop. */
	std::mutex*				 mMutex;
	std::condition_variable* mCV;
	/** Whether the queue is quit by user. */
	BIT_FLAG(mbStopFlagByUser){ false };

	/** Size of the queue. */
	std::atomic_uint_fast32_t mCount{ 0 };

#undef IS_MP
#undef IS_SC
#undef QUEUE_NODE_USE_ATOMIC_PTR
};
