/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#include "Platform/FileSystem/Boost/BoostStreamFileHandle.h"
#include "Platform/FileSystem/Boost/BoostFileStat.h"
#include "Core/Log.h"

#include <boost/interprocess/sync/sharable_lock.hpp>
#include <magic_enum_all.hpp>

DECLARE_LOG_CATEGORY(LogBoostStreamFileHandle)

#define BSFH_RECRSIVE_LOCK() ATOMIC_LOCK_GUARD(mRecursiveLock)

#define BSFH_SCOPE_LOCK()                                                           \
	boost::interprocess::sharable_lock<boost::interprocess::file_lock> __lock_file; \
	if (mFileLock)                                                                  \
	__lock_file = MoveTemp(boost::interprocess::sharable_lock<boost::interprocess::file_lock>(*mFileLock))

#define BSFH_HANDLE_EXCPETIONS() HandleException(Status_InOut, TO_TCHAR_CSTR(__FUNCTION__), Exception)
#define BSFH_HANDLE_EXCPETIONS2() HandleException2(Status_InOut, TO_TCHAR_CSTR(__FUNCTION__))

#define BSFH_HANDLE_ASSERT(x, ...) HLVM_ASSERT_F(x, TXT("File {} : {}"), *mFilePath, FString::Format(__VA_ARGS__))
#define BSFH_HANDLE_ENSURE(x, ...) HLVM_ENSURE_F(x, TXT("File {} : {}"), *mFilePath, FString::Format(__VA_ARGS__))
#define BSFH_HANDLE_ENSURE2(x, ...) HLVM_ENSURE_F(x, TXT("File {} : {}"), *FilePath, FString::Format(__VA_ARGS__))
#define BSFH_VERBOSE_LOG(...)                                                                                         \
	do                                                                                                                \
	{                                                                                                                 \
		if (Status_InOut->bVerbose)                                                                                   \
			HLVM_LOG(LogBoostStreamFileHandle, trace, TXT("File {} : {}"), *mFilePath, FString::Format(__VA_ARGS__)); \
	}                                                                                                                 \
	while (0)
#define BSFH_VERBOSE_LOG2(...)                                                                                       \
	do                                                                                                               \
	{                                                                                                                \
		if (Status_InOut->bVerbose)                                                                                  \
			HLVM_LOG(LogBoostStreamFileHandle, trace, TXT("File {} : {}"), *FilePath, FString::Format(__VA_ARGS__)); \
	}                                                                                                                \
	while (0)

#define BSFH_HANDLE_STATUS(Status) OpStatusType Status = &FileOpStatus

FBoostStreamFileHandle::~FBoostStreamFileHandle()
{
	if (mOpened)
	{
		Close();
	}
}

IFileHandle::OpRetType FBoostStreamFileHandle::Open(const FPath& FilePath, const FFileOptions& Options)
{
	BSFH_HANDLE_STATUS(Status_InOut);
	BSFH_HANDLE_ENSURE(*Status_InOut, TXT("File operation continue with failed status"));
	BSFH_HANDLE_ASSERT(!mOpened, TXT("File operation begin with another already open file"));

	mFilePath = FilePath;
	mFileOptions = Options;
	mFileOptions.eFileMapped = EFileMapped::NoMapped;
	{
		BSFH_VERBOSE_LOG(TXT("Create file stream"));
		BSFH_HANDLE_ASSERT(!mFStream.is_open(), TXT("File operation begin with another already open file"));
	}

	if (mFileOptions.eFileAsync == EFileAsync::Async)
	{
		HLVM_NOT_IMPLEMENTED();
	}

	if (mFileOptions.eFileLock & EFileLock::ThreadLock)
	{
		BSFH_VERBOSE_LOG(TXT("Create thread lock"));
		mRecursiveLock = FRecursiveAtomicFlag();
	}

	try
	{
		{
			BSFH_HANDLE_ASSERT(mFStream, TXT("FStream file null"));
			mFStream.open(FilePath, static_cast<std::ios::openmode>(mFileOptions.eFileMode));
			BSFH_HANDLE_ENSURE(mFStream.is_open(), TXT("FStream open failed"));
		}

		/**
		 * File lock can only init after success of open an existing file
		 */
		if (mFileOptions.eFileLock & EFileLock::InterProcessLock)
		{
			BSFH_VERBOSE_LOG(TXT("Create interprocess file lock"));
			mFileLock = new boost::interprocess::file_lock(FilePath);
		}

		mOpened = true;
		Status_InOut->eFileOpStatus = EFileOpStatus::Success;
		BSFH_VERBOSE_LOG(TXT("Open success with mode {}"), HLVM_ENUM_TCHAR_STR(mFileOptions.eFileMode));
	}
	catch (std::exception& Exception)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BSFH_HANDLE_EXCPETIONS();
	}
	catch (...)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BSFH_HANDLE_EXCPETIONS2();
	}

	return *this;
}

