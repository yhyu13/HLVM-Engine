/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#include "Platform/FileSystem/FileSystem.h"
#include "Platform/FileSystem/Boost/BoostPlatformFile.h"
#include "Platform/FileSystem/Path.h"
#include "Core/Assert.h"
#include "Definition/ClassDefinition.h"

DECLARE_LOG_CATEGORY(LogFileSystem)

bool FFileSystem::LoadFileToArray(TArray<TBYTE>& buffer, const FString& InPath)
{
	FPath path(InPath);
	TVector<TBYTE> result = FBoostPlatformFile::Get()->LoadAsByteArray(path);
	if (result.empty())
	{
		HLVM_LOG(LogFileSystem, warn, TXT("FFileSystem::LoadFileToArray : Failed to load file {}"), *InPath);
		return false;
	}
	buffer = MoveTemp(result);
	return true;
}

bool FFileSystem::SaveArrayToFile(const TArray<TBYTE>& buffer, const FString& InPath)
{
	FPath path(InPath);
	bool result = FBoostPlatformFile::Get()->SaveAsByteArray(path, buffer);
	if (!result)
	{
		HLVM_LOG(LogFileSystem, err, TXT("FFileSystem::SaveArrayToFile : Failed to save file {}"), *InPath);
	}
	return result;
}

bool FFileSystem::SaveStringToFile(const FString& InString, const FString& InPath)
{
	FPath path(InPath);
	bool result = FBoostPlatformFile::Get()->SaveAsString(path, InString);
	if (!result)
	{
		HLVM_LOG(LogFileSystem, err, TXT("FFileSystem::SaveStringToFile : Failed to save file {}"), *InPath);
	}
	return result;
}

FString FFileSystem::LoadFileToString(const FString& InPath)
{
	FPath path(InPath);
	FString result = FBoostPlatformFile::Get()->LoadAsString(path);
	if (result.empty())
	{
		HLVM_LOG(LogFileSystem, warn, TXT("FFileSystem::LoadFileToString : Failed to load file {} or file is empty"), *InPath);
	}
	return result;
}

bool FFileSystem::AppendStringToFile(const FString& InString, const FString& InPath)
{
	// Load existing content
	FString existing = LoadFileToString(InPath);
	if (existing.empty() && !FPath::Exists(InPath))
	{
		// File doesn't exist, just save
		return SaveStringToFile(InString, InPath);
	}
	
	// Append and save
	existing += InString;
	return SaveStringToFile(existing, InPath);
}

TSharedPtr<IFileHandle> FFileSystem::OpenForRead(const FString& InPath)
{
	FPath path(InPath);
	auto fileHandle = MAKE_SHARED(FBoostMapFileHandle);
	fileHandle->Open(path, GReadOnlyFileOptions);
	
	if (!fileHandle->IsOpen())
	{
		HLVM_LOG(LogFileSystem, err, TXT("FFileSystem::OpenForRead : Failed to open file {} for reading"), *InPath);
		return nullptr;
	}
	
	return fileHandle;
}

TSharedPtr<IFileHandle> FFileSystem::OpenForWrite(const FString& InPath)
{
	FPath path(InPath);
	auto fileHandle = MAKE_SHARED(FBoostMapFileHandle);
	fileHandle->Open(path, GWriteOnlyFileOptions);
	
	if (!fileHandle->IsOpen())
	{
		HLVM_LOG(LogFileSystem, err, TXT("FFileSystem::OpenForWrite : Failed to open file {} for writing"), *InPath);
		return nullptr;
	}
	
	return fileHandle;
}

TSharedPtr<IFileHandle> FFileSystem::CreateUniqueTempFile()
{
	// Generate unique temp file path using atomic counter
	static std::atomic<uint64_t> counter{ 0 };
	uint64_t id = counter.fetch_add(1);
	
	FString tempPath = FString::Format(TXT("./.tmp/hlvm_{}"), id);
	
	FPath path(tempPath);
	auto fileHandle = MAKE_SHARED(FBoostMapFileHandle);
	fileHandle->Open(path, GWriteOnlyFileOptions);
	
	if (!fileHandle->IsOpen())
	{
		HLVM_LOG(LogFileSystem, err, TXT("FFileSystem::CreateUniqueTempFile : Failed to create temp file {}"), *tempPath);
		return nullptr;
	}
	
	return fileHandle;
}

bool FFileSystem::DeleteFile(const FString& InPath)
{
	FPath path(InPath);
	bool result = FBoostPlatformFile::Get()->DeleteFile(path);
	if (!result)
	{
		HLVM_LOG(LogFileSystem, err, TXT("FFileSystem::DeleteFile : Failed to delete file {}"), *InPath);
	}
	return result;
}
