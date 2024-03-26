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
	bool		  bValidate = HLVM_MALLOC_VALIDATION>
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
#if 0
	PACK(struct FBlock {
		FBlock*	 prevFreeBlock{ nullptr };
		FBlock*	 nextFreeBlock{ nullptr };
		SizeType size{ 0 };

		HLVM_INLINE_FUNC bool GetFree() const
		{
			return size >= 0;
		}
	});
	static_assert(sizeof(FBlock) == 20, "FBlock size must be 20 bytes");
#else
	/**
	 * Use int32 offset to this pointer to represent another pointer, approximately 1%~3% slower than using raw pointer
	 */
	struct FBlock;
	struct FBlockOffsetPtr
	{
		int32_t offset{ 0x7FFFFFFF };
		operator FBlock*()
		{
			return (offset != 0x7FFFFFFF) ? R_C(FBlock*, R_C(TBYTE*, this) + offset) : nullptr;
		}
		operator const FBlock*() const
		{
			return (offset != 0x7FFFFFFF) ? R_C(const FBlock*, R_C(const TBYTE*, this) + offset) : nullptr;
		}
		FBlock* operator=(FBlock* lhs)
		{
			(lhs != nullptr) ? offset = S_C(int32_t, (R_C(TBYTE*, lhs) - R_C(TBYTE*, this))) : offset = 0x7FFFFFFF;
			return lhs;
		}
		FBlock* operator->()
		{
			return S_C(FBlock*, *this);
		}
		const FBlock* operator->() const
		{
			return S_C(const FBlock*, *this);
		}
		bool operator==(FBlock* rhs) const
		{
			return S_C(const FBlock*, *this) == rhs;
		}
	};

	PACK(struct FBlock {
		FBlockOffsetPtr prevFreeBlock{};
		FBlockOffsetPtr nextFreeBlock{};
		SizeType		size{ 0 };

		HLVM_INLINE_FUNC bool GetFree() const
		{
			return size >= 0;
		}
	});
	static_assert(sizeof(FBlock) == 12, "FBlock size must be 20 bytes");
#endif
	HLVM_INLINE_VAR HLVM_STATIC_VAR constexpr SizeType FBlock_Size = S_C(SizeType, sizeof(FBlock));
	static_assert(N - 2 * FBlock_Size > 0);
	HLVM_INLINE_VAR HLVM_STATIC_VAR constexpr SizeType Minimual_Block_Size = 24;

	void* InternalMalloc(size_t _size) noexcept(bValidate)
	{
		SizeType size = S_C(SizeType, _size);
		HLVM_CONSTEXPR_ASSERT(bValidate, size > 0 && size <= N - 2 * FBlock_Size);
		HLVM_CONSTEXPR_ASSERT(bValidate, mFreeBlockHead->prevFreeBlock == nullptr);
		if (mFreeSizeUpperBound < 0 || size <= mFreeSizeUpperBound)
		{
			FBlock* FreeBlock = mFreeBlockHead;
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
					if (NewFreeBlockSize < Minimual_Block_Size)
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
					mFreeSizeUpperBound = std::max(mFreeSizeUpperBound, FreeBlock->size);
					FreeBlock = FreeBlock->nextFreeBlock;
				}
			}
			HLVM_CONSTEXPR_ASSERT(bValidate, FreeBlock == mTail);
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

	void
	InternalFree(void* ptr) noexcept(bValidate)
	{
		if (InStackBound(ptr))
		{
			if constexpr (bMonolithic)
			{
				return;
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
				HLVM_CONSTEXPR_ASSERT(bValidate, FreeBlock->size > 0 && FreeBlock->size <= N - 2 * FBlock_Size);

				// Exchange new free block with free head
				FreeBlock->prevFreeBlock = nullptr;
				FreeBlock->nextFreeBlock = mFreeBlockHead;
				mFreeBlockHead->prevFreeBlock = FreeBlock;
				mFreeBlockHead = FreeBlock;

				// Defragmentation next physical block if it is free
				if constexpr (bDefragment)
				{
					FBlock* PrevBlock = R_C(FBlock*, mStack);
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

										// Update upper bound if necessary
										if (mFreeSizeUpperBound >= 0 && mFreeSizeUpperBound < PrevBlock->size)
										{
											mFreeSizeUpperBound = PrevBlock->size;
										}

										NextBlock = R_C(FBlock*, R_C(TBYTE*, NextBlock) + FBlock_Size + NextBlock->size);
										HLVM_CONSTEXPR_ASSERT(bValidate, NextBlock == R_C(FBlock*, R_C(TBYTE*, PrevBlock) + FBlock_Size + PrevBlock->size));
									}
							}
					}
				}

				// Swap next block if it is bigger than current free head,
				// so to keep free head a larger block size for easier allocation next time
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

	bool InStackBound(void* ptr) const
	{
		return ptr >= mLowerBound && ptr < mTail;
	}

	TBYTE	 mStack[N];
	FBlock*	 mFreeBlockHead{ nullptr };
	FBlock*	 mTail{ nullptr };
	void*	 mLowerBound{ nullptr };
	SizeType mFreeSizeUpperBound{ -1 };
};

/**
 * Default Stack allocator is no monolithic,
 * So we create a monolithic variant
 */
template <int32_t N = HLVM_STACK_MALLOCATOR_DEFAULT_SIZE>
using TStackMonolithicAllocator = TStackMallocator<N, true>;
