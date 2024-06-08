/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "MallocatorDefinition.h"
#include "MiMallocator.h"
#include "StdMallocator.h"

#include "Core/Assert.h"
#include "Template/PointerTemplate.tpp"

#ifndef HLVM_STACK_MALLOCATOR_DEFAULT_SIZE
	#define HLVM_STACK_MALLOCATOR_DEFAULT_SIZE 64 * 1024
#endif

/**
 * General purpose Stack allocator
 */
template <int32_t N = HLVM_STACK_MALLOCATOR_DEFAULT_SIZE,
	bool		  bMonolithic = false,
	bool		  bDefragment = true,
	bool		  bAllowOverflowToHeap = true,
	bool		  bUseHeapForAlignedAlloc = true,
	bool		  bValidate = HLVM_MALLOC_VALIDATION>
class TStackMallocator final : public IMallocator
{
public:
	using SizeType = int32_t;

	NOCOPYMOVE(TStackMallocator)
	TStackMallocator() noexcept
	{
		Type = EMallocator::Stack;
		Reset();
	}

	/**
	 * Reset the allocator, useful when you want to reuse the allocator and ignore all previously allocated memory
	 * Used in Assert Stack Allocation in Assert.cpp
	 */
	HLVM_INLINE_FUNC void Reset() noexcept
	{
		// Init stack and free block head which occupy the whole stack
		mFreeBlockHead = std::construct_at(R_C(FBlock*, mStack));
		mFreeBlockHead->size = (Size_UpperBound); // Stack size minus head block and tail block
		HLVM_CONSTEXPR_ASSERT(bValidate, mFreeBlockHead->size > 0);

		// Init tail which is trivially free
		mTail = std::construct_at(R_C(FBlock*, mStack + N - FBlock_Size));
		mTail->prevFreeBlock = mFreeBlockHead;

		// Connect head to tail
		mFreeBlockHead->nextFreeBlock = mTail;

		// Init free size upper bound
		mFreeSizeUpperBound = -1;
	}

