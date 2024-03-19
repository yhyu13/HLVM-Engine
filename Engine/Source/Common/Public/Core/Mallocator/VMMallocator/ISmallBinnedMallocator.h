/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "Core/Mallocator/IMallocator.h"

#ifndef HLVM_SMALL_ALLOC_THRESHOLD
	#define HLVM_SMALL_ALLOC_THRESHOLD 128 // Must be smaller than 256
#endif

#ifndef HLVM_SMALL_ALLOC_ALIGNMENT
	#define HLVM_SMALL_ALLOC_ALIGNMENT 16
#endif

struct FSmallBinnedBlockHead
{
	// Use this FSmallBinnedBlockHead* - Pos * Alignment to get FBlocks32's P pointer location
	uint8_t Pos;
	uint8_t Alignment;

	bool Valid() const
	{
		return Pos <= 32 && Alignment <= HLVM_SMALL_ALLOC_THRESHOLD && Alignment > 0;
	}
	HLVM_STATIC_FUNC uint8_t PotentiallyOwned(void* v)
	{
		FSmallBinnedBlockHead* BlockHead = R_C(FSmallBinnedBlockHead*, R_C(TBYTE*, v) - sizeof(FSmallBinnedBlockHead));
		if (BlockHead->Valid())
		{
			return BlockHead->Alignment;
		}
		return 0;
	}
	HLVM_STATIC_FUNC uint8_t GoodSize(size_t _size)
	{
		uint8_t size = S_C(uint8_t, _size);
		// Round to next 16 multiplier
		uint8_t remainder = size % HLVM_SMALL_ALLOC_ALIGNMENT;
		if (remainder == 0)
		{
			return size;
		}
		else
		{
			return size + (HLVM_SMALL_ALLOC_ALIGNMENT - remainder);
		}
	}
};
static_assert(sizeof(FSmallBinnedBlockHead) == 2);

class FVMArena;
/**
 * Inspired by mimalloc small allocation,
 * We implement a binnned mallocator that is designed for small allocation under 255 bytes
 * With 32 items in each bin, and bins chains together
 *
 * Call FSmallBinnedBlockHead::GoodeSize to determine a good Alignment size for small allocation
 */
class ISmallBinnedMallocator
{
public:
	NOCOPYMOVE(ISmallBinnedMallocator)
	ISmallBinnedMallocator() = default;
	virtual ~ISmallBinnedMallocator() noexcept = default;
	virtual void  Init(FVMArena* _Mallocator) = 0;
	virtual void* Malloc() = 0;
	virtual void  Free(void* p) noexcept = 0;
};
