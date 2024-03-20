/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "PackedEntryHandle.h"
#include "Core/Container/ContainerDefinition.h"
#include "Core/Parallel/Lock.h"
#include "Platform/GenericPlatformFile.h"

class FPackedPlatformFile final : public FGenericPlatformFile
{
public:
	NOCOPYMOVE(FPackedPlatformFile)
	FPackedPlatformFile() = default;
	virtual ~FPackedPlatformFile() final override = default;

	/**
	 * Called inside FGenericPlatformFile::Get()
	 */
	static void					_Init();
	static FPackedPlatformFile* Get();

	virtual bool				  IsDirectory(const FPath& path) final override;
	virtual bool				  Exists(const FPath& path) final override;
	virtual TSmallVector32<FPath> Glob(const FPath& root_dir, const FString& regex, bool recursive = false) final override;

	bool				  Mount(const FPath& path);
	bool				  Unmount(const FPath& path);
	FPackedEntryQuickFind QuickFindPackedEntry(const FPath& path);

private:
	TMap<FPathHash, FPackedEntryQuickFind> mPackedEntryQuickFindMap{};
	FAtomicFlagNC						   mPackedEntryQuickFindMapLock;

	TVector<std::unique_ptr<FPackedFileHandle>> mMountedPackedFileHandles{};
	FRWRivalLock								mMountedPackedFileHandlesLock;
};
