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
	using SizeType = TINT32;

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

	HLVM_STATIC_FUNC size_t EstimateHeapCapacityBySize(size_t size)
	{
		return size + 2 * sizeof(FBlock) + sizeof(FHeapHeadBlock);
	}

	size_t GetManagedSize() const
	{
		return Managed() ? GetHeapSize() - 2 * sizeof(FBlock) - sizeof(FHeapHeadBlock) : 0;
	}

	void* Malloc(size_t size);
	void  Free(void* p);

public:
	struct FHeapHeadBlock
	{
		FVMHeap* OwnerHeap{ nullptr };
	};
	static_assert(sizeof(FHeapHeadBlock) == 8, "FHeapHeadBlock size must be 16 bytes");

private:
	PACK(struct FBlock {
		NOCOPYMOVE(FBlock)
		FBlock() = default;
		TOffsetPtr32<FBlock> prevFreeBlock{};
		TOffsetPtr32<FBlock> nextFreeBlock{};
		SizeType			 size{ 0 };
		TUINT32				 _{ ~(0u) }; // Masked bytes to not mis-interpreting with FSmallBinnedBlockHead block heads

		HLVM_INLINE_FUNC bool GetFree() const
		{
			return size >= 0;
		}
	});
	static_assert(sizeof(FBlock) == 16, "FBlock size must be 16 bytes");
	HLVM_INLINE_VAR HLVM_STATIC_VAR constexpr SizeType FBlock_Size = S_C(SizeType, sizeof(FBlock));
	HLVM_INLINE_VAR HLVM_STATIC_VAR constexpr SizeType Minimal_Block_Size = HLVM_VMA_SMALL_ALLOC_THRESHOLD;

private:
	FVMArena* VMArena{ nullptr };
	size_t	  N{ 0 };
	BIT_FLAG(bManaged){ true };

	TBYTE*	 mHeap{ nullptr }; // TODO Allocate pointer to HeapAllocator in the beginning of Heap so that we can get the size of Heap
	FBlock*	 mFreeBlockHead{ nullptr };
	FBlock*	 mTail{ nullptr };
	TBYTE*	 mLowerBound{ nullptr };
	SizeType mFreeSizeUpperBound{ 0 };
};
