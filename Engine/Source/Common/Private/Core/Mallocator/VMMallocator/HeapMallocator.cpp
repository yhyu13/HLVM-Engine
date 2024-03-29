/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Core/Mallocator/VMMallocator/HeapMallocator.h"
#include "Core/Mallocator/VMMallocator/VMArena.h"

DECLARE_LOG_CATEGORY(LogHeapMallocator)

void FHeapMallocator::Init(FVMArena* _VMArena, size_t _size, bool bForceUnManage)
{
	VMArena = _VMArena;
	N = _size;

	if (bForceUnManage)
	{
		bManaged = false;
	}
	else
	{
		bManaged = N < std::numeric_limits<SizeType>::max();
	}
	if (bManaged)
	{
		HLVM_CONSTEXPR_ASSERT(bValidate, (N & (N - 1)) == 0); // Must be power of 2
		HLVM_CONSTEXPR_ASSERT(bValidate, (N - 2 * FBlock_Size > 0));
		mHeap = R_C(TBYTE*, VMArena->MallocOS(N, N)); // Aligned Alloc so that we can infer Heap pointer by pointer arithmetic

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
	else
	{
		mHeap = R_C(TBYTE*, VMArena->MallocOS(N));
	}
}

void FHeapMallocator::Destroy()
{
	if (mHeap)
	{
		VMArena->FreeOS(mHeap);
		mHeap = nullptr;
		bManaged = false;
	}
}

void* FHeapMallocator::Malloc(size_t _size)
{
	if (!bManaged)
	{
		HLVM_CONSTEXPR_ASSERT(bValidate, mHeap != nullptr);
		return mHeap;
	}
	else
	{
		void*	 RetPtr{ nullptr };
		SizeType size = S_C(SizeType, _size);
		HLVM_CONSTEXPR_ASSERT(bValidate, size > 0 && S_C(size_t, size) <= N - 2 * FBlock_Size);
		HLVM_CONSTEXPR_ASSERT(bValidate, mFreeBlockHead->prevFreeBlock == nullptr);
		/**
		 * Unlike StackAllocator, where we stop looping when there is a fit free block,
		 * Here we loop all free blocks all the fime to calcualte the correct free block size upper bound
		 */
		if (size <= mFreeSizeUpperBound)
		{
			HLVM_CONSTEXPR_ASSERT(bValidate, mFreeSizeUpperBound >= 0);
			mFreeSizeUpperBound = -1;
			FBlock* FreeBlock = mFreeBlockHead;
			while (FreeBlock->nextFreeBlock != nullptr)
			{
				HLVM_CONSTEXPR_ASSERT(bValidate, FreeBlock->GetFree());
				if (!RetPtr && FreeBlock->size >= size)
				{
					FBlock* NextFreeBlock = FreeBlock->nextFreeBlock;
					HLVM_CONSTEXPR_ASSERT(bValidate, NextFreeBlock && NextFreeBlock->prevFreeBlock == FreeBlock && (NextFreeBlock == mTail || NextFreeBlock->size > 0));
					FBlock* PrevFreeBlock = FreeBlock->prevFreeBlock;

					SizeType NewFreeBlockSize = (FreeBlock->size - FBlock_Size - size);
					if (NewFreeBlockSize <= Minimal_Block_Size)
					{
						// New free block is trivial
						// Mark current free block not free anymore
						FreeBlock->size = (-FreeBlock->size);
						NextFreeBlock->prevFreeBlock = PrevFreeBlock;
						//						if (PrevFreeBlock)
						//						{
						//							HLVM_CONSTEXPR_ASSERT(bValidate,
						//								PrevFreeBlock && PrevFreeBlock->nextFreeBlock == FreeBlock && PrevFreeBlock->size > 0);
						//							PrevFreeBlock->nextFreeBlock = NextFreeBlock;
						//						}
						//						else
						{
							HLVM_CONSTEXPR_ASSERT(bValidate, PrevFreeBlock == nullptr);
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
						// Assign NewFreeBlock to FreeBlock so that we can iterate to new free block later
						FreeBlock->nextFreeBlock = NewFreeBlock;

						NewFreeBlock->nextFreeBlock = NextFreeBlock;
						NewFreeBlock->prevFreeBlock = PrevFreeBlock;
						NextFreeBlock->prevFreeBlock = NewFreeBlock;
						//						if (PrevFreeBlock)
						//						{
						//							HLVM_CONSTEXPR_ASSERT(bValidate,
						//								PrevFreeBlock && PrevFreeBlock->nextFreeBlock == FreeBlock && PrevFreeBlock->size > 0);
						//							PrevFreeBlock->nextFreeBlock = NewFreeBlock;
						//						}
						//						else
						{
							HLVM_CONSTEXPR_ASSERT(bValidate, PrevFreeBlock == nullptr);
							HLVM_CONSTEXPR_ASSERT(bValidate, mFreeBlockHead == FreeBlock);
							// Otherwise, assign head to new free block
							mFreeBlockHead = NewFreeBlock;
						}
					}
					HLVM_CONSTEXPR_ASSERT(bValidate, mFreeBlockHead->prevFreeBlock == nullptr);
					HLVM_CONSTEXPR_ASSERT(bValidate, mFreeBlockHead != FreeBlock);

					// Return actual pointer address
					TBYTE* ptr = R_C(TBYTE*, FreeBlock) + FBlock_Size;
					RetPtr = ptr;
				}
				// Iterate to next free block
				if (mFreeSizeUpperBound < FreeBlock->size)
				{
					mFreeSizeUpperBound = FreeBlock->size;
				}
				FreeBlock = FreeBlock->nextFreeBlock;
			}
			HLVM_CONSTEXPR_ASSERT(bValidate, FreeBlock == mTail);
		}
		if (!RetPtr)
		{
			HLVM_LOG(LogHeapMallocator, debug, TXT("Failed to allocate memory from heap allocator size {}, free block upper bound {}"), size, mFreeSizeUpperBound);
		}
		return RetPtr;
	}
}

void FHeapMallocator::Free(void* p)
{
	HLVM_CONSTEXPR_ASSERT(bValidate, Owned(p));
	HLVM_ENSURE(mHeap, TXT("calling free on nullptr heap with pointer {} to be free"), p);
	if (!bManaged)
	{
		Destroy();
	}
	else
	{
		// Reset new block to free
		FBlock* FreeBlock = R_C(FBlock*, R_C(TBYTE*, p) - FBlock_Size);
		HLVM_CONSTEXPR_ASSERT(bValidate, FreeBlock->size < 0);
		HLVM_CONSTEXPR_ASSERT(bValidate, !FreeBlock->GetFree());
		HLVM_CONSTEXPR_ASSERT(bValidate, mFreeBlockHead->GetFree());
		HLVM_CONSTEXPR_ASSERT(bValidate, mFreeBlockHead->prevFreeBlock == nullptr);

		// Set free block to free again
		FreeBlock->size = (-FreeBlock->size);
		HLVM_CONSTEXPR_ASSERT(bValidate, FreeBlock->size > 0 && S_C(size_t, FreeBlock->size) <= N - 2 * FBlock_Size);

		// Exchange new free block with free head
		FreeBlock->prevFreeBlock = nullptr;
		FreeBlock->nextFreeBlock = mFreeBlockHead;
		mFreeBlockHead->prevFreeBlock = FreeBlock;
		mFreeBlockHead = FreeBlock;

		// Defragmentation next physical block if it is free
		{
			FBlock* CurrBlock = mFreeBlockHead;
			FBlock* NextBlock = R_C(FBlock*, R_C(TBYTE*, CurrBlock) + FBlock_Size + std::abs(CurrBlock->size));
			// While not reach the tail
			while (NextBlock != mTail)
			{
				HLVM_CONSTEXPR_ASSERT(bValidate, CurrBlock->size != 0 && NextBlock->size != 0);
				if (CurrBlock->size < 0 || NextBlock->size < 0)
				{
					// If not both blocks are free, continue
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
					HLVM_CONSTEXPR_ASSERT(bValidate, CurrBlock->size > 0 && S_C(size_t, CurrBlock->size) <= N - 2 * FBlock_Size);

					// Update upper bound if necessary
					if (mFreeSizeUpperBound < CurrBlock->size)
					{
						mFreeSizeUpperBound = CurrBlock->size;
					}

					NextBlock = R_C(FBlock*, R_C(TBYTE*, NextBlock) + FBlock_Size + NextBlock->size);
					HLVM_CONSTEXPR_ASSERT(bValidate, NextBlock == R_C(FBlock*, R_C(TBYTE*, CurrBlock) + FBlock_Size + CurrBlock->size));
				}
			}
		}

		{
			// Swap next block if it is bigger than current free head,
			// so to keep free head a larger block size for easier allocation next time
			FBlock* CurrBlock = mFreeBlockHead;
			FBlock* NextBlock = CurrBlock->nextFreeBlock;
			bool	bFirst = true;
			while (NextBlock->size > CurrBlock->size)
			{
				// Sanity checks
				HLVM_CONSTEXPR_ASSERT(bValidate, NextBlock->prevFreeBlock == CurrBlock);
				FBlock* NextBlockNext = NextBlock->nextFreeBlock;
				HLVM_CONSTEXPR_ASSERT(bValidate, NextBlockNext && NextBlockNext != NextBlock && NextBlockNext->prevFreeBlock == NextBlock);

				// Skip next and connect current with next block's next
				HLVM_CONSTEXPR_ASSERT(bValidate, NextBlockNext != CurrBlock);
				CurrBlock->nextFreeBlock = NextBlockNext;
				NextBlockNext->prevFreeBlock = CurrBlock;

				// Skip current and connect current block's prev with next
				FBlock* CurrPrevBlock = CurrBlock->prevFreeBlock;
				NextBlock->prevFreeBlock = CurrPrevBlock;
				if (CurrPrevBlock)
				{
					CurrPrevBlock->nextFreeBlock = NextBlock;
				}

				// Swap next block with free head
				NextBlock->nextFreeBlock = CurrBlock;
				CurrBlock->prevFreeBlock = NextBlock;

				// Only update mFreeBlockHead once!
				if (bFirst)
				{
					bFirst = false;
					mFreeBlockHead = NextBlock;
				}

				// Iterate to next block
				NextBlock = NextBlockNext;
			}
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
