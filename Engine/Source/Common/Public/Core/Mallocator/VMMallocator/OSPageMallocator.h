/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "Core/Mallocator/PMR.h"
#include "Core/Mallocator/StackMallocator.h"

#include "VMMallocatorDefinition.h"

/*
 *  OS page mallocator based on low level mallocator
 *  1, manage malloc of small heap of size HLVM_VMA_OSPAGE_SMALL_HEAP_SIZE
 *  2, manage malloc of large heap of size HLVM_VMA_OSPAGE_LARGE_HEAP_SIZE
 *  3, wrapper of low level mallocator's align mallocation
 */
class FOSPageMallocator
{
	HLVM_INLINE_VAR HLVM_STATIC_VAR constexpr bool bValidate = HLVM_MALLOC_VALIDATION;

public:
	HLVM_INLINE_VAR HLVM_STATIC_VAR constexpr size_t sLargeHeapPageSize = HLVM_VMA_OSPAGE_LARGE_HEAP_SIZE;

	NOCOPYMOVE(FOSPageMallocator)

	FOSPageMallocator() noexcept
	{
		mSmallHeapChainHead = std::construct_at(R_C(FSmallHeapChain*, HLVM_LOW_GMALLOC_TLS.MallocAligned(sizeof(FSmallHeapChain), sizeof(FSmallHeapChain))));
		mLargeHeapChainHead = std::construct_at(R_C(FLargeHeapChain*, HLVM_LOW_GMALLOC_TLS.Malloc(sizeof(FLargeHeapChain))));
	}

	~FOSPageMallocator() noexcept
	{
		{
			auto Small = mSmallHeapChainHead;
			while (Small)
			{
				auto tmp = Small->Next;
				std::destroy_at(Small);
				try
				{
					HLVM_ENSURE(HLVM_LOW_GMALLOC_TLS.FreeAligned(Small, sizeof(FSmallHeapChain)) == EFreeRetType::Success,
						TXT("~FOSPageMallocator small failed {}"), R_C(void*, Small));
				}
				catch (...)
				{
				}
				Small = tmp;
			}
		}
		{
			auto Large = mLargeHeapChainHead;
			while (Large)
			{
				auto Next = Large->Next;
				std::destroy_at(Large);
				try
				{
					HLVM_ENSURE(HLVM_LOW_GMALLOC_TLS.Free(Large) == EFreeRetType::Success,
						TXT("~FOSPageMallocator large failed {}"), R_C(void*, Large));
				}
				catch (...)
				{
				}
				Large = Next;
			}
		}
	}

	bool Owned(void* ptr) const noexcept
	{
		{
			auto Large = mLargeHeapChainHead;
			while (Large)
			{
				if (Large->Heap->Owned(ptr))
				{
					return true;
				}
				Large = Large->Next;
			}
		}
		{
			auto Small = mSmallHeapChainHead;
			while (Small)
			{
				if (Small->InlineMallocator.Owned(ptr))
				{
					return true;
				}
				Small = Small->Next;
			}
		}
		return false;
	}

	/**
	 *  Allocate memory from small heap with very limit size (<4K), if fail, return nullptr
	 */
	void* MallocSmall(size_t size)
	{
		void* p = nullptr;
		auto  Small = mSmallHeapChainHead;
		while (!p && Small)
		{
			p = Small->InlineMallocator.Malloc(size);
			if (!p)
			{
				if (!Small->Next)
				{
					/**
					 *  Allocate new small heap, and ensure we can allocate enough memory right away
					 */
					Small->Next = std::construct_at(R_C(FSmallHeapChain*, HLVM_LOW_GMALLOC_TLS.MallocAligned(sizeof(FSmallHeapChain), sizeof(FSmallHeapChain))));
					p = Small->Next->InlineMallocator.Malloc(size);
					break;
				}
				Small = Small->Next;
			}
		}
		HLVM_CONSTEXPR_ASSERT(bValidate, p != nullptr);
		return p;
	}

	void FreeSmall(void* ptr)
	{
		auto Small = mSmallHeapChainHead;
		while (Small)
		{
			if (Small->InlineMallocator.Owned(ptr))
			{
				HLVM_ENSURE(Small->InlineMallocator.Free(ptr) == EFreeRetType::Success,
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
				p = Large->Heap = R_C(FLargeHeapData*, HLVM_LOW_GMALLOC_TLS.MallocAligned(sizeof(FLargeHeapData), sizeof(FLargeHeapData)));
				break;
			}
			else
			{
				if (!Large->Next)
				{
					Large->Next = std::construct_at(R_C(FLargeHeapChain*, HLVM_LOW_GMALLOC_TLS.Malloc(sizeof(FLargeHeapChain))));
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
				HLVM_ENSURE(HLVM_LOW_GMALLOC_TLS.FreeAligned(ptr, sizeof(FLargeHeapData)) == EFreeRetType::Success,
					TXT("FreeLargeHeap failed {}"), R_C(void*, ptr));
				Large->Heap = nullptr;
				bFound = true;
			}
			Large = Large->Next;
		}
		HLVM_CONSTEXPR_ASSERT(bValidate, bFound);
	}

	void* MallocAlign(size_t N)
	{
		void* p = nullptr;
		p = HLVM_LOW_GMALLOC_TLS.MallocAligned(N, sizeof(FLargeHeapData));
		HLVM_CONSTEXPR_ASSERT(bValidate, p != nullptr);
		return p;
	}

	EFreeRetType FreeAlign(void* p)
	{
		return HLVM_LOW_GMALLOC_TLS.FreeAligned(p, sizeof(FLargeHeapData));
	}

private:
	struct FSmallHeapChain
	{
		// 48 bytes offset so that FSmallHeapChain is equal to HLVM_VMA_OSPAGE_SMALL_HEAP_SIZE
		TStackMallocator<HLVM_VMA_OSPAGE_SMALL_HEAP_SIZE - 48, true, false, false, true> InlineMallocator;
		FSmallHeapChain*														  Next{ nullptr };
	};
	static_assert(sizeof(FSmallHeapChain) == HLVM_VMA_OSPAGE_SMALL_HEAP_SIZE, "SmallHeap size must be HLVM_VMA_OSPAGE_SMALL_HEAP_SIZE");
	/**
	 *  Small heap chain
	 */
	FSmallHeapChain* mSmallHeapChainHead{ nullptr };

	struct FLargeHeapData
	{
		TBYTE Data[HLVM_VMA_OSPAGE_LARGE_HEAP_SIZE];

		bool Owned(void* ptr) const
		{
			return ptr >= Data && ptr < Data + sizeof(Data);
		}
	};
	static_assert(sizeof(FLargeHeapData) == HLVM_VMA_OSPAGE_LARGE_HEAP_SIZE, "FLargeHeapData size must be HLVM_VMA_OSPAGE_LARGE_HEAP_SIZE");

	struct FLargeHeapChain
	{
		FLargeHeapData*	 Heap{ nullptr };
		FLargeHeapChain* Next{ nullptr };

		~FLargeHeapChain()
		{
			HLVM_ENSURE(HLVM_LOW_GMALLOC_TLS.FreeAligned(this->Heap, sizeof(FLargeHeapData)) == EFreeRetType::Success,
				TXT("~FOSPageMallocator Heap failed {}"), R_C(void*, this->Heap));
		}
	};
	/**
	 * Large heap chain
	 */
	FLargeHeapChain* mLargeHeapChainHead{ nullptr };
};
