/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "MiMallocator.h"
#include "Core/Assert.h"

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
	bool		  bValidate = !HLVM_BUILD_RELEASE>
class TStackMallocator final : public IMallocator
{
public:
	using SizeType = int32_t;

	NOCOPYMOVE(TStackMallocator)
	TStackMallocator()
	{
		Type = EMallocator::Stack;

		// Init stack and free block head which occupy the whole stack
		mFreeBlockHead = R_C(FBlock*, mStack);
		*mFreeBlockHead = FBlock();
		mFreeBlockHead->size = (N - 2 * FBlock_Size); // Stack size minus head block and tail block
		HLVM_CONSTEXPR_ASSERT(bValidate, mFreeBlockHead->size > 0);

		// Init tail which is trivially free
		mTail = R_C(FBlock*, mStack + N - FBlock_Size);
		*mTail = FBlock();
		mTail->prevFreeBlock = mFreeBlockHead;

		// Connect head to tail
		mFreeBlockHead->nextFreeBlock = mTail;

		// Cache the lower bound memory address for stack pointers
		mLowerBound = mStack + FBlock_Size;

		// Startoff defragmentation from the start of stack
		mDefragmentHead = R_C(FBlock*, mStack);
	}
	~TStackMallocator() final override
	{
	}
	HLVM_INLINE_FUNC virtual bool Owened(void* ptr) noexcept final override
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
	HLVM_INLINE_FUNC virtual void* Malloc(size_t size) noexcept(false) final override
	{
		return InternalMalloc(size);
	}
	HLVM_INLINE_FUNC virtual void* Malloc2(size_t size) noexcept final override
	{
		return InternalMalloc(size);
	}
	HLVM_INLINE_FUNC virtual void* MallocAligned(size_t size, size_t) noexcept(false) final override
	{
		return InternalMalloc(size);
	}
	HLVM_INLINE_FUNC virtual void* MallocAligned2(size_t size, size_t) noexcept final override
	{
		return InternalMalloc(size);
	}
	HLVM_INLINE_FUNC virtual void Free(void* ptr) noexcept final override
	{
		InternalFree(ptr);
	}
	HLVM_INLINE_FUNC virtual void FreeSize(void* ptr, size_t) noexcept final override
	{
		InternalFree(ptr);
	}
	HLVM_INLINE_FUNC virtual void FreeAligned(void* ptr, size_t) noexcept final override
	{
		InternalFree(ptr);
	}
	HLVM_INLINE_FUNC virtual void FreeSizeAligned(void* ptr, size_t, size_t) noexcept final override
	{
		InternalFree(ptr);
	}

private:
	PACK(struct FBlock {
		FBlock*				  prevFreeBlock{ nullptr };
		FBlock*				  nextFreeBlock{ nullptr };
		SizeType			  size{ 0 };
		HLVM_INLINE_FUNC bool GetFree() const
		{
			return size >= 0;
		}
	});
	HLVM_STATIC_VAR constexpr SizeType FBlock_Size = S_C(SizeType, sizeof(FBlock));
	static_assert(N - 2 * FBlock_Size > 0);