	HLVM_NODISCARD HLVM_INLINE_FUNC virtual bool Owned(void* ptr) noexcept final override
	{
		if (InStackBound(ptr))
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	HLVM_NODISCARD HLVM_INLINE_FUNC virtual void* Malloc(size_t size) noexcept(false) final override
	{
		return InternalMalloc(size);
	}
	HLVM_NODISCARD HLVM_INLINE_FUNC virtual void* Malloc2(size_t size) noexcept final override
	{
		return InternalMalloc(size);
	}
	HLVM_NODISCARD HLVM_INLINE_FUNC virtual void* MallocAligned(size_t size, size_t align) noexcept(false) final override
	{
		if constexpr (bUseHeapForAlignedAlloc)
		{
			return HLVM_LOW_GMALLOC_TLS.MallocAligned(size, align);
		}
		else
		{
			return InternalMalloc(size);
		}
	}
	HLVM_NODISCARD HLVM_INLINE_FUNC virtual void* MallocAligned2(size_t size, size_t align) noexcept final override
	{
		if constexpr (bUseHeapForAlignedAlloc)
		{
			return HLVM_LOW_GMALLOC_TLS.MallocAligned2(size, align);
		}
		else
		{
			return InternalMalloc(size);
		}
	}
	HLVM_NODISCARD HLVM_INLINE_FUNC virtual EFreeRetType Free(void* ptr) noexcept final override
	{
		return InternalFree(ptr);
	}
	HLVM_NODISCARD HLVM_INLINE_FUNC virtual EFreeRetType FreeSize(void* ptr, size_t) noexcept final override
	{
		return InternalFree(ptr);
	}
	HLVM_NODISCARD HLVM_INLINE_FUNC virtual EFreeRetType FreeAligned(void* ptr, size_t align) noexcept final override
	{
		if constexpr (bUseHeapForAlignedAlloc)
		{
			return HLVM_LOW_GMALLOC_TLS.FreeAligned(ptr, align);
		}
		else
		{
			return InternalFree(ptr);
		}
	}
	HLVM_NODISCARD HLVM_INLINE_FUNC virtual EFreeRetType FreeSizeAligned(void* ptr, size_t size, size_t align) noexcept final override
	{
		if constexpr (bUseHeapForAlignedAlloc)
		{
			return HLVM_LOW_GMALLOC_TLS.FreeSizeAligned(ptr, size, align);
		}
		else
		{
			return InternalFree(ptr);
		}
	}

private:
	struct FBlock
	{
		NOCOPYMOVE(FBlock)
		FBlock() = default;
		TOffsetPtr32<FBlock> prevFreeBlock{};
		TOffsetPtr32<FBlock> nextFreeBlock{};
		SizeType			 size{ 0 };

		HLVM_INLINE_FUNC bool GetFree() const
		{
			return size >= 0;
		}
	};
	static_assert(sizeof(FBlock) == 12, "FBlock size must be 12 bytes");

	HLVM_INLINE_VAR HLVM_STATIC_VAR constexpr SizeType FBlock_Size = S_C(SizeType, sizeof(FBlock));
	HLVM_INLINE_VAR HLVM_STATIC_VAR constexpr SizeType Size_UpperBound = S_C(SizeType, N - 2 * FBlock_Size);
	static_assert(Size_UpperBound > 0);
	HLVM_INLINE_VAR HLVM_STATIC_VAR constexpr SizeType Minimal_Block_Size = 24;

	// TODO : Maybe consider implement aligned address looking up for stack allocator
	void* InternalMalloc(size_t _size) noexcept(bValidate)
	{
		SizeType size = S_C(SizeType, _size);
		HLVM_CONSTEXPR_ASSERT(bValidate, size > 0);
		HLVM_CONSTEXPR_ASSERT(bValidate, mFreeBlockHead->prevFreeBlock == nullptr);
		/**
		 * Only allocate from stack if the size is smaller than the upper bound
		 */
		const bool bSizeValid = size <= Size_UpperBound;
		const bool bStackFreeSpace = (mFreeSizeUpperBound < 0 || size <= mFreeSizeUpperBound);
		const bool bValidForStack = bSizeValid && bStackFreeSpace;
		if (bValidForStack)
		{
			FBlock* FreeBlock = mFreeBlockHead;
			/**
			 * Find the first free block which is large enough
			 */
			while (FreeBlock->nextFreeBlock != nullptr)
			{
				HLVM_CONSTEXPR_ASSERT(bValidate, FreeBlock->GetFree());
				if (FreeBlock->size >= size)
				{
					// Reset free size upper bound since allocation success
					mFreeSizeUpperBound = -1;

					FBlock* NextFreeBlock = FreeBlock->nextFreeBlock;
					HLVM_CONSTEXPR_ASSERT(bValidate, NextFreeBlock && NextFreeBlock->prevFreeBlock == FreeBlock && (NextFreeBlock == mTail || NextFreeBlock->size > 0));
					FBlock* PrevFreeBlock = FreeBlock->prevFreeBlock;

					SizeType NewFreeBlockSize = (FreeBlock->size - FBlock_Size - size);
					if (NewFreeBlockSize <= Minimal_Block_Size)
						HLVM_UNLIKELY
						{
							// New free block is trivial
							// Mark current free block not free anymore
							FreeBlock->size = (-FreeBlock->size);
							NextFreeBlock->prevFreeBlock = PrevFreeBlock;
							if (PrevFreeBlock)
							{
								HLVM_CONSTEXPR_ASSERT(bValidate,
									PrevFreeBlock && PrevFreeBlock->nextFreeBlock == FreeBlock && PrevFreeBlock->size > 0);
								PrevFreeBlock->nextFreeBlock = NextFreeBlock;
							}
							else
							{
								HLVM_CONSTEXPR_ASSERT(bValidate, mFreeBlockHead == FreeBlock);
								mFreeBlockHead = NextFreeBlock;
							}
						}
					else
						HLVM_LIKELY
						{
							// Otherwise, connect next and prev to new free block
							FBlock* NewFreeBlock = R_C(FBlock*, R_C(TBYTE*, FreeBlock) + FBlock_Size + size);
							NewFreeBlock->size = NewFreeBlockSize;
							// Mark current free block not free anymore and update to malloced size
							FreeBlock->size = (-size);

							NewFreeBlock->nextFreeBlock = NextFreeBlock;
							NewFreeBlock->prevFreeBlock = PrevFreeBlock;
							NextFreeBlock->prevFreeBlock = NewFreeBlock;
							if (PrevFreeBlock)
							{
								HLVM_CONSTEXPR_ASSERT(bValidate,
									PrevFreeBlock && PrevFreeBlock->nextFreeBlock == FreeBlock && PrevFreeBlock->size > 0);
								PrevFreeBlock->nextFreeBlock = NewFreeBlock;
							}
							else
							{
								HLVM_CONSTEXPR_ASSERT(bValidate, mFreeBlockHead == FreeBlock);
								// Otherwise, assign head to new free block
								mFreeBlockHead = NewFreeBlock;
							}
						}
					HLVM_CONSTEXPR_ASSERT(bValidate, mFreeBlockHead->prevFreeBlock == nullptr);
					HLVM_CONSTEXPR_ASSERT(bValidate, mFreeBlockHead != FreeBlock);

					// Return actual pointer address
					TBYTE* ptr = R_C(TBYTE*, FreeBlock) + FBlock_Size;
					return ptr;
				}
				else
				{
					// Try out next free block
					if (mFreeSizeUpperBound < FreeBlock->size)
					{
						mFreeSizeUpperBound = FreeBlock->size;
					}
					FreeBlock = FreeBlock->nextFreeBlock;
				}
			}
			HLVM_CONSTEXPR_ASSERT(bValidate, FreeBlock == mTail);
			HLVM_CONSTEXPR_ASSERT(bValidate, mFreeSizeUpperBound >= 0);
		}
		// Running out of free blocks in stack, try heap
		if constexpr (bAllowOverflowToHeap)
		{
			return HLVM_LOW_GMALLOC_TLS.Malloc(_size);
		}
		else
		{
			return nullptr;
		}
	}

	EFreeRetType InternalFree(void* ptr) noexcept(bValidate)
	{
		if (InStackBound(ptr))
		{
			if constexpr (bMonolithic)
			{
				return EFreeRetType::Success;
			}
			else
			{
				// Reset new block to free
				FBlock* FreeBlock = R_C(FBlock*, R_C(TBYTE*, ptr) - FBlock_Size);
				HLVM_CONSTEXPR_ASSERT(bValidate, FreeBlock->size < 0);
				HLVM_CONSTEXPR_ASSERT(bValidate, !FreeBlock->GetFree());
				HLVM_CONSTEXPR_ASSERT(bValidate, mFreeBlockHead->GetFree());
				HLVM_CONSTEXPR_ASSERT(bValidate, mFreeBlockHead->prevFreeBlock == nullptr);

				// Set free block to free again
				FreeBlock->size = (-FreeBlock->size);
				HLVM_CONSTEXPR_ASSERT(bValidate, FreeBlock->size > 0 && FreeBlock->size <= Size_UpperBound);

				// Exchange new free block with free head
				FreeBlock->prevFreeBlock = nullptr;
				FreeBlock->nextFreeBlock = mFreeBlockHead;
				mFreeBlockHead->prevFreeBlock = FreeBlock;
				mFreeBlockHead = FreeBlock;

				// Defragmentation next physical block if it is free
				if constexpr (bDefragment)
				{
					FBlock* CurrBlock = mFreeBlockHead;
					FBlock* NextBlock = R_C(FBlock*, R_C(TBYTE*, CurrBlock) + FBlock_Size + CurrBlock->size);
					// While not reach the tail
					while (NextBlock != mTail)
					{
						HLVM_CONSTEXPR_ASSERT(bValidate, CurrBlock->size != 0 && NextBlock->size != 0);
						if (NextBlock->size < 0)
						{
							break;
						}
						else
						{
							// Sanity checks
							FBlock* NextBlockPrevFreeBlock = NextBlock->prevFreeBlock;
							HLVM_CONSTEXPR_ASSERT(bValidate, NextBlockPrevFreeBlock);

							HLVM_CONSTEXPR_ASSERT(bValidate, CurrBlock->size > 0 && NextBlock->size > 0);
							HLVM_CONSTEXPR_ASSERT(bValidate, (NextBlockPrevFreeBlock && NextBlockPrevFreeBlock != NextBlock && NextBlockPrevFreeBlock->nextFreeBlock == NextBlock));
							FBlock* NextBlockNextFreeBlock = NextBlock->nextFreeBlock;
							HLVM_CONSTEXPR_ASSERT(bValidate, NextBlockNextFreeBlock && NextBlockNextFreeBlock != NextBlock && NextBlockNextFreeBlock->prevFreeBlock == NextBlock && (NextBlockNextFreeBlock == mTail || NextBlockNextFreeBlock->size > 0));

							// Eliminate Next block by connect prev and next
							HLVM_CONSTEXPR_ASSERT(bValidate, NextBlockPrevFreeBlock != NextBlockNextFreeBlock);
							NextBlockPrevFreeBlock->nextFreeBlock = NextBlockNextFreeBlock;
							NextBlockNextFreeBlock->prevFreeBlock = NextBlockPrevFreeBlock;

							// Defragment CurrBlock and move on to next block once more
							CurrBlock->size += NextBlock->size + FBlock_Size;
							HLVM_CONSTEXPR_ASSERT(bValidate, CurrBlock->size > 0 && CurrBlock->size <= Size_UpperBound);

							// Update upper bound if necessary
							if (!(mFreeSizeUpperBound < 0) && mFreeSizeUpperBound < CurrBlock->size)
							{
								mFreeSizeUpperBound = CurrBlock->size;
							}

							// Iterate to next block
							NextBlock = R_C(FBlock*, R_C(TBYTE*, NextBlock) + FBlock_Size + NextBlock->size);
							HLVM_CONSTEXPR_ASSERT(bValidate, NextBlock == R_C(FBlock*, R_C(TBYTE*, CurrBlock) + FBlock_Size + CurrBlock->size));
						}
					}
				}

				{
					// Swap next block if it is bigger than current free head,
					// so to keep free head a larger block size for easier allocation next time
#if 1 // Fast path: only swap at most once
					if (FBlock* NextBlock = mFreeBlockHead->nextFreeBlock;
						NextBlock->size > mFreeBlockHead->size)
					{
						// Sanity checks
						HLVM_CONSTEXPR_ASSERT(bValidate, NextBlock->prevFreeBlock == mFreeBlockHead);
						FBlock* NextBlockNext = NextBlock->nextFreeBlock;
						HLVM_CONSTEXPR_ASSERT(bValidate, NextBlockNext && NextBlockNext != NextBlock && NextBlockNext->prevFreeBlock == NextBlock);

						// Skip next and connect free head with next block's next
						HLVM_CONSTEXPR_ASSERT(bValidate, NextBlockNext != mFreeBlockHead);
						mFreeBlockHead->nextFreeBlock = NextBlockNext;
						NextBlockNext->prevFreeBlock = mFreeBlockHead;

						// Swap next block with free head
						NextBlock->nextFreeBlock = mFreeBlockHead;
						mFreeBlockHead->prevFreeBlock = NextBlock;

						mFreeBlockHead = NextBlock;
						mFreeBlockHead->prevFreeBlock = nullptr;
					}
#else // Slow path: swap free block until it is no longer smaller than its next block
					FBlock* CurrBlock = mFreeBlockHead;
					FBlock* NextBlock = CurrBlock->nextFreeBlock;
					FBlock* InsertBlock = NextBlock;
					HLVM_CONSTEXPR_ASSERT(bValidate, InsertBlock);
					HLVM_CONSTEXPR_ASSERT(bValidate, InsertBlock->size >= 0);
					/**
					 * Find a insert block that is smaller than or equal current block
					 * and insert current block before it
					 * TODO : Optimize this insert search phase as it cost most time
					 */
					while (InsertBlock->size > CurrBlock->size)
					{
						++Iter;
						InsertBlock = InsertBlock->nextFreeBlock;
					}

					// We need to insert curr block as the prev block of insert block
					if (InsertBlock != NextBlock)
					{
						// Make nextblock now the free head block
						mFreeBlockHead = NextBlock;
						mFreeBlockHead->prevFreeBlock = nullptr;

						// Insert curr block and connect prev and next
						FBlock* InsertBlockPrev = InsertBlock->prevFreeBlock;
						HLVM_CONSTEXPR_ASSERT(bValidate, InsertBlockPrev && InsertBlockPrev != InsertBlock && InsertBlockPrev->nextFreeBlock == InsertBlock);
						InsertBlockPrev->nextFreeBlock = CurrBlock;
						CurrBlock->prevFreeBlock = InsertBlockPrev;
						CurrBlock->nextFreeBlock = InsertBlock;
						InsertBlock->prevFreeBlock = CurrBlock;
						HLVM_CONSTEXPR_ASSERT(bValidate, InsertBlockPrev->size > CurrBlock->size && CurrBlock->size >= InsertBlock->size);
					}
#endif
				}

				/**
				 * Check free block validity
				 */
				if constexpr (bValidate)
				{
					assert(mFreeBlockHead->prevFreeBlock == nullptr
						&& mFreeBlockHead->nextFreeBlock
						&& mFreeBlockHead->nextFreeBlock != mFreeBlockHead
						&& FreeBlock->size > 0);
					FreeBlock = mFreeBlockHead->nextFreeBlock;
					while (FreeBlock->nextFreeBlock)
					{
						assert(FreeBlock->prevFreeBlock != nullptr
							&& FreeBlock->prevFreeBlock != FreeBlock->nextFreeBlock
							&& FreeBlock->prevFreeBlock != FreeBlock
							&& FreeBlock->nextFreeBlock != FreeBlock
							&& FreeBlock != mFreeBlockHead
							&& FreeBlock->size > 0);
						FreeBlock = FreeBlock->nextFreeBlock;
					}
					assert(FreeBlock == mTail);
				}
				return EFreeRetType::Success;
			}
		}
		else
		{
			if constexpr (bAllowOverflowToHeap)
			{
				return HLVM_LOW_GMALLOC_TLS.Free(ptr);
			}
		}
		return EFreeRetType::NotOwned;
	}

	bool InStackBound(void* ptr) const
	{
		return ptr >= mStack + FBlock_Size && ptr < mTail;
	}

	TBYTE	 mStack[N];
	FBlock*	 mFreeBlockHead;
	FBlock*	 mTail;
	SizeType mFreeSizeUpperBound;
};

/**
 * We create a monolithic specialization of stack allocator
 */
template <int32_t N = HLVM_STACK_MALLOCATOR_DEFAULT_SIZE>
using TStackMonolithicAllocator = TStackMallocator<N, true>;
