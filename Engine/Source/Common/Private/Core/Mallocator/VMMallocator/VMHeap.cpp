/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Core/Mallocator/VMMallocator/VMHeap.h"
#include "Core/Mallocator/VMMallocator/VMArena.h"
#include "Core/Log.h"

DECLARE_LOG_CATEGORY(LogHeapMallocator)

void FVMHeap::Init(FVMArena* _VMArena, size_t _size, bool bForceUnManaged)
{
	VMArena = _VMArena;
	N = _size;

	if (bForceUnManaged)
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
		HLVM_CONSTEXPR_ASSERT(bValidate, (GetManagedSize() > 0));
		mHeap = R_C(TBYTE*, VMArena->MallocOSPage(N));

		// Init head block
		FHeapHeadBlock* HeapHead = std::construct_at(R_C(FHeapHeadBlock*, mHeap));
		HeapHead->OwnerHeap = this;

		// Init stack and free block head which occupy the whole stack
		mFreeBlockHead = std::construct_at(R_C(FBlock*, mHeap + sizeof(FHeapHeadBlock)));
		mFreeBlockHead->size = S_C(SizeType, GetManagedSize()); // Stack size minus head block and tail block
		HLVM_CONSTEXPR_ASSERT(bValidate, mFreeBlockHead->size > 0);

		// Init tail which is trivially free
		mTail = std::construct_at(R_C(FBlock*, mHeap + N - FBlock_Size));
		mTail->prevFreeBlock = mFreeBlockHead;

		// Connect head to tail
		mFreeBlockHead->nextFreeBlock = mTail;

		// Cache the lower bound memory address for allocatable heap pointers
		// And the upperbound is simply mTail
		mLowerBound = R_C(TBYTE*, mFreeBlockHead) + FBlock_Size;
	}
	else
	{
		mHeap = R_C(TBYTE*, VMArena->MallocLowLevel(N));
		// Init head block
		FHeapHeadBlock* HeapHead = std::construct_at(R_C(FHeapHeadBlock*, mHeap));
		HeapHead->OwnerHeap = this;
	}
}

void FVMHeap::Destroy()
{
	if (mHeap)
	{
		if (bManaged)
		{
			VMArena->FreeOSPage(mHeap);
		}
		else
		{
			VMArena->FreeLowLevel(mHeap);
		}
		mHeap = nullptr;
	}
}

void* FVMHeap::Malloc(size_t _size)
{
	if (!bManaged)
		HLVM_UNLIKELY
		{
			HLVM_CONSTEXPR_ASSERT(bValidate, mHeap != nullptr);
			auto RetPtr = mHeap + sizeof(FHeapHeadBlock);
			return RetPtr;
		}
	else
		HLVM_LIKELY
		{
			void*	 RetPtr{ nullptr };
			SizeType size = S_C(SizeType, _size);
			HLVM_CONSTEXPR_ASSERT(bValidate, size > 0 && S_C(size_t, size) <= GetManagedSize());
			HLVM_CONSTEXPR_ASSERT(bValidate, mFreeBlockHead->prevFreeBlock == nullptr);
			{
				FBlock* FreeBlock = mFreeBlockHead;
				HLVM_CONSTEXPR_ASSERT(bValidate, FreeBlock->nextFreeBlock != nullptr);
				{
					HLVM_CONSTEXPR_ASSERT(bValidate, FreeBlock->GetFree());
					HLVM_CONSTEXPR_ASSERT(bValidate, FreeBlock->size >= size);
					{
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

								{
									// Make sure the free block to allocate is the head free block
									HLVM_CONSTEXPR_ASSERT(bValidate, PrevFreeBlock == nullptr);
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
								// Assign NewFreeBlock to FreeBlock so that we can iterate to new free block later
								FreeBlock->nextFreeBlock = NewFreeBlock;

								NewFreeBlock->nextFreeBlock = NextFreeBlock;
								NewFreeBlock->prevFreeBlock = PrevFreeBlock;
								NextFreeBlock->prevFreeBlock = NewFreeBlock;

								{
									// Make sure the free block to allocate is the head free block
									HLVM_CONSTEXPR_ASSERT(bValidate, PrevFreeBlock == nullptr);
									HLVM_CONSTEXPR_ASSERT(bValidate, mFreeBlockHead == FreeBlock);
									// Otherwise, assign head to new free block
									mFreeBlockHead = NewFreeBlock;
								}

								// Now try swap the head free block to its right position in the free list
								// Sort free block list
								{
									SortFreeBlockList();
								}
							}
						HLVM_CONSTEXPR_ASSERT(bValidate, mFreeBlockHead->prevFreeBlock == nullptr);
						HLVM_CONSTEXPR_ASSERT(bValidate, mFreeBlockHead != FreeBlock);

						// Return actual pointer address
						RetPtr = R_C(TBYTE*, FreeBlock) + FBlock_Size;
					}
				}
			}
			return RetPtr;
		}
}

