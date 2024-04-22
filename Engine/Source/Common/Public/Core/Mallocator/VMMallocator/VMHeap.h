/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "VMMallocatorDefinition.h"

class FVMArena;
class FVMHeap
{
	HLVM_INLINE_VAR HLVM_STATIC_VAR constexpr bool bValidate = HLVM_MALLOC_VALIDATION;

public:
	using SizeType = int32_t;

	NOCOPYMOVE(FVMHeap)
	FVMHeap() = default;
	~FVMHeap()
	{
		Destroy();
	}

	void Init(FVMArena* _VMArena, size_t _size, bool bForceUnManaged = false);

	/**
	 * Sometimes we need destroy w/o deconstruction
	 */
	void Destroy();

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

	size_t GetManagedSize() const
	{
		return Managed() ? GetHeapSize() - 2 * sizeof(FBlock) : 0;
	}

	void* Malloc(size_t size);
	void  Free(void* p);

private:
	FVMArena* VMArena{ nullptr };
	size_t	  N{ 0 };
	BIT_FLAG(bManaged){ true };
	TBYTE* mHeap{ nullptr }; // TODO Allocate pointer to HeapAllocator in the beginning of Heap so that we can get the size of Heap

	PACK(struct FBlock {
		TOffsetPtr32<FBlock> prevFreeBlock{};
		TOffsetPtr32<FBlock> nextFreeBlock{};
		SizeType			 size{ 0 };
		uint16_t			 _{ 0xFFFF }; // Reserved 2 bytes to not mis-interpreting with FSmallBinnedBlockHead block heads

		HLVM_INLINE_FUNC bool GetFree() const
		{
			return size >= 0;
		}
	});
	static_assert(sizeof(FBlock) == 14, "FBlock size must be 14 bytes");

	HLVM_INLINE_VAR HLVM_STATIC_VAR constexpr SizeType FBlock_Size = S_C(SizeType, sizeof(FBlock));
	HLVM_INLINE_VAR HLVM_STATIC_VAR constexpr SizeType Minimal_Block_Size = HLVM_VMA_SMALL_ALLOC_THRESHOLD;

	FBlock*	 mFreeBlockHead{ nullptr };
	FBlock*	 mTail{ nullptr };
	void*	 mLowerBound{ nullptr };
	SizeType mFreeSizeUpperBound{ 0 };
};
