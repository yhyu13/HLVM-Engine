/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Core/Mallocator/VMMallocator/HeapMallocator.h"

DELCARE_LOG_CATEGORY(LogHeapMallocator)

void* FHeapMallocator::Malloc(size_t _size)
{
	if (!bManaged)
	{
		HLVM_CONSTEXPR_ASSERT(bValidate, mHeap != nullptr);
		return mHeap;
	}
	else
	{
		SizeType size = S_C(SizeType, _size);
		HLVM_CONSTEXPR_ASSERT(bValidate, size > 0 && S_C(size_t, size) <= GetManagedSize());
		HLVM_CONSTEXPR_ASSERT(bValidate, mFreeBlockHead->prevFreeBlock == nullptr);
		if (mFreeSizeUpperBound < 0 || size <= mFreeSizeUpperBound)
			HLVM_LIKELY
			{
				mFreeSizeUpperBound = -1;
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

						// Mark current free block not free anymore
						FreeBlock->size = (-size);
						HLVM_CONSTEXPR_ASSERT(bValidate, FreeBlock->size < 0);

						// Reset free size upper bound since allocation success
						mFreeSizeUpperBound = -1;

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
		HLVM_LOG(LogHeapMallocator, trace, TXT("Failed to allocate memory from heap allocator size {}, free block upper bound {}"), size, mFreeSizeUpperBound);
		return nullptr;
	}
}

void FHeapMallocator::Free(void* p)
{
	(void)p;
	if (!bManaged)
	{
		Destroy();
		return;
	}
	HLVM_CONSTEXPR_ASSERT(bValidate, Owned(p));
	// TODO
	{
	}
}
