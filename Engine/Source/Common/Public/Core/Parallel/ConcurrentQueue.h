
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
	 *  Head = Tail
	 *
	 *  Push 1
		OldHead1=null NewNode1=SomePtr
		Push 2
		OldHead2=null NewNode2=SomePtr

		Push 2 Interlock step1
		OldHead2 = Head = Tail
		Head = NewNode2

		Push 1 Interlock step1
		OldHead1 = Head = NewNode2
		Head = NewNode1

		Push 2 Interlock step2
		OldHead2->Next = Tail->Next = NewNode2

		Pop
		Tail->Next is poped
		delete Tail
		Tail = Tail->Next
		Tail->Item = Empty

		Push 1 Interlock step2
		OldHead1->Next = NewNode2->Next = Tail->Next = NewNode1
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
		Head = Tail = new QueueNode();

		if constexpr (bBlockPopOnEmpty)
		{
			Mutex = new std::mutex();
			CV = new std::condition_variable();
		}
	}

	~TConcurrentQueue() noexcept
	{
		if constexpr (bBlockPopOnEmpty)
		{
			delete Mutex;
			delete CV;
		}

		while (QueueNode* temp = Tail)
		{
			Tail = Tail->m_nextNode;
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
		HLVM_ASSERT(Tail->m_nextNode, TXT("Tail is null"));
		return Tail->m_nextNode->m_item;
	}

	bool PopFront(T& ret) noexcept
	{
		if constexpr (bBlockPopOnEmpty)
		{
			while (Empty() && !bStopFlagByUser)
			{
				std::unique_lock<std::mutex> lock(*Mutex);
				CV->wait(lock, [] {
					return true;
				});
			}
		}

		if (QueueNode* poped_node = Tail->m_nextNode)
		{
			if constexpr (IS_SC)
			{
				// Step1 swap tail pointer
				QueueNode* old_tail = Tail;
				Tail = poped_node;

				// Step2 assign value
				ret = MoveTemp(poped_node->m_item);

				// Step3 delete old tail
				delete old_tail;

				if constexpr (bCountSize)
				{
					Count.fetch_add(-1, std::memory_order_relaxed);
				}
				return true;
			}
			else
			{
				QueueNode* old_tail = Tail;
				// Step1 swap tail pointer
				if (old_tail->m_nextNode == poped_node
					&& FGenericPlatformAtomicPointer::AtomicCompareExchange(&Tail, &old_tail, poped_node))
				{
					// Step2 assign value
					ret = MoveTemp(poped_node->m_item);

					// Step3 delete old tail
					delete old_tail;

					if constexpr (bCountSize)
					{
						Count.fetch_add(-1, std::memory_order_relaxed);
					}
					return true;
				}
			}
		}

		return false;
	}

	bool Empty() const noexcept
	{
		return Tail->m_nextNode == nullptr;
	}

	/**
	 * This method is for debugging propose.
	 * User should use while(!Queue.ShouldStopPop()) to check queue should pop or not.
	 * @return size_t Number of elements in the queue
	 */
	size_t Num() const noexcept
		requires(bCountSize)
	{
		return static_cast<size_t>(Count.load(std::memory_order_relaxed));
	}

	void SignalStop() noexcept
	{
		bStopFlagByUser = true;
	}

	bool ShouldStopPop() const noexcept
	{
		if constexpr (bBlockPopOnEmpty)
		{
			return bStopFlagByUser && Empty();
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
			old_head = FGenericPlatformAtomicPointer::AtomicExchange(&Head, NewNode);
			// Step2, chain pointer
			FGenericPlatformAtomicPointer::AtomicExchange(&old_head->m_nextNode, NewNode);
		}
		else
		{
			// Step1, swap pointer
			old_head = Head;
			Head = NewNode;

			// Step2, chain pointer
			// Prevent compiler reordering step2 into step1
			ATOMIC_THREAD_FENCE();
			old_head->m_nextNode = NewNode;
		}

		if constexpr (bCountSize)
		{
			Count.fetch_add(1, std::memory_order_relaxed);
		}

		if constexpr (bBlockPopOnEmpty)
		{
			CV->notify_one(); // Notify the poping thread
		}
	}

private:
	/** Holds a pointer to the head (back) of the list. */
	HLVM_CACHE_ALIGN TAtomicPointer<QueueNode*> Head{ nullptr };
	/** Holds a pointer to the tail (front) of the list. */
	TAtomicPointer<QueueNode*> Tail{ nullptr };

	/** Mutex for blocking pop. */
	std::mutex*				 Mutex;
	std::condition_variable* CV;
	/** Whether the queue is quit by user. */
	BIT_FLAG(bStopFlagByUser){ false };

	/** Size of the queue. */
	std::atomic_int_fast32_t Count{ 0 };

#undef IS_MP
#undef IS_SC
};
