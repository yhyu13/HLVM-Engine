/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "MiMallocator.h"
#include "Core/Assert.h"

/**
 * General purpose Stack allocator
 */
struct FStackMallocatorContext
{
	bool bMonolithic{ false };
	BIT_FLAG(bAllowOverflowToHeap){ true }; // Use mimallocator for overflowed allocation
	BIT_FLAG(bDefragment){ false };
};

template <int32_t N = 4 * 1024 * 1024, bool bValidate = !HLVM_BUILD_RELEASE>
class FStackMallocator final : public IMallocator
{
public:
	NOCOPYMOVE(FStackMallocator)
	FStackMallocator(const FStackMallocatorContext& _Ctx = FStackMallocatorContext())
		: mCtx(_Ctx)
	{
		Type = EMallocator::Stack;

		// Init stack and free block head which occupy the whole stack
		std::memset(mStack, 0, N);
		mFreeBlockHead = R_C(FBlock*, mStack);
		*mFreeBlockHead = FBlock();
		mFreeBlockHead->size = (N - 2 * FBlock_Size); // Stack size minus head block and tail block

		// Init tail which is travially free
		mTail = R_C(FBlock*, mStack + N - FBlock_Size);
		*mTail = FBlock();
		mTail->prevFreeBlock = mFreeBlockHead;

		// Connect head to tail
		mFreeBlockHead->nextFreeBlock = mTail;

		// Cache the lower bound memory address for stack pointers
		mLowerBound = mStack + FBlock_Size;
	}
	~FStackMallocator() final override
	{
	}
	HLVM_INLINE_FUNC virtual bool Owened(void* ptr) noexcept final override
	{
		if (InStackBound(ptr))
		{
			return true;
		}
		else if (mCtx.bAllowOverflowToHeap && mMiMallocator.has_value())
		{
			return mMiMallocator->Owened(ptr);
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
	using SizeType = int32_t;

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
	static_assert(2 * FBlock_Size < N);

	void* InternalMalloc(size_t _size)
	{
		SizeType size = S_C(SizeType, _size);
		assert(size < N);
		assert(mFreeBlockHead->prevFreeBlock == nullptr);
		FBlock* FreeBlock = mFreeBlockHead;
		while (FreeBlock->nextFreeBlock != nullptr)
		{
			assert(FreeBlock->GetFree());
			if (FreeBlock->size >= size + FBlock_Size)
			{
				auto NextFreeBlock = FreeBlock->nextFreeBlock;
				auto PrevFreeBlock = FreeBlock->prevFreeBlock;

				auto NewFreeBlock = R_C(FBlock*, R_C(TBYTE*, FreeBlock) + FBlock_Size + size);
				*NewFreeBlock = FBlock();
				NewFreeBlock->size = (FreeBlock->size - FBlock_Size - size);
				if (NewFreeBlock->size == 0)
					HLVM_UNLIKELY
					{
						// New free block is trivial, simply ignore it and connect next and prev to each other
						NextFreeBlock->prevFreeBlock = PrevFreeBlock;
						if (PrevFreeBlock)
						{
							PrevFreeBlock->nextFreeBlock = NextFreeBlock;
						}
						else
						{
							// Since only free head has null prev pointer, double check that we have hit this case
							assert(mFreeBlockHead == FreeBlock);
							// Make next free block new head
							mFreeBlockHead = NextFreeBlock;
						}
					}
				else
					HLVM_LIKELY
					{
						NewFreeBlock->nextFreeBlock = NextFreeBlock;
						NewFreeBlock->prevFreeBlock = PrevFreeBlock;
						// Otherwise, connect next and prev to new free block
						NextFreeBlock->prevFreeBlock = NewFreeBlock;
						if (PrevFreeBlock)
						{
							PrevFreeBlock->nextFreeBlock = NewFreeBlock;
						}
						else
						{
							// Otherwise, assign head to new free block
							mFreeBlockHead = NewFreeBlock;
						}
					}
				assert(mFreeBlockHead->prevFreeBlock == nullptr);

				// Mark current free block not free anymore
				FreeBlock->size = -size;

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
		if (mCtx.bAllowOverflowToHeap)
		{
			if (!mMiMallocator.has_value())
			{
				mMiMallocator.emplace(FMiMallocatorContext{ .bNewHeap = true, .bDestory = true });
			}
			return mMiMallocator->Malloc(size);
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
			if (mCtx.bMonolithic)
			{
				return;
			}
			else
			{
				// Reset new block to free
				FBlock* FreeBlock = R_C(FBlock*, ptr - FBlock_Size);
				assert(FreeBlock->size < 0);
				assert(!FreeBlock->GetFree());
				assert(mFreeBlockHead->GetFree());
				assert(mFreeBlockHead->prevFreeBlock == nullptr);

				// Set free block to free again
				FreeBlock->size = -FreeBlock->size;

				// Exchange new free block with free head
				mFreeBlockHead->prevFreeBlock = FreeBlock;
				FreeBlock->nextFreeBlock = mFreeBlockHead;
				mFreeBlockHead = FreeBlock;
				mFreeBlockHead->prevFreeBlock = nullptr;

				// Defragmentation next physical block if it is free
				if (mCtx.bDefragment)
				{
					FBlock* NextBlock = R_C(FBlock*, R_C(TBYTE*, FreeBlock) + FBlock_Size + FreeBlock->size);
					while (NextBlock->size > 0)
					{
						// Sanity checks
						auto NextBlockPrevFreeBlock = NextBlock->prevFreeBlock;
						assert(NextBlockPrevFreeBlock && NextBlockPrevFreeBlock != NextBlock && NextBlockPrevFreeBlock->nextFreeBlock == NextBlock);
						auto NextBlockNextFreeBlock = NextBlock->nextFreeBlock;
						assert(NextBlockNextFreeBlock && NextBlockNextFreeBlock != NextBlock && NextBlockNextFreeBlock->prevFreeBlock == NextBlock);

						// Eliminate Next block by connect prev and next
						NextBlockPrevFreeBlock->nextFreeBlock = NextBlockNextFreeBlock;
						NextBlockNextFreeBlock->prevFreeBlock = NextBlockPrevFreeBlock;

						// Defragment FreeBlock and move on to next block once more
						FreeBlock->size += NextBlock->size + FBlock_Size;
						NextBlock = R_C(FBlock*, R_C(TBYTE*, FreeBlock) + FBlock_Size + FreeBlock->size);
					}
				}
				assert(mFreeBlockHead->prevFreeBlock == nullptr);

				/**
				 * Check free block validity
				 */
				if constexpr (bValidate)
				{
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
		else if (mCtx.bAllowOverflowToHeap && mMiMallocator.has_value())
		{
			mMiMallocator->Free(ptr);
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

	TBYTE						 mStack[N];
	FBlock*						 mFreeBlockHead{ nullptr };
	FBlock*						 mTail{ nullptr };
	void*						 mLowerBound{ nullptr };
	std::optional<FMiMallocator> mMiMallocator{ std::nullopt }; // Use mimallocator when out of stack
	FStackMallocatorContext		 mCtx;
};
