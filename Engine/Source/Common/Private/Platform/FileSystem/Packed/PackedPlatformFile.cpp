/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#include "Platform/FileSystem/Packed/PackedPlatformFile.h"

DECLARE_LOG_CATEGORY(LogPackedPlatformFile)

void FPackedPlatformFile::InternalInit()
{
	HLVM_ASSERT_F(!sPlatformFileRedirector[E2VALUE(EPlatformFileType::Packed)], TXT("Packed Platform file is already registered"));
	sPlatformFileRedirector[E2VALUE(EPlatformFileType::Packed)] = FPackedPlatformFile::Get();
	HLVM_LOG(LogPackedPlatformFile, debug, TXT("Init FPackedPlatformFile"));
}

TNoNullablePtr<FPackedPlatformFile> FPackedPlatformFile::Get()
{
	static FPackedPlatformFile SPackedPlatformFile{};
	return &SPackedPlatformFile;
}

bool FPackedPlatformFile::IsDirectory(const FPath&)
{
	//	LOCK_GUARD_RW(mMountedPackedFileHandlesLock, FRWLock::Group::Read);
	//	Packed file does not contain directory
	HLVM_LOG(LogPackedPlatformFile, warn, TXT("Packed file does not contain directory"));
	return false;
}

bool FPackedPlatformFile::Exists(const FPath& path)
{
	LOCK_GUARD_RW(mMountedPackedFileHandlesLock, FRWLock::Group::Read);
	for (auto& packed_file_handle : mMountedPackedFileHandles)
	{
		if (packed_file_handle->mTokenEntryFragmentMap.find(path.GetHash()) != packed_file_handle->mTokenEntryFragmentMap.end())
		{
			return true;
		}
	}
	return false;
}

TSmallVector32<FPath> FPackedPlatformFile::Glob(const FPath&, const FString&, bool)
{
	// LOCK_GUARD_RW(mMountedPackedFileHandlesLock, FRWLock::Group::Read);
	HLVM_LOG(LogPackedPlatformFile, warn, TXT("Packed file does not support glob"));
	return TSmallVector32<FPath>();
}

bool FPackedPlatformFile::Mount(const FPath& path)
{
	LOCK_GUARD_RW(mMountedPackedFileHandlesLock, FRWLock::Group::Write);
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
		HLVM_LOG(LogPackedPlatformFile, info, TXT("Success to mount packed file : {}"), *path);
		return true;
	}
	catch (...)
	{
		HLVM_LOG(LogPackedPlatformFile, err, TXT("Exception happened when mount packed file : {}"), *path);
		return false;
	}
}

bool FPackedPlatformFile::Unmount(const FPath& path)
{
	LOCK_GUARD_RW(mMountedPackedFileHandlesLock, FRWLock::Group::Write);
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
				HLVM_LOG(LogPackedPlatformFile, info, TXT("Success to unmount packed file : {}"), *path);
				return true;
			}
			else
			{
				HLVM_LOG(LogPackedPlatformFile, warn, TXT("Fail to unmount packed file : {}, because it is still in use by {} opened pak entries"),
					*path, packed_file_handle->mPackedEntryRefCount.load());
				return false;
			}
		}
		else
		{
			HLVM_LOG(LogPackedPlatformFile, err, TXT("Fail to find to be unmount packed file handle : {}"), *path);
			return false;
		}
	}
	catch (...)
	{
		HLVM_LOG(LogPackedPlatformFile, err, TXT("Exception happened when unmount packed file : {}"), *path);
		return false;
	}
}

FPackedEntryQuickFind FPackedPlatformFile::QuickFindPackedEntry(const FPath& path)
{
	LOCK_GUARD_RW(mMountedPackedFileHandlesLock, FRWLock::Group::Read);
	/**
	 * Quick search map if found
	 */
	{
		ATOMIC_LOCK_GUARD(mPackedEntryQuickFindMapLock);
		if (auto iter = mPackedEntryQuickFindMap.find(path.GetHash());
			iter != mPackedEntryQuickFindMap.end())
		{
			return iter->second;
		}
	}
	/**
	 * Otherwise, search all by mount order (already sorted when mounting, so no need to sort here)
	 */
	for (auto& packed_file_handle : mMountedPackedFileHandles)
	{
		if (auto iter = packed_file_handle->mTokenEntryFragmentMap.find(path.GetHash());
			iter != packed_file_handle->mTokenEntryFragmentMap.end())
		{
			auto Data = &(iter->second.Data);
			auto RefCount = &(packed_file_handle->mPackedEntryRefCount);
			auto Fragment = &(packed_file_handle->mContainerFragments[iter->second.FragmentID]);
			{
				ATOMIC_LOCK_GUARD(mPackedEntryQuickFindMapLock);
				auto Result = mPackedEntryQuickFindMap.insert_or_assign(path.GetHash(),
					{ .Data = Data, .Fragment = Fragment, .RefCount = RefCount });
				return Result.first->second;
			}
		}
	}

	HLVM_ENSURE_F(false, TXT("Fail to find content from packed file : {}"), *path);
	return {};
}

bool FPackedPlatformFile::SaveAsString(const FPath& /*path*/, const FString& /*content*/)
{
	// TODO
	HLVM_NOT_IMPLEMENTED();
	return false;
}

bool FPackedPlatformFile::SaveAsStringArray(const FPath& /*path*/, const TVector<FString>& /*Result*/, const FString& /*linechanger*/)
{
	// TODO
	HLVM_NOT_IMPLEMENTED();
	return false;
}

bool FPackedPlatformFile::SaveAsByteArray(const FPath& /*path*/, const TVector<TBYTE>& /*content*/)
{
	// TODO
	HLVM_NOT_IMPLEMENTED();
	return false;
}

FString FPackedPlatformFile::LoadAsString(const FPath& /*path*/)
{
	// TODO
	HLVM_NOT_IMPLEMENTED();
	return FString();
}

TVector<FString> FPackedPlatformFile::LoadAsStringArray(const FPath& /*path*/, const TVector<FString>& /*delimiters*/)
{
	// TODO
	HLVM_NOT_IMPLEMENTED();
	return TVector<FString>();
}

TVector<TBYTE> FPackedPlatformFile::LoadAsByteArray(const FPath& /*path*/)
{
	// TODO
	HLVM_NOT_IMPLEMENTED();
	return TVector<TBYTE>();
}

bool FPackedPlatformFile::DeleteFile(const FPath& /*path*/)
{
	// Pack file should not call delete
	HLVM_ASSERT(false);
	return false;
}
