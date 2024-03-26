/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "Platform/FileSystem/FileHandle.h"
#include "Core/Log.h"

#include <magic_enum_all.hpp>

DECLARE_LOG_CATEGORY(LogIFileHandle)


void IFileHandle::HandleException(const OpStatusType& Status_InOut, const TCHAR* Function, const std::exception& Exception)
{
	FString Msg = FString::Format(TXT("File {}: calling '{}' return {} with errorNo {} and exception {}"),
		*mFilePath,
		Function,
		HLVM_ENUM_TCHAR_STR(Status_InOut->eFileOpStatus),
		HLVM_ENUM_TCHAR_STR(Status_InOut->eFileOpErrorNo),
		TO_TCHAR_STR(Exception.what()));
	if (!Status_InOut->bCancelByUser)
	{
		HLVM_LOG(LogIFileHandle, err, MoveTemp(Msg));
		if (!Status_InOut->bSupressFailExceptions)
		{
			throw Exception;
		}
	}
	else
	{
		HLVM_LOG(LogIFileHandle, warn, TXT("{} but canceled by user, so we continue."), MoveTemp(Msg));
	}
}

void IFileHandle::HandleException2(const OpStatusType& Status_InOut, const TCHAR* Function)
{
	FString Msg = FString::Format(TXT("File {}: calling '{}' return {} with errorNo {}"),
		*mFilePath,
		Function,
		HLVM_ENUM_TCHAR_STR(Status_InOut->eFileOpStatus),
		HLVM_ENUM_TCHAR_STR(Status_InOut->eFileOpErrorNo));
	if (!Status_InOut->bCancelByUser)
	{
		HLVM_LOG(LogIFileHandle, err, MoveTemp(Msg));
		if (!Status_InOut->bSupressFailExceptions)
		{
			// 重新抛出了当前正在处理的异常
			throw;
		}
	}
	else
	{
		HLVM_LOG(LogIFileHandle, warn, TXT("{} but canceled by user, so we continue."), MoveTemp(Msg));
	}
}
