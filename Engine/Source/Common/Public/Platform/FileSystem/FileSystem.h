/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#pragma once

#include "FileHandle.h"

// Helper class for read writ files
class FFileSystem
{
public:

	HLVM_STATIC_FUNC bool LoadFileToArray(TArray<TBYTE>& buffer, const FString& InPath);
	HLVM_STATIC_FUNC bool SaveArrayToFile(const TArray<TBYTE>& buffer, const FString& InPath);

	HLVM_STATIC_FUNC bool SaveStringToFile(const FString& InString, const FString& InPath);
	HLVM_STATIC_FUNC FString LoadFileToString(const FString& InPath);

	HLVM_STATIC_FUNC bool AppendStringToFile(const FString& InString, const FString& InPath);

	HLVM_STATIC_FUNC TSharedPtr<IFileHandle> OpenForRead(const FString& InPath);
	HLVM_STATIC_FUNC TSharedPtr<IFileHandle> OpenForWrite(const FString& InPath);
	HLVM_STATIC_FUNC TSharedPtr<IFileHandle> CreateUniqueTempFile();

	HLVM_STATIC_FUNC bool DeleteFile(const FString& InPath);
};
