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
		return S_C(size_t, mFreeBlockHead->size);
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
	void SortFreeBlockList();

	PACK(struct FBlock {
		NOCOPYMOVE(FBlock)
		FBlock() = default;
		TOffsetPtr32<FBlock> prevFreeBlock{};
		TOffsetPtr32<FBlock> nextFreeBlock{};
		SizeType			 size{ 0 };
		bool				 bCached{ false };							// Being used as a cached block (special block cannot defragment!)
		bool				 bMid{ false };								// Being used as a mid block (special block cannot defragment!)
		TUINT16				 __{ std::numeric_limits<TUINT16>::max() }; // Masked bytes to not mis-interpreting with FSmallBinnedBlockHead block heads

		HLVM_INLINE_FUNC bool GetFree() const
		{
			return size >= 0;
		}

		HLVM_INLINE_FUNC bool CanDefragment() const
		{
			return !(bCached || bMid);
		}
	});
	static_assert(sizeof(FBlock) == 16, "FBlock size must be 16 bytes");
	HLVM_INLINE_VAR HLVM_STATIC_VAR constexpr SizeType FBlock_Size = S_C(SizeType, sizeof(FBlock));
	HLVM_INLINE_VAR HLVM_STATIC_VAR constexpr SizeType Minimal_Block_Size = HLVM_VMA_SMALL_ALLOC_THRESHOLD;

	/**
	 * Cached free blocks, used to reduce the number of internal malloc/free calls
	 * (because we have to sort free list to find the best fit for internal malloc/free which cost time)
	 */
	struct CachedFreeBlocks
	{
		constexpr static TUINT8 MaxCachedFreeBlocks = 64;
		TUINT8					Canary1{ 128 };
		FBlock*					CachedFreeBlocks[MaxCachedFreeBlocks];
		TUINT8					Canary2{ 128 };
		TUINT8					NumCachedFreeBlocks{ 0 };
		bool					bUseCachedFreeBlocks{ true };

		bool ShouldTryCachFreeBlock()
		{
			return bUseCachedFreeBlocks && NumCachedFreeBlocks < MaxCachedFreeBlocks;
		}

		FBlock** Head()
		{
			return CachedFreeBlocks;
		}

		FBlock** Tail()
		{
			return CachedFreeBlocks + NumCachedFreeBlocks - 1;
		}

		TUINT8 Num()
		{
			return NumCachedFreeBlocks;
		}

		FBlock** LowerBound(SizeType size)
		{
			// Lower bound algorithm to find the best fit
			// https://en.cppreference.com/w/cpp/algorithm/lower_bound
			FBlock** first = CachedFreeBlocks;
			{
				TUINT8 count = NumCachedFreeBlocks;
				TUINT8 step;
				while (count > 0)
				{
					auto it = first;
					step = count / 2;
					it += step;
					HLVM_CONSTEXPR_ASSERT(bValidate, (*it)->size > 0);
					if (size >= (*it)->size)
					{
						first = it + 1;
						count -= step + 1;
					}
					else
					{
						count = step;
					}
				}
				auto index = S_C(TUINT8, first - CachedFreeBlocks);
				HLVM_CONSTEXPR_ASSERT(bValidate, index <= NumCachedFreeBlocks);
				if (index > 0)
				{
					HLVM_CONSTEXPR_ASSERT(bValidate, size >= (*(first - 1))->size);
				}
				if (index < NumCachedFreeBlocks)
				{
					HLVM_CONSTEXPR_ASSERT(bValidate, size <= (*first)->size);
				}
			}
			return first;
		}

		void Insert(FBlock** first, FBlock* block)
		{
			HLVM_CONSTEXPR_ASSERT(bValidate, block->size > 0);
			// Insert the block;
			auto index = S_C(TUINT8, first - CachedFreeBlocks);
			HLVM_CONSTEXPR_ASSERT(bValidate, index >= 0 && index < MaxCachedFreeBlocks);
			HLVM_CONSTEXPR_ASSERT(bValidate, NumCachedFreeBlocks < MaxCachedFreeBlocks);
			HLVM_CONSTEXPR_ASSERT(bValidate, index <= NumCachedFreeBlocks);
			if (auto distance = S_C(TUINT8, NumCachedFreeBlocks - index);
				distance > 0)
			{
				std::memmove(first + 1, first, S_C(size_t, distance) * sizeof(FBlock*));
			}
			block->bCached = true;
			CachedFreeBlocks[index] = block;
			if (index > 0)
			{
				HLVM_CONSTEXPR_ASSERT(bValidate, CachedFreeBlocks[index - 1]->size <= CachedFreeBlocks[index]->size);
			}
			if (index < NumCachedFreeBlocks)
			{
				HLVM_CONSTEXPR_ASSERT(bValidate, CachedFreeBlocks[index]->size <= CachedFreeBlocks[index + 1]->size);
			}
			++NumCachedFreeBlocks;
			HLVM_CONSTEXPR_ASSERT(bValidate, NumCachedFreeBlocks <= MaxCachedFreeBlocks);
			Validate();
		}

		void Erase(FBlock** first)
		{
			// Remove the block;
			auto block = *first;
			block->bCached = false;

			auto index = S_C(TUINT8, first - CachedFreeBlocks);
			HLVM_CONSTEXPR_ASSERT(bValidate, index >= 0 && index < MaxCachedFreeBlocks);
			HLVM_CONSTEXPR_ASSERT(bValidate, NumCachedFreeBlocks > 0);
			HLVM_CONSTEXPR_ASSERT(bValidate, index < NumCachedFreeBlocks);
			if (auto distance = S_C(TUINT8, NumCachedFreeBlocks - index - 1);
				distance > 0)
			{
				std::memmove(first, first + 1, S_C(size_t, distance) * sizeof(FBlock*));
			}
			if (index > 0)
			{
				HLVM_CONSTEXPR_ASSERT(bValidate, CachedFreeBlocks[index - 1]->size <= CachedFreeBlocks[index]->size);
			}
			HLVM_CONSTEXPR_ASSERT(bValidate, NumCachedFreeBlocks > 0);
			--NumCachedFreeBlocks;
			if (index < NumCachedFreeBlocks)
			{
				HLVM_CONSTEXPR_ASSERT(bValidate, CachedFreeBlocks[index]->size <= CachedFreeBlocks[index + 1]->size);
			}
			Validate();
		}

	private:
		void Validate()
		{
			HLVM_CONSTEXPR_ASSERT(bValidate, Canary1 == 128);
			HLVM_CONSTEXPR_ASSERT(bValidate, Canary2 == 128);
			HLVM_CONSTEXPR_ASSERT(bValidate, NumCachedFreeBlocks <= MaxCachedFreeBlocks);
		}
	};

private:
	FVMArena* VMArena{ nullptr };
	size_t	  N{ 0 };
	BIT_FLAG(bManaged){ true };

	TBYTE*	mHeap{ nullptr };
	TBYTE*	mLowerBound{ nullptr };
	FBlock* mTail{ nullptr };
	FBlock* mFreeBlockHead{ nullptr };
	FBlock* mMid{ nullptr };

	CachedFreeBlocks mCachedFreeBlocks;
};
