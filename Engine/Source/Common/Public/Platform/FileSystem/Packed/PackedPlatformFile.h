/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#pragma once

#include "PackedEntryHandle.h"
#include "Core/Container/ContainerDefinition.h"
#include "Core/Parallel/Lock.h"
#include "Platform/GenericPlatformFile.h"

class FPackedPlatformFile final : public FGenericPlatformFile
{
public:
	NOCOPYMOVE(FPackedPlatformFile);

	static TNoNullablePtr<FPackedPlatformFile> Get();

	virtual bool				  IsDirectory(const FPath& path) final override;
	virtual bool				  Exists(const FPath& path) final override;
	virtual TSmallVector32<FPath> Glob(const FPath& root_dir, const FString& regex, bool recursive = false) final override;

	virtual bool				  SaveAsString(const FPath& path, const FString& content) final override;
	virtual bool				  SaveAsStringArray(const FPath& path, const TVector<FString>& Result, const FString& linechanger = TXT("\n")) final override;
	virtual bool				  SaveAsByteArray(const FPath& path, const TVector<TBYTE>& content) final override;

	virtual FString				  LoadAsString(const FPath& path) final override;
	virtual TVector<FString>	  LoadAsStringArray(const FPath& path, const TVector<FString>& delimiters = {TXT("\n"), TXT("\r\n")}) final override;
	virtual TVector<TBYTE>		  LoadAsByteArray(const FPath& path) final override;

	virtual bool				  DeleteFile(const FPath& path) final override;

	bool				  Mount(const FPath& path);
	bool				  Unmount(const FPath& path);
	FPackedEntryQuickFind QuickFindPackedEntry(const FPath& path);

private:
	friend class FGenericPlatformFile;

	FPackedPlatformFile() = default;

	/**
	 * Called inside FGenericPlatformFile::Get()
	 * Intentionally internal
	 */
	static void InternalInit();

private:
	TMap<FPathHash, FPackedEntryQuickFind> mPackedEntryQuickFindMap{};
	FAtomicFlagNC						   mPackedEntryQuickFindMapLock;

	TVector<std::unique_ptr<FPackedFileHandle>> mMountedPackedFileHandles{};
	FRWLock								mMountedPackedFileHandlesLock;
};