	void* InternalMalloc(size_t _size)
	{
		SizeType size = S_C(SizeType, _size);
		HLVM_CONSTEXPR_ASSERT(bValidate, size > 0 && size <= N - 2 * FBlock_Size);
		HLVM_CONSTEXPR_ASSERT(bValidate, mFreeBlockHead->prevFreeBlock == nullptr);
		FBlock* FreeBlock = mFreeBlockHead;
		while (FreeBlock->nextFreeBlock != nullptr)
		{
			HLVM_CONSTEXPR_ASSERT(bValidate, FreeBlock->GetFree());
			if (FreeBlock->size > size + FBlock_Size)
			{
				FBlock* NextFreeBlock = FreeBlock->nextFreeBlock;
				HLVM_CONSTEXPR_ASSERT(bValidate, NextFreeBlock && NextFreeBlock->prevFreeBlock == FreeBlock && (NextFreeBlock == mTail || NextFreeBlock->size > 0));
				FBlock* PrevFreeBlock = FreeBlock->prevFreeBlock;

				FBlock* NewFreeBlock = R_C(FBlock*, R_C(TBYTE*, FreeBlock) + FBlock_Size + size);
				NewFreeBlock->size = (FreeBlock->size - FBlock_Size - size);
				HLVM_CONSTEXPR_ASSERT(bValidate, NewFreeBlock->size > 0);
				{
					NewFreeBlock->nextFreeBlock = NextFreeBlock;
					NewFreeBlock->prevFreeBlock = PrevFreeBlock;
					// Otherwise, connect next and prev to new free block
					NextFreeBlock->prevFreeBlock = NewFreeBlock;
					if (PrevFreeBlock)
					{
						HLVM_CONSTEXPR_ASSERT(bValidate, PrevFreeBlock && PrevFreeBlock->nextFreeBlock == FreeBlock && PrevFreeBlock->size > 0);
						PrevFreeBlock->nextFreeBlock = NewFreeBlock;
					}
					else
					{
						// Otherwise, assign head to new free block
						mFreeBlockHead = NewFreeBlock;
					}
				}
				HLVM_CONSTEXPR_ASSERT(bValidate, mFreeBlockHead->prevFreeBlock == nullptr);
				HLVM_CONSTEXPR_ASSERT(bValidate, mFreeBlockHead != FreeBlock);

				// Mark current free block not free anymore
				FreeBlock->size = (-size);
				HLVM_CONSTEXPR_ASSERT(bValidate, FreeBlock->size < 0);

				// Return actual pointer address
				TBYTE* ptr = R_C(TBYTE*, FreeBlock) + FBlock_Size;
				return ptr;
			}
			else
			{
				// Try out next free block
				FreeBlock = FreeBlock->nextFreeBlock;
			}
		}
		// Running out of free blocks in stack, try heap
		if constexpr (bAllowOverflowToHeap)
		{
			return GMiMallocatorTLS.Malloc(_size);
		}
		else
		{
			return nullptr;
		}
	}

