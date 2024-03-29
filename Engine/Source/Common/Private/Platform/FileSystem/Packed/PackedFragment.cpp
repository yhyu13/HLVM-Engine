/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Platform/FileSystem/Packed/PackedFragment.h"
#include "Core/Assert.h"
#include "Core/Log.h"

DECLARE_LOG_CATEGORY(LogPackedFragment)

/**
 * Suggested Fragment Size (Maybe vary by platform)
 */
size_t FPackedContainerFragment::sSuggestedFragmentSize = 4 * 1024 * 1024;

void FPackedContainerFragment::Open()
{
	using namespace boost::interprocess;
	if (FramgentRefCount.fetch_add(1, std::memory_order_relaxed) == 0)
	{
		/**
		 * Move construct our Region of interest
		 */
		// CAUTION : If throwing is xsi errors on Linux system, switch back mode to read_only instead of read_private
		Region = MoveTemp(mapped_region(*FileMapping, read_private, S_C(offset_t, FragmentStartPos), FragmentSize, nullptr, default_map_options));
	}
}

void FPackedContainerFragment::Close()
{
	// TODO : Consider delay freeing, instead, push freeing to free queue and regularly free in period
	using namespace boost::interprocess;
	if (FramgentRefCount.fetch_sub(1, std::memory_order_relaxed) == 1)
	{
		/**
		 * Swap Region with dummy Region to umap the Region on dummy destruction
		 */
		auto dummy = mapped_region();
		Region.swap(dummy);
	}
}

HLVM_NODISCARD FConstByteBuffer FPackedContainerFragment::GetSubRegion(const FPackedTokenEntryData& Data) const
{
	size_t	   Offset = Data.StartPos - FragmentStartPos;
	const bool bValid = Region.get_size() > 0;
	HLVM_ASSERT(bValid && Data.StartPos >= FragmentStartPos && Offset + Data.Size <= FragmentSize, TXT("Offset out of bounds"));
	return FConstByteBuffer(R_C(const TBYTE*, Region.get_address()) + Offset, Data.Size);
}