IFileHandle::OpRetType FBoostStreamFileHandle::Close()
{
	BSFH_HANDLE_STATUS(Status_InOut);
	BSFH_HANDLE_ENSURE(*Status_InOut, TXT("File operation continue with failed status"));
	BSFH_HANDLE_ASSERT(mOpened, TXT("File operation continue w/o open"));

	try
	{
		BSFH_SCOPE_LOCK();

		{
			mFStream.close();
			BSFH_HANDLE_ENSURE(!mFStream.fail(), TXT("File operation failed"));
		}

		{
			HLVM_DELETE(mFileLock);
			mRecursiveLock.reset();
		}

		if (Status_InOut->bCancelByUser)
			HLVM_UNLIKELY
			{
				Status_InOut->eFileOpStatus = EFileOpStatus::Canceled;
			}
		else
			HLVM_LIKELY
			{
				Status_InOut->eFileOpStatus = EFileOpStatus::Success;
			}
		mOpened = false;
		BSFH_VERBOSE_LOG(TXT("Close file success"));
	}
	catch (std::exception& Exception)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BSFH_HANDLE_EXCPETIONS();
	}
	catch (...)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BSFH_HANDLE_EXCPETIONS2();
	}

	return *this;
}

IFileHandle::OpRetType FBoostStreamFileHandle::Read(void* Buffer, size_t Size, const FFileSeekCtx& SeekCtx)
{
	BSFH_HANDLE_STATUS(Status_InOut);
	BSFH_HANDLE_ENSURE(*Status_InOut, TXT("File operation continue with failed status"));
	BSFH_HANDLE_ASSERT(mOpened, TXT("File operation continue w/o open"));
	BSFH_HANDLE_ASSERT(mFileOptions.eFileMode & EFileMode::R, TXT("File operation cannot read"));
	BSFH_HANDLE_ASSERT(Size > 0, TXT("Buffer size invalid {}"), Size);
	BSFH_RECRSIVE_LOCK();

	// tell if necessary
	int64_t Prev_Tell = { -1 };
	if (SeekCtx.bResetPos)
	{
		Tell(Prev_Tell);
		BSFH_HANDLE_ASSERT(Prev_Tell > 0, TXT("Tell failed before reset pos"), Prev_Tell);
	}
	// Seek if necessary
	if (SeekCtx.NonTrivialSeek())
	{
		Seek(SeekCtx.Offset, SeekCtx.Whence);
	}

	try
	{
		BSFH_SCOPE_LOCK();
		{
			mFStream.read(reinterpret_cast<char*>(Buffer), static_cast<std::streamsize>(Size));
			BSFH_HANDLE_ENSURE(!mFStream.fail(), TXT("File operation failed"));
		}

		if (Status_InOut->bCancelByUser)
			HLVM_UNLIKELY
			{
				Status_InOut->eFileOpStatus = EFileOpStatus::Canceled;
			}
		else
			HLVM_LIKELY
			{
				Status_InOut->eFileOpStatus = EFileOpStatus::Success;
			}
		BSFH_VERBOSE_LOG(TXT("Read file success with {} bytes"), Size);
	}
	catch (std::exception& Exception)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BSFH_HANDLE_EXCPETIONS();
	}
	catch (...)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BSFH_HANDLE_EXCPETIONS2();
	}

	// Reset if necessary
	if (SeekCtx.bResetPos)
		HLVM_UNLIKELY
		{
			BSFH_VERBOSE_LOG(TXT("bResetPos after read"));
			Seek(Prev_Tell, EWhence::Begin);
		}
	// Erase if necessary
	else if (SeekCtx.bEraseSeekPos)
	{
		BSFH_VERBOSE_LOG(TXT("bEraseSeekPos after read"));
		Seek(0 - static_cast<int64_t>(Size), EWhence::Current);
	}

	return *this;
}

