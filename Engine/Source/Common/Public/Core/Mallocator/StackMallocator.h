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
#if !HLVM_BUILD_RELEASE
	BIT_FLAG(bValidate){ true };
#else
	BIT_FLAG(bValidate){ false };
#endif
};

template <int32_t N = 4 * 1024 * 1024>
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
		mFreeBlockHead->SetSize(N - 2 * FBlock_Size); // Stack size minus head block and tail block

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
	using SIZE_T = int32_t;

	PACK(struct FBlock {
		FBlock*					prevFreeBlock{ nullptr };
		FBlock*					nextFreeBlock{ nullptr };
		SIZE_T					size{ 0 };
		HLVM_INLINE_FUNC SIZE_T GetSize() const
		{
			return std::abs(size);
		}
		HLVM_INLINE_FUNC void SetSize(SIZE_T value)
		{
			size = value;
		}
		HLVM_INLINE_FUNC bool GetFree() const
		{
			return size >= 0;
		}
	});
	HLVM_STATIC_VAR constexpr SIZE_T FBlock_Size = S_C(SIZE_T, sizeof(FBlock));
	static_assert(2 * FBlock_Size < N);

	void* InternalMalloc(size_t _size)
	{
		SIZE_T size = S_C(SIZE_T, _size);
		assert(size < N);
		assert(mFreeBlockHead->prevFreeBlock == nullptr);
		FBlock* FreeBlock = mFreeBlockHead;
		while (FreeBlock->nextFreeBlock != nullptr)
		{
			assert(FreeBlock->GetFree());
			if (FreeBlock->GetSize() >= size + FBlock_Size)
			{
				auto NextFreeBlock = FreeBlock->nextFreeBlock;
				auto PrevFreeBlock = FreeBlock->prevFreeBlock;

				// Reset current free block
				FreeBlock->nextFreeBlock = nullptr;
				FreeBlock->prevFreeBlock = nullptr;

				auto NewFreeBlock = R_C(FBlock*, R_C(TBYTE*, FreeBlock) + FBlock_Size + size);
				*NewFreeBlock = FBlock();
				NewFreeBlock->SetSize(FreeBlock->GetSize() - FBlock_Size - size);
				if (NewFreeBlock->GetSize() == 0)
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
							// Only free head has null prev pointer, double check that
							assert(mFreeBlockHead == FreeBlock);
							// Otherwise, assign head to next
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
							// Otherwise, assign head to new
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
				assert(FreeBlock->GetSize() > 0);
				assert(!FreeBlock->GetFree());
				assert(mFreeBlockHead->GetFree());

				FreeBlock->size = -FreeBlock->size;
				FreeBlock->prevFreeBlock = nullptr;

				// Exchange new free block with free head
				mFreeBlockHead->prevFreeBlock = FreeBlock;
				FreeBlock->nextFreeBlock = mFreeBlockHead;
				mFreeBlockHead = FreeBlock;

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
				if (mCtx.bValidate)
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
