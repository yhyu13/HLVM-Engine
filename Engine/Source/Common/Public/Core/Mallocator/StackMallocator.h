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
#if !HLVM_BUILD_RELEASE
	BIT_FLAG(bValidate){ true };
#else
	BIT_FLAG(bValidate){ false };
#endif
};

template <size_t N = 4 * 1024 * 1024>
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
		mFreeBlockHead->size = N - 2 * sizeof(FBlock); // Stack size minus head block and tail block

		// Init tail which is travially free
		FBlock* Tail = R_C(FBlock*, mStack + N - sizeof(FBlock));
		*Tail = FBlock();
		Tail->prevFreeBlock = mFreeBlockHead;

		// Connect head to tail
		mFreeBlockHead->nextFreeBlock = Tail;
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
	// TODO : optimize FBlock size using 31 bit size and 48 bit uintptr_t
	PACK(struct FBlock {
		FBlock* prevFreeBlock{ nullptr };
		FBlock* nextFreeBlock{ nullptr };
		size_t	size : 63 { 0 };
		BIT_FLAG(free){ true };
	});
	static_assert(2 * sizeof(FBlock) < N);

	void* InternalMalloc(size_t size)
	{
		HLVM_ASSERT(size < N, TXT("Required size {} is greater than stack size {}"), size, N);
		HLVM_ASSERT(mFreeBlockHead->prevFreeBlock == nullptr, TXT("Free block head should have null prev pointer"));
		FBlock* FreeBlock = mFreeBlockHead;
		while (FreeBlock->nextFreeBlock != nullptr)
		{
			HLVM_ASSERT(FreeBlock->free, TXT("Free block not freed"));
			if (FreeBlock->size >= size + sizeof(FBlock))
			{
				auto NextFreeBlock = FreeBlock->nextFreeBlock;
				auto PrevFreeBlock = FreeBlock->prevFreeBlock;

				// Reset current free block
				FreeBlock->nextFreeBlock = nullptr;
				FreeBlock->prevFreeBlock = nullptr;

				auto NewFreeBlock = R_C(FBlock*, R_C(TBYTE*, FreeBlock) + sizeof(FBlock) + size);
				*NewFreeBlock = FBlock();
				NewFreeBlock->size = FreeBlock->size - sizeof(FBlock) - size;
				HLVM_ASSERT(NewFreeBlock->size < N, TXT("Free block exceed stack size"));
				if (NewFreeBlock->size == 0)
					HLVM_UNLIKELY
					{
						// New free block is trivial, simply ignore it and connect next and prev to each other
						NextFreeBlock->prevFreeBlock = PrevFreeBlock;
						if (PrevFreeBlock)
						{
							PrevFreeBlock->nextFreeBlock = NextFreeBlock;
							// If prev free exists, assign head to prev,
							// and iterate until we reach a head whose prev free block is null
							mFreeBlockHead = PrevFreeBlock;
							while (mFreeBlockHead->prevFreeBlock)
							{
								const bool bValid = mFreeBlockHead != mFreeBlockHead->prevFreeBlock;
								HLVM_ASSERT(bValid, TXT("Free block head should not have self dependency"));
								mFreeBlockHead = mFreeBlockHead->prevFreeBlock;
							}
						}
						else
						{
							// Otherwise, assign head to next
							mFreeBlockHead = NextFreeBlock;
							mFreeBlockHead->prevFreeBlock = nullptr;
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
							// If prev free exists, assign head to prev,
							// and iterate until we reach a head whose prev free block is null
							mFreeBlockHead = PrevFreeBlock;
							while (mFreeBlockHead->prevFreeBlock)
							{
								const bool bValid = mFreeBlockHead != mFreeBlockHead->prevFreeBlock;
								HLVM_ASSERT(bValid, TXT("Free block head should not have self dependency"));
								mFreeBlockHead = mFreeBlockHead->prevFreeBlock;
							}
						}
						else
						{
							// Otherwise, assign head to new
							mFreeBlockHead = NewFreeBlock;
							mFreeBlockHead->prevFreeBlock = nullptr;
						}
					}

				// Allocate current free block
				FreeBlock->size = size;
				FreeBlock->free = false;

				// Return actual pointer address
				std::byte* ptr = R_C(std::byte*, FreeBlock) + sizeof(FBlock);
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
		auto ptr = R_C(std::byte*, _ptr);
		if (InStackBound(ptr))
		{
			if (mCtx.bMonolithic)
			{
				return;
			}
			else
			{
				// Reset new block to free
				FBlock* FreeBlock = R_C(FBlock*, ptr - sizeof(FBlock));
				HLVM_ASSERT(FreeBlock->size > 0, TXT("Free block should have non zero size"));
				HLVM_ASSERT(!FreeBlock->free, TXT("Free block double freed"));
				FreeBlock->free = true;
				FreeBlock->prevFreeBlock = nullptr;

				// Exchange new free block with free head
				HLVM_ASSERT(mFreeBlockHead->prevFreeBlock == nullptr, TXT("Free block head shoul not have prev pointer"));
				mFreeBlockHead->prevFreeBlock = FreeBlock;
				FreeBlock->nextFreeBlock = mFreeBlockHead;
				mFreeBlockHead = FreeBlock;

				/**
				 * Check free block validity
				 */
				if (mCtx.bValidate)
				{
					FreeBlock = mFreeBlockHead->nextFreeBlock;
					while (FreeBlock->nextFreeBlock)
					{
						HLVM_ENSURE(FreeBlock->prevFreeBlock != nullptr, TXT("non head free block should have prev pointer"));
						HLVM_ENSURE(FreeBlock->prevFreeBlock != FreeBlock->nextFreeBlock, TXT("free block should not have self dependency"));
						FreeBlock = FreeBlock->nextFreeBlock;
					}
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
		return ptr >= mStack + sizeof(FBlock) && ptr < mStack + N - sizeof(FBlock);
	}

	std::byte					 mStack[N];
	FBlock*						 mFreeBlockHead{ nullptr };
	std::optional<FMiMallocator> mMiMallocator{ std::nullopt }; // Use mimallocator when out of stack
	FStackMallocatorContext		 mCtx;
};