IFileHandle::OpRetType FBoostStreamFileHandle::Write(const void* Buffer, size_t Size, const FFileSeekCtx& SeekCtx)
{
	BSFH_HANDLE_STATUS(Status_InOut);
	BSFH_HANDLE_ENSURE(*Status_InOut, TXT("File operation continue with failed status"));
	BSFH_HANDLE_ASSERT(mOpened, TXT("File operation continue w/o open"));
	BSFH_HANDLE_ASSERT(mFileOptions.eFileMode & EFileMode::W, TXT("File operation cannot write"));
	BSFH_HANDLE_ASSERT(Size > 0, TXT("Buffer size invalid {}"), Size);
	BSFH_RECRSIVE_LOCK();

	// tell if necessary
	int64_t Prev_Tell{ -1 };
	if (SeekCtx.bResetPos)
	{
		Tell(Prev_Tell);
		BSFH_HANDLE_ASSERT(Prev_Tell > 0, TXT("Tell failed before reset pos"), Prev_Tell);
	}
	// Seek if necessary
	if (SeekCtx.NonTrivialSeek())
	{
		Seek(SeekCtx.Offset, SeekCtx.Whence);
	}

	try
	{
		BSFH_SCOPE_LOCK();
		{
			mFStream.write(reinterpret_cast<const char*>(Buffer), static_cast<std::streamsize>(Size));
			BSFH_HANDLE_ENSURE(!mFStream.fail(), TXT("File operation failed"));
		}

		if (Status_InOut->bCancelByUser)
			HLVM_UNLIKELY
			{
				Status_InOut->eFileOpStatus = EFileOpStatus::Canceled;
			}
		else
			HLVM_LIKELY
			{
				Status_InOut->eFileOpStatus = EFileOpStatus::Success;
			}
		BSFH_VERBOSE_LOG(TXT("Write file success with {} bytes"), Size);
	}
	catch (std::exception& Exception)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BSFH_HANDLE_EXCPETIONS();
	}
	catch (...)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BSFH_HANDLE_EXCPETIONS2();
	}

	// Reset if necessary
	if (SeekCtx.bResetPos)
	{
		BSFH_VERBOSE_LOG(TXT("bResetPos after write"));
		Seek(Prev_Tell, EWhence::Begin);
	}
	// Erase if necessary
	else if (SeekCtx.bEraseSeekPos)
	{
		BSFH_VERBOSE_LOG(TXT("bEraseSeekPos after write"));
		Seek(0 - static_cast<int64_t>(Size), EWhence::Current);
	}

	return *this;
}

IFileHandle::OpRetType FBoostStreamFileHandle::Flush()
{
	BSFH_HANDLE_STATUS(Status_InOut);
	BSFH_HANDLE_ENSURE(*Status_InOut, TXT("File operation continue with failed status"));
	BSFH_HANDLE_ASSERT(mOpened, TXT("File operation continue w/o open"));
	BSFH_HANDLE_ASSERT(mFileOptions.eFileMode & EFileMode::W, TXT("File operation cannot flush"));
	BSFH_RECRSIVE_LOCK();

	try
	{
		BSFH_SCOPE_LOCK();
		{
			mFStream.flush();
			BSFH_HANDLE_ENSURE(!mFStream.fail(), TXT("File operation failed"));
		}

		if (Status_InOut->bCancelByUser)
			HLVM_UNLIKELY
			{
				Status_InOut->eFileOpStatus = EFileOpStatus::Canceled;
			}
		else
			HLVM_LIKELY
			{
				Status_InOut->eFileOpStatus = EFileOpStatus::Success;
			}
		BSFH_VERBOSE_LOG(TXT("Flush file"));
	}
	catch (std::exception& Exception)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BSFH_HANDLE_EXCPETIONS();
	}
	catch (...)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BSFH_HANDLE_EXCPETIONS2();
	}

	return *this;
}

IFileHandle::OpRetType FBoostStreamFileHandle::Seek(int64_t Offset, EWhence Whence)
{
	BSFH_HANDLE_STATUS(Status_InOut);
	BSFH_HANDLE_ENSURE(*Status_InOut, TXT("File operation continue with failed status"));
	BSFH_HANDLE_ASSERT(mOpened, TXT("File operation continue w/o open"));
	BSFH_RECRSIVE_LOCK();

	try
	{
		BSFH_SCOPE_LOCK();
		{
			mFStream.seekg(static_cast<std::streamoff>(Offset), static_cast<std::ios::seekdir>(Whence));
			BSFH_HANDLE_ENSURE(!mFStream.fail(), TXT("File operation failed"));
		}

		if (Status_InOut->bCancelByUser)
			HLVM_UNLIKELY
			{
				Status_InOut->eFileOpStatus = EFileOpStatus::Canceled;
			}
		else
			HLVM_LIKELY
			{
				Status_InOut->eFileOpStatus = EFileOpStatus::Success;
			}
		BSFH_VERBOSE_LOG(TXT("Seek success given offset {} with {}"), Offset, HLVM_ENUM_TCHAR_STR(Whence));
	}
	catch (std::exception& Exception)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BSFH_HANDLE_EXCPETIONS();
	}
	catch (...)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BSFH_HANDLE_EXCPETIONS2();
	}

	return *this;
}

