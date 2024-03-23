/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

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

			// Init tail which is trivially free
			mTail = R_C(FBlock*, mHeap + N - FBlock_Size);
			*mTail = FBlock();
			mTail->prevFreeBlock = mFreeBlockHead;

			// Connect head to tail
			mFreeBlockHead->nextFreeBlock = mTail;

			// Cache the lower bound memory address for stack pointers
			mLowerBound = mHeap + FBlock_Size;

			// Startoff defragmentation from the start of stack
			mDefragmentHead = R_C(FBlock*, mHeap);
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
		return Managed() ? p >= mLowerBound && p < mTail : false;
	}

	bool Managed() const
	{
		return bManaged;
	}

	size_t GetFreeBlockSizeUpperBound() const
	{
		return S_C(size_t, mFreeSizeUpperBound);
	}

	size_t GetSize() const
	{
		return N;
	}

	HLVM_STATIC_FUNC size_t CalculateCapacity(size_t size)
	{
		return size + 2 * FBlock_Size;
	}

	size_t GetManagedSize() const
	{
		return Managed() ? GetSize() - 2 * sizeof(FBlock) : 0;
	}

	void* Malloc(size_t size);
	void  Free(void* p);

private:
	FMiMallocator* MiMallocator{ nullptr };
	TBYTE*		   mHeap{ nullptr };
	size_t		   N{ 0 };
	BIT_FLAG(bManaged){ true };

	PACK(struct FBlock {
		FBlock*	 prevFreeBlock{ nullptr };
		FBlock*	 nextFreeBlock{ nullptr };
		SizeType size{ 0 };
		uint16_t heapID{ 0 };
		uint16_t _reserved{ 0xFFFF }; // Reserved 2 bytes to not collide with FSmallBinnedBlockHead when interpreting block heads

		HLVM_INLINE_FUNC bool
		GetFree() const
		{
			return size >= 0;
		}
	});
	HLVM_STATIC_VAR constexpr SizeType FBlock_Size = S_C(SizeType, sizeof(FBlock));

	FBlock*	 mFreeBlockHead{ nullptr };
	FBlock*	 mDefragmentHead{ nullptr };
	FBlock*	 mTail{ nullptr };
	void*	 mLowerBound{ nullptr };
	SizeType mFreeSizeUpperBound{ 0 };
};
