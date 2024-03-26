/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "Platform/GenericPlatformAtomicPointer.h"
#include "Core/Mallocator/MiMallocator.h"

class FHeapMallocator
{
	HLVM_INLINE_VAR HLVM_STATIC_VAR constexpr bool bValidate = HLVM_MALLOC_VALIDATION;

public:
	using SizeType = int32_t;

	NOCOPYMOVE(FHeapMallocator)
	FHeapMallocator() = default;
	~FHeapMallocator()
	{
		Destroy();
	}

	void Init(FMiMallocator* _MiMallocator, size_t _size)
	{
		MiMallocator = _MiMallocator;
		N = _size;
		HLVM_CONSTEXPR_ASSERT(bValidate, (N - 2 * FBlock_Size > 0));
		mHeap = R_C(TBYTE*, MiMallocator->Malloc(N));

		bManaged = N < std::numeric_limits<SizeType>::max();
		if (bManaged)
		{
			// Init stack and free block head which occupy the whole stack
			mFreeBlockHead = R_C(FBlock*, mHeap);
			*mFreeBlockHead = FBlock();
			mFreeBlockHead->size = (S_C(SizeType, N) - 2 * FBlock_Size); // Stack size minus head block and tail block
			HLVM_CONSTEXPR_ASSERT(bValidate, mFreeBlockHead->size > 0);
			mFreeSizeUpperBound = mFreeBlockHead->size;

			// Init tail which is trivially free
			mTail = R_C(FBlock*, mHeap + N - FBlock_Size);
			*mTail = FBlock();
			mTail->prevFreeBlock = mFreeBlockHead;

			// Connect head to tail
			mFreeBlockHead->nextFreeBlock = mTail;

			// Cache the lower bound memory address for stack pointers
			mLowerBound = mHeap + FBlock_Size;
		}
	}

	/**
	 * Sometimes we need destroy w/o deconstruction
	 */
	void Destroy()
	{
		if (mHeap)
		{
			MiMallocator->Free(mHeap);
			mHeap = nullptr;
			bManaged = false;
		}
	}

	bool Owned(void* p) const
	{
		return Managed() ? p >= mLowerBound && p < mTail : p == mHeap;
	}

	bool Managed() const
	{
		return bManaged;
	}

	size_t GetFreeBlockSizeUpperBound() const
	{
		return S_C(size_t, mFreeSizeUpperBound);
	}

	size_t GetHeapSize() const
	{
		return N;
	}

	HLVM_STATIC_FUNC size_t CalculateCapacity(size_t size)
	{
		return size + 2 * FBlock_Size;
	}

	size_t GetManagedRegsionSize() const
	{
		return Managed() ? GetHeapSize() - 2 * sizeof(FBlock) : 0;
	}

	void* Malloc(size_t size);
	void  Free(void* p);

private:
	FMiMallocator* MiMallocator{ nullptr };
	TBYTE*		   mHeap{ nullptr };
	size_t		   N{ 0 };
	BIT_FLAG(bManaged){ true };

	struct FBlock
	{
		TOffsetPtr32<FBlock> prevFreeBlock{};
		TOffsetPtr32<FBlock> nextFreeBlock{};
		SizeType			 size{ 0 };
		uint16_t			 offset{ 0 };
		uint16_t			 _{ 0xFFFF }; // Reserved 2 bytes to not mis-interpreting with FSmallBinnedBlockHead block heads

		HLVM_INLINE_FUNC bool GetFree() const
		{
			return size >= 0;
		}
	};
	static_assert(sizeof(FBlock) == 16, "FBlock size must be 16 bytes");

	HLVM_INLINE_VAR HLVM_STATIC_VAR constexpr SizeType FBlock_Size = S_C(SizeType, sizeof(FBlock));
	HLVM_INLINE_VAR HLVM_STATIC_VAR constexpr SizeType Minimual_Block_Size = 24;

	FBlock*	 mFreeBlockHead{ nullptr };
	FBlock*	 mTail{ nullptr };
	void*	 mLowerBound{ nullptr };
	SizeType mFreeSizeUpperBound{ 0 };
};