IFileHandle::IFileHandle::OpRetType FBoostStreamFileHandle::Tell(int64_t& Offset)
{
	BSFH_HANDLE_STATUS(Status_InOut);
	BSFH_HANDLE_ENSURE(*Status_InOut, TXT("File operation continue with failed status"));
	BSFH_HANDLE_ASSERT(mOpened, TXT("File operation continue w/o open"));
	BSFH_RECRSIVE_LOCK();

	try
	{
		BSFH_SCOPE_LOCK();
		{
			Offset = static_cast<int64_t>(mFStream.tellg());
			BSFH_HANDLE_ENSURE(Offset >= 0, TXT("File Tell failed with value {}"), Offset);
			BSFH_HANDLE_ENSURE(!mFStream.fail(), TXT("File operation failed"));
		}

		if (Status_InOut->bCancelByUser)
			HLVM_UNLIKELY
			{
				Status_InOut->eFileOpStatus = EFileOpStatus::Canceled;
			}
		else
			HLVM_LIKELY
			{
				Status_InOut->eFileOpStatus = EFileOpStatus::Success;
			}
		BSFH_VERBOSE_LOG(TXT("Tell success with offset {}"), Offset);
	}
	catch (std::exception& Exception)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BSFH_HANDLE_EXCPETIONS();
	}
	catch (...)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BSFH_HANDLE_EXCPETIONS2();
	}

	return *this;
}

IFileHandle::OpRetType FBoostStreamFileHandle::Size(size_t& Size)
{
	BSFH_HANDLE_STATUS(Status_InOut);
	BSFH_HANDLE_ENSURE(*Status_InOut, TXT("File operation continue with failed status"));
	BSFH_RECRSIVE_LOCK();

	try
	{
		BSFH_SCOPE_LOCK();
		{
			auto curPos = mFStream.tellg();
			mFStream.seekg(0, static_cast<std::ios::seekdir>(EWhence::End));
			Size = static_cast<size_t>(mFStream.tellg());
			mFStream.seekg(curPos, static_cast<std::ios::seekdir>(EWhence::Begin));
			BSFH_HANDLE_ENSURE(!mFStream.fail(), TXT("File operation failed"));
		}

		if (Status_InOut->bCancelByUser)
			HLVM_UNLIKELY
			{
				Status_InOut->eFileOpStatus = EFileOpStatus::Canceled;
			}
		else
			HLVM_LIKELY
			{
				Status_InOut->eFileOpStatus = EFileOpStatus::Success;
			}
		BSFH_VERBOSE_LOG(TXT("Size success with size {}"), Size);
	}
	catch (std::exception& Exception)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BSFH_HANDLE_EXCPETIONS();
	}
	catch (...)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BSFH_HANDLE_EXCPETIONS2();
	}

	return *this;
}

IFileHandle::OpRetType FBoostStreamFileHandle::Truncate(size_t Size)
{
	BSFH_HANDLE_STATUS(Status_InOut);
	BSFH_HANDLE_ENSURE(*Status_InOut, TXT("File operation continue with failed status"));
	BSFH_RECRSIVE_LOCK();

	try
	{
		BSFH_SCOPE_LOCK();
		{
			boost::system::error_code ec;
			boost::filesystem::resize_file(mFilePath.ToCharCStr(), Size, ec);
			BSFH_HANDLE_ENSURE(!ec, TXT("File truncate failed"));
		}

		if (Status_InOut->bCancelByUser)
			HLVM_UNLIKELY
			{
				Status_InOut->eFileOpStatus = EFileOpStatus::Canceled;
			}
		else
			HLVM_LIKELY
			{
				Status_InOut->eFileOpStatus = EFileOpStatus::Success;
			}
		BSFH_VERBOSE_LOG(TXT("Truncate success with size {}"), Size);
	}
	catch (std::exception& Exception)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BSFH_HANDLE_EXCPETIONS();
	}
	catch (...)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BSFH_HANDLE_EXCPETIONS2();
	}

	return *this;
}

HLVM_NODISCARD std::shared_ptr<IFFileStat> FBoostStreamFileHandle::Stat(const FPath& FilePath)
{
	std::shared_ptr<IFFileStat> Stat;
	BSFH_HANDLE_STATUS(Status_InOut);
	BSFH_HANDLE_ENSURE2(*Status_InOut, TXT("File operation continue with failed status"));
	BSFH_RECRSIVE_LOCK();

	try
	{
		BSFH_SCOPE_LOCK();
		{
			Stat = std::make_shared<FBoostFileStat>(FilePath);
		}

		if (Status_InOut->bCancelByUser)
			HLVM_UNLIKELY
			{
				Status_InOut->eFileOpStatus = EFileOpStatus::Canceled;
			}
		else
			HLVM_LIKELY
			{
				Status_InOut->eFileOpStatus = EFileOpStatus::Success;
			}
		BSFH_VERBOSE_LOG2(TXT("Stat success"));
	}
	catch (std::exception& Exception)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BSFH_HANDLE_EXCPETIONS();
	}
	catch (...)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BSFH_HANDLE_EXCPETIONS2();
	}

	return Stat;
}