void FVMHeap::Free(void* p)
{
	HLVM_CONSTEXPR_ASSERT(bValidate, Owned(p));
	HLVM_ASSERT(mHeap, TXT("calling free on nullptr heap with pointer {} to be free"), p);
	if (!bManaged)
		HLVM_UNLIKELY
		{
			Destroy();
		}
	else
		HLVM_LIKELY
		{
			// Reset new block to free
			FBlock* FreeBlock = R_C(FBlock*, R_C(TBYTE*, p) - FBlock_Size);
			HLVM_CONSTEXPR_ASSERT(bValidate, FreeBlock->size < 0);
			HLVM_CONSTEXPR_ASSERT(bValidate, !FreeBlock->GetFree());
			HLVM_CONSTEXPR_ASSERT(bValidate, mFreeBlockHead->GetFree());
			HLVM_CONSTEXPR_ASSERT(bValidate, mFreeBlockHead->prevFreeBlock == nullptr);

			// Set free block to free again
			FreeBlock->size = (-FreeBlock->size);
			HLVM_CONSTEXPR_ASSERT(bValidate, FreeBlock->size > 0 && S_C(size_t, FreeBlock->size) <= GetManagedSize());

			// Exchange new free block with free head
			FreeBlock->prevFreeBlock = nullptr;
			FreeBlock->nextFreeBlock = mFreeBlockHead;
			mFreeBlockHead->prevFreeBlock = FreeBlock;
			mFreeBlockHead = FreeBlock;

			// Defragmentation next physical block if it is free
			{
				FBlock* CurrBlock = mFreeBlockHead;
				FBlock* NextBlock = R_C(FBlock*, R_C(TBYTE*, CurrBlock) + FBlock_Size + CurrBlock->size);
				while (NextBlock != mTail)
				{
					HLVM_CONSTEXPR_ASSERT(bValidate, CurrBlock->size != 0 && NextBlock->size != 0);
					if (NextBlock->size < 0)
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
						HLVM_CONSTEXPR_ASSERT(bValidate, CurrBlock->size > 0 && S_C(size_t, CurrBlock->size) <= GetManagedSize());

						NextBlock = R_C(FBlock*, R_C(TBYTE*, NextBlock) + FBlock_Size + NextBlock->size);
						HLVM_CONSTEXPR_ASSERT(bValidate, NextBlock == R_C(FBlock*, R_C(TBYTE*, CurrBlock) + FBlock_Size + CurrBlock->size));
					}
				}
			}

			// Sort free block list
			{
				SortFreeBlockList();
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

#include "Template/PrintTemplate.tpp"

// TODO : implement free list sharding (i.e. using multiple free list to store sorted free pointers, a
//  and use each free list for malloc)
void FVMHeap::SortFreeBlockList()
{
	static int LongIterCount = 0;
	static int MaxIter = 0;
	int		   Iter = 0;

	FBlock* CurrBlock = mFreeBlockHead;
	HLVM_CONSTEXPR_ASSERT(bValidate, CurrBlock->size > 0);
	FBlock* NextBlock = CurrBlock->nextFreeBlock;
	HLVM_CONSTEXPR_ASSERT(bValidate, NextBlock->size >= 0);
	/**
	 * Find a insert block that is smaller than or equal current block
	 */
	FBlock* InsertBlock = NextBlock;
	// if mid block is greater than current block, use it
	bool bUsedMid = false;
	if (mMid)
	{
		HLVM_CONSTEXPR_ASSERT(bValidate, mMid != CurrBlock);
		HLVM_CONSTEXPR_ASSERT(bValidate, mMid->size > 0);
		if (mMid->size > CurrBlock->size)
		{
			bUsedMid = true;
			InsertBlock = mMid->nextFreeBlock;
			HLVM_CONSTEXPR_ASSERT(bValidate, InsertBlock);
			HLVM_CONSTEXPR_ASSERT(bValidate, InsertBlock->size >= 0);
		}
	}
	/**
	 * Find a insert block that is smaller than or equal current block
	 * and insert current block before it
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

		if (!mMid)
			HLVM_UNLIKELY
			{
				mMid = CurrBlock;
			}
		else
			HLVM_LIKELY
			{
				mMid = (bUsedMid) ? mMid->nextFreeBlock : mMid->prevFreeBlock;
			}
	}

#if !HLVM_BUILD_RELEASE
	if (MaxIter < Iter)
	{
		MaxIter = Iter;
		StreamPrintf(&std::cout, "MaxIter: %s\n", MaxIter);
	}
	if (Iter > 100)
	{
		StreamPrintf(&std::cout, "Iter: %s %s\n", Iter, ++LongIterCount);
	}
#endif
}
