/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "Core/Mallocator/StackMallocator.h"
#include "Core/Parallel/Lock.h"

#include "VMMallocatorDefinition.h"

#if HLVM_MALLOC_USE_MIMALLOC_OVER_STD
	#include "Core/Mallocator/MiMallocator.h"
	#define GMALLOCATOR GMiMallocatorTLS
#else
	#include "Core/Mallocator/StdMallocator.h"
	#define GMALLOCATOR GStdMallocator
#endif

class FOSPageMallocator
{
	HLVM_INLINE_VAR HLVM_STATIC_VAR constexpr bool bValidate = HLVM_MALLOC_VALIDATION;

public:
	NOCOPYMOVE(FOSPageMallocator)

	FOSPageMallocator() noexcept
	{
		mSmallAllocatorHead = new (GMALLOCATOR.MallocAligned(sizeof(FSmallAllocator), sizeof(FSmallAllocator))) FSmallAllocator();
		mLargeHeapChainHead = R_C(FLargeHeapChain*, GMALLOCATOR.Malloc(sizeof(FLargeHeapChain)));
	}

	~FOSPageMallocator() noexcept
	{
		auto Small = mSmallAllocatorHead;
		while (Small)
		{
			Small->~FSmallAllocator();
			HLVM_ENSURE(GMALLOCATOR.FreeAligned(Small, sizeof(FSmallAllocator)) == EFreeRetType::Success,
				TXT("~FOSPageMallocator failed {}"), R_C(void*, Small));
			Small = Small->Next;
		}

		auto Large = mLargeHeapChainHead;
		while (Large)
		{
			auto Next = Large->Next;
			HLVM_ENSURE(GMALLOCATOR.FreeAligned(Large->Heap, sizeof(FLargeHeap)) == EFreeRetType::Success,
				TXT("~FOSPageMallocator failed {}"), R_C(void*, Small));
			HLVM_ENSURE(GMALLOCATOR.Free(Large) == EFreeRetType::Success,
				TXT("~FOSPageMallocator failed {}"), R_C(void*, Small));
			Large = Next;
		}
	}

	bool Owned(void* ptr) const noexcept
	{
		LOCK_GUARD_RIVAL(mRWLock, 0);
		auto Large = mLargeHeapChainHead;
		while (Large)
		{
			if (Large->Heap->Owned(ptr))
			{
				return true;
			}
			Large = Large->Next;
		}

		auto Small = mSmallAllocatorHead;
		while (Small)
		{
			if (Small->StackMallocator.Owned(ptr))
			{
				return true;
			}
			Small = Small->Next;
		}
		return false;
	}

	/**
	 *  Allocate memory from small heap with very limit size (<4K), if fail, return nullptr
	 */
	void* MallocSmall(size_t size)
	{
		void* p = nullptr;
		auto  Small = mSmallAllocatorHead;
		while (!p && Small)
		{
			p = Small->StackMallocator.Malloc(size);
			if (!p)
			{
				if (!Small->Next)
				{
					LOCK_GUARD_RIVAL(mRWLock, 1);
					/**
					 *  Allocate new small heap, and ensure we can allocate enough memory right away
					 */
					Small->Next = R_C(FSmallAllocator*, GMALLOCATOR.MallocAligned(sizeof(FSmallAllocator), sizeof(FSmallAllocator)));
					Small = Small->Next;
					p = Small->StackMallocator.Malloc(size);
					HLVM_CONSTEXPR_ASSERT(bValidate, p != nullptr);
					return p;
				}
				else
				{
					Small = Small->Next;
				}
			}
		}
		HLVM_CONSTEXPR_ASSERT(bValidate, p != nullptr);
		return p;
	}

	void FreeSmall(void* ptr)
	{
		auto Small = mSmallAllocatorHead;
		while (Small)
		{
			if (Small->StackMallocator.Owned(ptr))
			{
				HLVM_ENSURE(Small->StackMallocator.Free(ptr) == EFreeRetType::Success,
					TXT("FreeSmall failed {}"), R_C(void*, Small));
				return;
			}
			Small = Small->Next;
		}
	}

	void* MallocLargeHeap()
	{
		void* p = nullptr;
		auto  Large = mLargeHeapChainHead;
		while (!p && Large)
		{
			if (!Large->Heap)
			{
				p = Large->Heap = R_C(FLargeHeap*, GMALLOCATOR.MallocAligned(sizeof(FLargeHeap), sizeof(FLargeHeap)));
				HLVM_CONSTEXPR_ASSERT(bValidate, p != nullptr);
			}
			else
			{
				if (!Large->Next)
				{
					LOCK_GUARD_RIVAL(mRWLock, 1);
					Large->Next = R_C(FLargeHeapChain*, GMALLOCATOR.Malloc(sizeof(FLargeHeapChain)));
				}
				Large = Large->Next;
			}
		}
		HLVM_CONSTEXPR_ASSERT(bValidate, p != nullptr);
		return p;
	}

	void FreeLargeHeap(void* ptr)
	{
		bool bFound = false;
		auto Large = mLargeHeapChainHead;
		while (!bFound && Large)
		{
			if (Large->Heap == ptr)
			{
				HLVM_ENSURE(GMALLOCATOR.FreeAligned(ptr, sizeof(FLargeHeap)) == EFreeRetType::Success,
					TXT("FreeLargeHeap failed {}"), R_C(void*, ptr));
				Large->Heap = nullptr;
				bFound = true;
			}
			Large = Large->Next;
		}
		HLVM_CONSTEXPR_ASSERT(bValidate, bFound);
	}

private:
	struct FSmallAllocator
	{
		TStackMallocator<HLVM_VMA_SMALL_HEAP_SIZE - 48, false, true, false, false> StackMallocator;
		FSmallAllocator*														   Next{};
	};
	static_assert(sizeof(FSmallAllocator) == HLVM_VMA_SMALL_HEAP_SIZE, "SmallHeap size must be HLVM_VMA_SMALL_HEAP_SIZE");
	/**
	 *  Small heap chain
	 */
	FSmallAllocator* mSmallAllocatorHead{ nullptr };

	struct FLargeHeap
	{
		TBYTE Data[HLVM_VMA_LARGE_HEAP_SIZE];

		bool Owned(void* ptr) const
		{
			return ptr >= Data && ptr < Data + sizeof(Data);
		}
	};
	static_assert(sizeof(FLargeHeap) == HLVM_VMA_LARGE_HEAP_SIZE, "FLargeHeap size must be HLVM_VMA_LARGE_HEAP_SIZE");

	struct FLargeHeapChain
	{
		FLargeHeap*		 Heap{ nullptr };
		FLargeHeapChain* Next{ nullptr };
	};
	/**
	 * Large heap chain
	 */
	FLargeHeapChain* mLargeHeapChainHead{ nullptr };

	/**
	 * Read write mutex lock
	 */
	mutable FRWRivalLock mRWLock;
};

#undef GMALLOCATOR
