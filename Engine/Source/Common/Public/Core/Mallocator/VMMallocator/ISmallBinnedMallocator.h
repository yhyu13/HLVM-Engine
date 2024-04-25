/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "Core/Mallocator/IMallocator.h"
#include "VMMallocatorDefinition.h"

struct FSmallBinnedBlockHead
{
	// Use this FSmallBinnedBlockHead* - Pos * Alignment to get FBlocks32's P pointer location
	TUINT8 Alignment;
	TUINT8 Pos;

	bool Valid() const
	{
		return Pos < 32 && Alignment <= HLVM_VMA_SMALL_ALLOC_THRESHOLD && Alignment > 0;
	}
	HLVM_STATIC_FUNC TUINT8 IsSmallAlloc(void* v)
	{
		FSmallBinnedBlockHead* BlockHead = R_C(FSmallBinnedBlockHead*, R_C(TBYTE*, v) - sizeof(FSmallBinnedBlockHead));
		if (BlockHead->Valid())
		{
			return BlockHead->Alignment;
		}
		return 0;
	}
	HLVM_STATIC_FUNC TUINT8 GoodSize(size_t size)
	{
		return S_C(TUINT8, AlignUp(size, HLVM_VMA_SMALL_ALLOC_ALIGNMENT));
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
