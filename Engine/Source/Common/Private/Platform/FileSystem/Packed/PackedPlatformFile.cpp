/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#include "Platform/FileSystem/Packed/PackedPlatformFile.h"

DECLARE_LOG_CATEGORY(LogPackedPlatformFile)

static FPackedPlatformFile SPackedPlatformFile{};

void FPackedPlatformFile::_Init()
{
	HLVM_ASSERT(!sPlatformFileRedirector[HLVM_ENUM_V(EPlatformFileType, Packed)], TXT("Packed Platform file is already registered"));
	sPlatformFileRedirector[HLVM_ENUM_V(EPlatformFileType, Packed)] = FPackedPlatformFile::Get();
	HLVM_LOG(LogPackedPlatformFile, debug, TXT("FGenericPlatformFile init FPackedPlatformFile"));
}

FPackedPlatformFile* FPackedPlatformFile::Get()
{
	return &SPackedPlatformFile;
}

bool FPackedPlatformFile::IsDirectory(const FPath&)
{
	LOCK_GUARD_RIVAL(mMountedPackedFileHandlesLock, FRWRivalLock::Group::Read);
	// TODO : actually we should set IsDirectory for packed file to N/A
	return false;
}

bool FPackedPlatformFile::Exists(const FPath&)
{
	LOCK_GUARD_RIVAL(mMountedPackedFileHandlesLock, FRWRivalLock::Group::Read);
	// TODO : iterate all mounted packed file handles
	return true;
}

TSmallVector32<FPath> FPackedPlatformFile::Glob(const FPath&, const FString&, bool)
{
	LOCK_GUARD_RIVAL(mMountedPackedFileHandlesLock, FRWRivalLock::Group::Read);
	// TODO : actually we should set Find for packed file to N/A
	return TSmallVector32<FPath>();
}

bool FPackedPlatformFile::Mount(const FPath& path)
{
	LOCK_GUARD_RIVAL(mMountedPackedFileHandlesLock, FRWRivalLock::Group::Write);
	try
	{
		std::unique_ptr<FPackedFileHandle> packed_file_handle = std::make_unique<FPackedFileHandle>();
		packed_file_handle->Open(path);
		mMountedPackedFileHandles.push_back(MoveTemp(packed_file_handle));
		// Sort by mount order, so that pack with greater mount order will be at the front of the list
		std::sort(mMountedPackedFileHandles.begin(), mMountedPackedFileHandles.end(),
			[](const std::unique_ptr<FPackedFileHandle>& a, const std::unique_ptr<FPackedFileHandle>& b) -> bool {
				return *a > *b;
			});
		HLVM_LOG(LogPackedPlatformFile, info, TXT("Success to mount packed file : %s"), *path);
		return true;
	}
	catch (...)
	{
		HLVM_LOG(LogPackedPlatformFile, err, TXT("Exception happened when mount packed file : %s"), *path);
		return false;
	}
}

bool FPackedPlatformFile::Unmount(const FPath& path)
{
	LOCK_GUARD_RIVAL(mMountedPackedFileHandlesLock, FRWRivalLock::Group::Write);
	try
	{
		if (auto iter = std::ranges::find_if(mMountedPackedFileHandles,
				[&path](const std::unique_ptr<FPackedFileHandle>& packed_file_handle) -> bool {
					return packed_file_handle->mFilePath == path;
				});
			iter != mMountedPackedFileHandles.end())
		{
			auto packed_file_handle = iter->get();
			if (packed_file_handle->mPackedEntryRefCount == 0)
			{
				iter->get()->Close();
				// Remove handle from map, no need to sort as unmount won't change order
				mMountedPackedFileHandles.erase(iter);
				// Clear quick find map
				mPackedEntryQuickFindMap.clear();
				HLVM_LOG(LogPackedPlatformFile, info, TXT("Success to unmount packed file : %s"), *path);
				return true;
			}
			else
			{
				// TODO Spit out more debug info
				HLVM_LOG(LogPackedPlatformFile, warn, TXT("Fail to unmount packed file : %s, because it is still in use"), *path);
				return false;
			}
		}
		else
		{
			HLVM_LOG(LogPackedPlatformFile, err, TXT("Fail to find to be unmount packed file : %s"), *path);
			return false;
		}
	}
	catch (...)
	{
		HLVM_LOG(LogPackedPlatformFile, err, TXT("Exception happened when unmount packed file : %s"), *path);
		return false;
	}
}

FPackedEntryQuickFind FPackedPlatformFile::QuickFindPackedEntry(const FPath& path)
{
	LOCK_GUARD_RIVAL(mMountedPackedFileHandlesLock, FRWRivalLock::Group::Read);
	/**
	 * Quick search map if found
	 */
	{
		ATOMIC_LOCK_GUARD(mPackedEntryQuickFindMapLock);
		HLVM_MAP_FIND(mPackedEntryQuickFindMap, path.GetHash())
		{
			return iter->second;
		}
	}
	/**
	 * Otherwise, search all by mount order (already sorted when mounting, so no need to sort here)
	 */
	for (auto& packed_file_handle : mMountedPackedFileHandles)
	{
		HLVM_MAP_FIND(packed_file_handle->mTokenEntryFragmentMap, path.GetHash())
		{
			auto Data = &(iter->second.Data);
			auto Fragment = &(packed_file_handle->mContainerFragments[iter->second.FragmentID]);
			{
				ATOMIC_LOCK_GUARD(mPackedEntryQuickFindMapLock);
				auto Result = mPackedEntryQuickFindMap.insert_or_assign(path.GetHash(), { .Data = Data, .Fragment = Fragment });
				return Result.first->second;
			}
		}
	}

	HLVM_ENSURE(false, TXT("Fail to find content from packed file : %s"), *path);
	return {};
}
