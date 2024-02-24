
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
enum class EConcurrentQueueMode : uint8_t
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
#define IS_MP Mode == EConcurrentQueueMode::Mpsc || Mode == EConcurrentQueueMode::Mpmc
#define IS_SC Mode == EConcurrentQueueMode::Mpsc || Mode == EConcurrentQueueMode::Spsc
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
			: m_item(CopyTemp(InItem))
		{
		}

		explicit QueueNode(T&& InItem) noexcept
			: m_item(MoveTemp(InItem))
		{
		}

		~QueueNode() noexcept
		{
			/**
			 * Call Release manually to avoid delete m_nextNode
			 * which might trigger recursive delete on chained QueueNode* and their m_nextNode)
			 */
			m_nextNode.Release();
		}

		TAtomicPointer<QueueNode*> m_nextNode{ nullptr };
		T						   m_item;
	};

public:
	TConcurrentQueue()
	{
		mHead = mTail = new QueueNode();

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
			mTail = mTail->m_nextNode;
			ATOMIC_THREAD_FENCE();
			delete temp;
		}
	}

	void Push(const T& item) noexcept
	{
		push_internal(new QueueNode(item));
	}

	void Push(T&& item) noexcept
	{
		push_internal(new QueueNode(item));
	}

	/**
	 * This is not atomic! Use with caution!
	 * Also make sure that the queue is not empty before calling this function.
	 * @return T&
	 */
	T& PeekFront() const noexcept
	{
		HLVM_ASSERT(mTail->m_nextNode, TXT("mTail is null"));
		return mTail->m_nextNode->m_item;
	}

	bool PopFront(T& ret) noexcept
	{
		if constexpr (bBlockPopOnEmpty)
		{
			while (Empty() && !mbStopFlagByUser)
			{
				std::unique_lock<std::mutex> lock(*mMutex);
				mCV->wait(lock, [] {
					return true;
				});
			}
		}

		if (QueueNode* poped_node = mTail->m_nextNode)
		{
			if constexpr (IS_SC)
			{
				// Step1 swap tail pointer
				QueueNode* old_tail = mTail;
				mTail = poped_node;

				// Step2 assign value
				ret = MoveTemp(poped_node->m_item);

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
				if (old_tail->m_nextNode == poped_node
					&& FGenericPlatformAtomicPointer::AtomicCompareExchange(&mTail, &old_tail, poped_node))
				{
					// Step2 assign value
					ret = MoveTemp(poped_node->m_item);

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

		return false;
	}

	bool Empty() const noexcept
	{
		return mTail->m_nextNode == nullptr;
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
	void push_internal(QueueNode* NewNode) noexcept
	{
		QueueNode* old_head;
		if constexpr (IS_MP)
		{
			// Step1, swap pointer
			old_head = FGenericPlatformAtomicPointer::AtomicExchange(&mHead, NewNode);
			// Step2, chain pointer
			FGenericPlatformAtomicPointer::AtomicExchange(&old_head->m_nextNode, NewNode);
		}
		else
		{
			// Step1, swap pointer
			old_head = mHead;
			mHead = NewNode;

			// Step2, chain pointer
			// Prevent compiler reordering step2 into step1
			ATOMIC_THREAD_FENCE();
			old_head->m_nextNode = NewNode;
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
	HLVM_CACHE_ALIGN TAtomicPointer<QueueNode*> mHead{ nullptr };
	/** Holds a pointer to the tail (front) of the list. */
	TAtomicPointer<QueueNode*> mTail{ nullptr };

	/** mMutex for blocking pop. */
	std::mutex*				 mMutex;
	std::condition_variable* mCV;
	/** Whether the queue is quit by user. */
	BIT_FLAG(mbStopFlagByUser){ false };

	/** Size of the queue. */
	std::atomic_uint_fast32_t mCount{ 0 };

#undef IS_MP
#undef IS_SC
};
