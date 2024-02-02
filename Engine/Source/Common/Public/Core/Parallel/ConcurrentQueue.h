
/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once
#include "ParallelDefinition.h"
#include "Platform/GenericPlatformAtomicPointer.h"
#include "Core/Assert.h"
#include "Template/GlobalTemplate.tpp"

#include <utility>

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
template <typename T, EConcurrentQueueMode Mode = EConcurrentQueueMode::Mpmc, bool bCountSize = false>
class FConcurrentQueue
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
	FConcurrentQueue()
	{
		m_head = m_tail = new QueueNode();
	}

	~FConcurrentQueue() noexcept
	{
		while (QueueNode* temp = m_tail)
		{
			m_tail = m_tail->m_nextNode;
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
	 * @return T&
	 */
	T& PeekFront() const noexcept
	{
		HLVM_ASSERT(m_tail->m_nextNode, TXT("Tail is null"));
		return m_tail->m_nextNode->m_item;
	}

	bool PopFront(T& ret) noexcept
	{
		if (QueueNode* poped_node = m_tail->m_nextNode)
		{
			if constexpr (Mode == EConcurrentQueueMode::Mpsc || Mode == EConcurrentQueueMode::Spsc)
			{
				// Step1 swap tail pointer
				QueueNode* old_tail = m_tail;
				m_tail = poped_node;

				// Step2 assign value
				ret = MoveTemp(poped_node->m_item);

				// Step3 delete old tail
				delete old_tail;

				if constexpr (bCountSize)
				{
					m_size.fetch_add(-1, std::memory_order_relaxed);
				}
				return true;
			}
			else
			{
				QueueNode* old_tail = m_tail;
				// Step1 swap tail pointer
				if (old_tail->m_nextNode == poped_node
					&& FGenericPlatformAtomicPointer::AtomicCompareExchange(&m_tail, &old_tail, poped_node))
				{
					// Step2 assign value
					ret = MoveTemp(poped_node->m_item);

					// Step3 delete old tail
					delete old_tail;

					if constexpr (bCountSize)
					{
						m_size.fetch_add(-1, std::memory_order_relaxed);
					}
					return true;
				}
			}
		}

		return false;
	}

	bool Empty() const noexcept
	{
		return m_tail->m_nextNode == nullptr;
	}

	/**
	 * This method is debugging propose. Should use while(!Queue.Empty()) to check queue is empty or not.
	 * @return
	 */
	size_t Num() const noexcept
		requires(bCountSize == true)
	{
		return m_size.load(std::memory_order_relaxed);
	}

private:
	void push_internal(QueueNode* NewNode) noexcept
	{
		QueueNode* old_head;
		if constexpr (Mode == EConcurrentQueueMode::Mpsc || Mode == EConcurrentQueueMode::Mpmc)
		{
			// Step1, swap pointer
			old_head = FGenericPlatformAtomicPointer::AtomicExchange(&m_head, NewNode);
			// Step2, chain pointer
			FGenericPlatformAtomicPointer::AtomicExchange(&old_head->m_nextNode, NewNode);
		}
		else
		{
			// Step1, swap pointer
			old_head = m_head;
			m_head = NewNode;

			// Step2, chain pointer
			// Prevent compiler reordering step2 into step1
			ATOMIC_THREAD_FENCE();
			old_head->m_nextNode = NewNode;
		}

		if constexpr (bCountSize)
		{
			m_size.fetch_add(1, std::memory_order_relaxed);
		}
	}

private:
	/** Holds a pointer to the head (back) of the list. */
	HLVM_CACHE_ALIGN TAtomicPointer<QueueNode*> m_head{ nullptr };
	/** Holds a pointer to the tail (front) of the list. */
	TAtomicPointer<QueueNode*> m_tail{ nullptr };

	std::atomic_int32_t m_size{ 0 };

#undef IS_MP
#undef IS_SC
};
