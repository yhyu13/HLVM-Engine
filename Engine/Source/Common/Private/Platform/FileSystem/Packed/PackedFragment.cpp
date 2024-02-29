/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Platform/FileSystem/Packed/PackedFragment.h"
#include "Core/Assert.h"

/**
 * Suggested Fragment Size (Maybe vary by platform)
 */
size_t FPackedContainerFragment::sSuggestedFragmentSize = 4 * 1024 * 1024;

void FPackedContainerFragment::Open(const boost::interprocess::file_mapping& fm)
{
	using namespace boost::interprocess;
	/**
	 * Move construct our Region of interest
	 */
	Region = MoveTemp(mapped_region(fm, read_only, S_C(offset_t, FragmentStartPos), FragmentSize, nullptr, default_map_options));
}

void FPackedContainerFragment::Close()
{
	using namespace boost::interprocess;
	/**
	 * Swap Region with dummy Region to umap the Region on dummy destruction
	 */
	auto dummy = mapped_region();
	Region.swap(dummy);
}

std::span<const std::byte> FPackedContainerFragment::GetSubRegion(const FPackedTokenEntryData& Data) const
{
	size_t Offset = Data.StartPos - FragmentStartPos;
	HLVM_ASSERT(Data.StartPos >= FragmentStartPos && Offset + Data.Size <= FragmentSize, TXT("Offset out of bounds"));
	return std::span<const std::byte>(R_C(const std::byte*, Region.get_address()) + Offset, Data.Size);
}