	void InternalFree(void* _ptr)
	{
		auto ptr = R_C(TBYTE*, _ptr);
		if (InStackBound(ptr))
		{
			if constexpr (bMonolithic)
			{
				return;
			}
			else
			{
				// Reset new block to free
				FBlock* FreeBlock = R_C(FBlock*, ptr - FBlock_Size);
				HLVM_CONSTEXPR_ASSERT(bValidate, FreeBlock->size < 0);
				HLVM_CONSTEXPR_ASSERT(bValidate, !FreeBlock->GetFree());
				HLVM_CONSTEXPR_ASSERT(bValidate, mFreeBlockHead->GetFree());
				HLVM_CONSTEXPR_ASSERT(bValidate, mFreeBlockHead->prevFreeBlock == nullptr);

				// Set free block to free again
				FreeBlock->size = (-FreeBlock->size);
				HLVM_CONSTEXPR_ASSERT(bValidate, FreeBlock->size > 0 && FreeBlock->size <= N - 2 * FBlock_Size);

				// Exchange new free block with free head
				FreeBlock->prevFreeBlock = nullptr;
				FreeBlock->nextFreeBlock = mFreeBlockHead;
				mFreeBlockHead->prevFreeBlock = FreeBlock;
				mFreeBlockHead = FreeBlock;

				// Defragmentation next physical block if it is free
				if constexpr (bDefragment)
				{
					FBlock* PrevBlock = R_C(FBlock*, mDefragmentHead);
					FBlock* NextBlock = R_C(FBlock*, R_C(TBYTE*, PrevBlock) + FBlock_Size + std::abs(PrevBlock->size));
					// While not reach the tail
					while (NextBlock != mTail)
					{
						HLVM_CONSTEXPR_ASSERT(bValidate, PrevBlock->size != 0 && NextBlock->size != 0);
						if (PrevBlock->size < 0 || NextBlock->size < 0)
							HLVM_LIKELY
							{
								// If not both blocks are free, continue
								PrevBlock = NextBlock;
								NextBlock = R_C(FBlock*, R_C(TBYTE*, PrevBlock) + FBlock_Size + std::abs(PrevBlock->size));
							}
						else
							HLVM_UNLIKELY
							{
								// Sanity checks
								FBlock* NextBlockPrevFreeBlock = NextBlock->prevFreeBlock;
								if (!NextBlockPrevFreeBlock)
									HLVM_UNLIKELY
									{
										// If next block is free head, No defragment as it make things complicated
										HLVM_CONSTEXPR_ASSERT(bValidate, NextBlock == mFreeBlockHead);
										PrevBlock = NextBlock;
										NextBlock = R_C(FBlock*, R_C(TBYTE*, PrevBlock) + FBlock_Size + PrevBlock->size);
									}
								else
									HLVM_LIKELY
									{
										HLVM_CONSTEXPR_ASSERT(bValidate, PrevBlock->size > 0 && NextBlock->size > 0);
										HLVM_CONSTEXPR_ASSERT(bValidate, (NextBlockPrevFreeBlock && NextBlockPrevFreeBlock != NextBlock && NextBlockPrevFreeBlock->nextFreeBlock == NextBlock));
										FBlock* NextBlockNextFreeBlock = NextBlock->nextFreeBlock;
										HLVM_CONSTEXPR_ASSERT(bValidate, NextBlockNextFreeBlock && NextBlockNextFreeBlock != NextBlock && NextBlockNextFreeBlock->prevFreeBlock == NextBlock && (NextBlockNextFreeBlock == mTail || NextBlockNextFreeBlock->size > 0));

										// Eliminate Next block by connect prev and next
										HLVM_CONSTEXPR_ASSERT(bValidate, NextBlockPrevFreeBlock != NextBlockNextFreeBlock);
										NextBlockPrevFreeBlock->nextFreeBlock = NextBlockNextFreeBlock;
										NextBlockNextFreeBlock->prevFreeBlock = NextBlockPrevFreeBlock;

										// Defragment PrevBlock and move on to next block once more
										PrevBlock->size += NextBlock->size + FBlock_Size;
										HLVM_CONSTEXPR_ASSERT(bValidate, PrevBlock->size > 0 && PrevBlock->size <= N - 2 * FBlock_Size);

										NextBlock->prevFreeBlock = nullptr;
										NextBlock->nextFreeBlock = nullptr;

										NextBlock = R_C(FBlock*, R_C(TBYTE*, NextBlock) + FBlock_Size + NextBlock->size);
										HLVM_CONSTEXPR_ASSERT(bValidate, NextBlock == R_C(FBlock*, R_C(TBYTE*, PrevBlock) + FBlock_Size + PrevBlock->size));

										mDefragmentHead = NextBlock;
										break;
									}
							}
					}
					if (mDefragmentHead == mTail)
					{
						mDefragmentHead = R_C(FBlock*, mStack);
					}
				}

				// Swap next block if it is bigger than current free head,
				// so to keep free head a larger block size for easier allocation next time
				if (auto NextBlock = mFreeBlockHead->nextFreeBlock;
					NextBlock->size > mFreeBlockHead->size)
				{
					// Sanity checks
					HLVM_CONSTEXPR_ASSERT(bValidate, NextBlock->prevFreeBlock == mFreeBlockHead);
					auto NextBlockNext = NextBlock->nextFreeBlock;
					HLVM_CONSTEXPR_ASSERT(bValidate, NextBlockNext && NextBlockNext != NextBlock && NextBlockNext->prevFreeBlock == NextBlock);

					// Skip next and connect free head with next block's next
					HLVM_CONSTEXPR_ASSERT(bValidate, NextBlockNext != mFreeBlockHead);
					mFreeBlockHead->nextFreeBlock = NextBlockNext;
					NextBlockNext->prevFreeBlock = mFreeBlockHead;

					// Swap next block with free head
					NextBlock->prevFreeBlock = nullptr;
					NextBlock->nextFreeBlock = mFreeBlockHead;
					mFreeBlockHead->prevFreeBlock = NextBlock;
					mFreeBlockHead = NextBlock;
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
			}
		}
		else
		{
			GMiMallocatorTLS.Free(ptr);
		}
	}

	bool InStackBound(const void* ptr) const
	{
		return ptr >= mLowerBound && ptr < mTail;
	}

	TBYTE	mStack[N];
	FBlock* mFreeBlockHead{ nullptr };
	FBlock* mDefragmentHead{ nullptr };
	FBlock* mTail{ nullptr };
	void*	mLowerBound{ nullptr };
};

template <int32_t N = HLVM_STACK_MALLOCATOR_DEFAULT_SIZE>
using TMonoStackAllocator = TStackMallocator<N, true>;
