/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#include "Platform/FileSystem/Boost/BoostMapFileHandle.h"
#include "Platform/FileSystem/Boost/BoostFileStat.h"
#include "Core/Log.h"

#include <boost/interprocess/sync/sharable_lock.hpp>
#include <magic_enum_all.hpp>

DECLARE_LOG_CATEGORY(LogBoostFileHandle)

#define BMFH_RECRSIVE_LOCK() ATOMIC_LOCK_GUARD(mRecursiveLock)

#define BMFH_SCOPE_LOCK()                                                           \
	boost::interprocess::sharable_lock<boost::interprocess::file_lock> __lock_file; \
	if (mFileLock)                                                                  \
	__lock_file = MoveTemp(boost::interprocess::sharable_lock<boost::interprocess::file_lock>(*mFileLock))

#define BMFH_HANDLE_EXCPETIONS() HandleException(Status_InOut, TO_TCHAR_CSTR(__FUNCTION__), Exception)
#define BMFH_HANDLE_EXCPETIONS2() HandleException2(Status_InOut, TO_TCHAR_CSTR(__FUNCTION__))

#define BMFH_HANDLE_ASSERT(x, ...) HLVM_ASSERT_F(x, TXT("File {} : {}"), *mFilePath, FString::Format(__VA_ARGS__))
#define BMFH_HANDLE_ENSURE(x, ...) HLVM_ENSURE_F(x, TXT("File {} : {}"), *mFilePath, FString::Format(__VA_ARGS__))
#define BMFH_HANDLE_ENSURE2(x, ...) HLVM_ENSURE_F(x, TXT("File {} : {}"), *FilePath, FString::Format(__VA_ARGS__))
#define BMFH_VERBOSE_LOG(...)                                                                                   \
	do                                                                                                          \
	{                                                                                                           \
		if (Status_InOut->bVerbose)                                                                             \
			HLVM_LOG(LogBoostFileHandle, trace, TXT("File {} : {}"), *mFilePath, FString::Format(__VA_ARGS__)); \
	}                                                                                                           \
	while (0)
#define BMFH_VERBOSE_LOG2(...)                                                                                 \
	do                                                                                                         \
	{                                                                                                          \
		if (Status_InOut->bVerbose)                                                                            \
			HLVM_LOG(LogBoostFileHandle, trace, TXT("File {} : {}"), *FilePath, FString::Format(__VA_ARGS__)); \
	}                                                                                                          \
	while (0)

#define BMFH_HANDLE_STATUS(Status) OpStatusType Status = &FileOpStatus

FBoostMapFileHandle::~FBoostMapFileHandle()
{
	if (mOpened)
	{
		Close();
	}
}

IFileHandle::OpRetType FBoostMapFileHandle::Open(const FPath& FilePath, const FFileOptions& Options)
{
	BMFH_HANDLE_STATUS(Status_InOut);
	BMFH_HANDLE_ENSURE(*Status_InOut, TXT("File operation continue with failed status"));
	BMFH_HANDLE_ASSERT(!mOpened, TXT("File operation begin with another already open file"));

	mFilePath = FilePath;
	mFileOptions = Options;
	mFileOptions.eFileMapped = EFileMapped::Mapped;
	{
		BMFH_VERBOSE_LOG(TXT("Create Mapped file"));
		BMFH_HANDLE_ASSERT(!mMappedFile.is_open(), TXT("File operation begin with another already open file"));
	}

	if (mFileOptions.eFileAsync == EFileAsync::Async)
	{
		HLVM_NOT_IMPLEMENTED();
	}

	if (mFileOptions.eFileLock & EFileLock::ThreadLock)
	{
		BMFH_VERBOSE_LOG(TXT("Create thread lock"));
		mRecursiveLock = FRecursiveAtomicFlag();
	}

	try
	{
		{
			mMappedLazyInit = false;
			{
				auto file = std::fstream();
				file.open(FilePath, static_cast<std::ios::openmode>(mFileOptions.eFileMode | EFileMode::E));
				BMFH_HANDLE_ENSURE(file.is_open(), TXT("file open failed"));
				/**
				 * File not exists
				 */
				if (file.tellg() <= 0)
				{
					mMappedLazyInit = true;
				}
				file.close();
			}
			if (!mMappedLazyInit)
			{
				MappedFileInit();
			}
			else
			{
				BMFH_VERBOSE_LOG(TXT("Mapped File requires lazy init because not exists"));
			}
		}

		/**
		 * File lock can only init after success of open an existing file
		 */
		if (mFileOptions.eFileLock & EFileLock::InterProcessLock)
		{
			BMFH_VERBOSE_LOG(TXT("Create interprocess file lock"));
			mFileLock = new boost::interprocess::file_lock(FilePath);
		}

		mOpened = true;
		Status_InOut->eFileOpStatus = EFileOpStatus::Success;
		BMFH_VERBOSE_LOG(TXT("Open success with mode {}"), HLVM_ENUM_TO_TCHAR(mFileOptions.eFileMode));
	}
	catch (std::exception& Exception)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BMFH_HANDLE_EXCPETIONS();
	}
	catch (...)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BMFH_HANDLE_EXCPETIONS2();
	}

	return *this;
}

IFileHandle::OpRetType FBoostMapFileHandle::Close()
{
	BMFH_HANDLE_STATUS(Status_InOut);
	BMFH_HANDLE_ENSURE(*Status_InOut, TXT("File operation continue with failed status"));
	BMFH_HANDLE_ASSERT(mOpened, TXT("File operation continue w/o open"));

	try
	{
		BMFH_SCOPE_LOCK();

		{
			mMappedFile.close();
			BMFH_HANDLE_ASSERT(!mMappedFile.is_open(), TXT("File failed to close"));
		}

		{
			mMappedSeekPos = 0;
			HLVM_DELETE(mFileLock);
			mRecursiveLock.reset();
			mMappedLazyInit = false;
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
		BMFH_VERBOSE_LOG(TXT("Close file success"));
	}
	catch (std::exception& Exception)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BMFH_HANDLE_EXCPETIONS();
	}
	catch (...)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BMFH_HANDLE_EXCPETIONS2();
	}

	return *this;
}

IFileHandle::OpRetType FBoostMapFileHandle::Read(void* Buffer, size_t Size, const FFileSeekCtx& SeekCtx)
{
	BMFH_HANDLE_STATUS(Status_InOut);
	BMFH_HANDLE_ENSURE(*Status_InOut, TXT("File operation continue with failed status"));
	BMFH_HANDLE_ASSERT(mOpened, TXT("File operation continue w/o open"));
	BMFH_HANDLE_ASSERT(mFileOptions.eFileMode & EFileMode::R, TXT("File operation cannot read"));
	BMFH_HANDLE_ASSERT(Size > 0, TXT("Buffer size invalid {}"), Size);
	BMFH_RECRSIVE_LOCK();

	// tell if necessary
	int64_t Prev_Tell = { -1 };
	if (SeekCtx.bResetPos)
	{
		Tell(Prev_Tell);
		BMFH_HANDLE_ASSERT(Prev_Tell > 0, TXT("Tell failed before reset pos"), Prev_Tell);
	}
	// Seek if necessary
	if (SeekCtx.NonTrivialSeek())
	{
		Seek(SeekCtx.Offset, SeekCtx.Whence);
	}

	try
	{
		BMFH_SCOPE_LOCK();
		{
			BMFH_HANDLE_ASSERT(!mMappedLazyInit, TXT("Mapped file not init!"));
			size_t	FileSize = mMappedFile.size();
			int64_t rest_size = static_cast<int64_t>(FileSize) - (mMappedSeekPos);
			// Check space available for reading
			const bool available = rest_size >= static_cast<int64_t>(Size);
			BMFH_HANDLE_ASSERT(available, TXT("mMappedFile size is not enough for read. SeekPos {}, File Size {}, available size {} Buffer Size {}"),
				mMappedSeekPos, FileSize, rest_size, Size);

			auto readPos = MappedFileCurPos_R();
			// Check buffer and mmap no overlapping
			const bool overlap = IsPointerOverlap(Buffer, Size, readPos, static_cast<size_t>(rest_size));
			BMFH_HANDLE_ASSERT(!overlap, TXT("mMappedFile overlap with read region. SeekPos {}, File Size {}, available size {} Buffer Size {}"),
				mMappedSeekPos, FileSize, rest_size, Size);

			// do the mmap
			std::memcpy(Buffer, readPos, Size);
			mMappedSeekPos += Size;
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
		BMFH_VERBOSE_LOG(TXT("Read file success with {} bytes"), Size);
	}
	catch (std::exception& Exception)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BMFH_HANDLE_EXCPETIONS();
	}
	catch (...)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BMFH_HANDLE_EXCPETIONS2();
	}

	// Reset if necessary
	if (SeekCtx.bResetPos)
		HLVM_UNLIKELY
		{
			BMFH_VERBOSE_LOG(TXT("bResetPos after read"));
			Seek(Prev_Tell, EWhence::Begin);
		}
	// Erase if necessary
	else if (SeekCtx.bEraseSeekPos)
	{
		BMFH_VERBOSE_LOG(TXT("bEraseSeekPos after read"));
		Seek(0 - static_cast<int64_t>(Size), EWhence::Current);
	}

	return *this;
}

IFileHandle::OpRetType FBoostMapFileHandle::Write(const void* Buffer, size_t Size, const FFileSeekCtx& SeekCtx)
{
	BMFH_HANDLE_STATUS(Status_InOut);
	BMFH_HANDLE_ENSURE(*Status_InOut, TXT("File operation continue with failed status"));
	BMFH_HANDLE_ASSERT(mOpened, TXT("File operation continue w/o open"));
	BMFH_HANDLE_ASSERT(mFileOptions.eFileMode & EFileMode::W, TXT("File operation cannot write"));
	BMFH_HANDLE_ASSERT(Size > 0, TXT("Buffer size invalid {}"), Size);
	BMFH_RECRSIVE_LOCK();

	// tell if necessary
	int64_t Prev_Tell{ -1 };
	if (SeekCtx.bResetPos)
	{
		Tell(Prev_Tell);
		BMFH_HANDLE_ASSERT(Prev_Tell > 0, TXT("Tell failed before reset pos"), Prev_Tell);
	}
	// Seek if necessary
	if (SeekCtx.NonTrivialSeek())
	{
		Seek(SeekCtx.Offset, SeekCtx.Whence);
	}

	try
	{
		BMFH_SCOPE_LOCK();
		{
			if (mMappedLazyInit)
			{
				// Create file on lazy init with size of write buffer
				{
					std::filebuf fbuf;
					fbuf.open(mFilePath, std::ios::out | std::ios::trunc | std::ios::binary);
					// Set the size
					auto offset = mMappedSeekPos + static_cast<int64_t>(Size);
					fbuf.pubseekoff(offset - 1, std::ios::beg);
					fbuf.sputc(0);
					fbuf.close();
				}
				{
					MappedFileInit();
					BMFH_VERBOSE_LOG(TXT("Mapped File finish lazy init before write"));
				}
			}

			size_t	FileSize = mMappedFile.size();
			int64_t rest_size = static_cast<int64_t>(FileSize) - (mMappedSeekPos);
			// Resize file to fit write buffer if necessary
			if (rest_size < static_cast<int64_t>(Size))
			{
				auto new_size = static_cast<int64_t>(FileSize) - rest_size + static_cast<int64_t>(Size);
				mMappedFile.resize(new_size);
				BMFH_VERBOSE_LOG(TXT("Mapped File resize before write from {} to {}"), FileSize, new_size);
				{
					FileSize = mMappedFile.size();
					rest_size = static_cast<int64_t>(FileSize) - (mMappedSeekPos);
					bool available = rest_size >= static_cast<int64_t>(Size);
					BMFH_HANDLE_ASSERT(available, TXT("mMappedFile size is not enough for write. SeekPos {}, File Size {}, available size {} Buffer Size {}"),
						mMappedSeekPos, FileSize, rest_size, Size);
				}
			}

			auto writePos = MappedFileCurPos_W();
			// Check buffer and mmap no overlapping
			const bool overlap = IsPointerOverlap(Buffer, Size, writePos, static_cast<size_t>(rest_size));
			BMFH_HANDLE_ASSERT(!overlap, TXT("mMappedFile overlap with write region. SeekPos {}, File Size {}, available size {} Buffer Size {}"),
				mMappedSeekPos, FileSize, rest_size, Size);

			// Do mmap
			std::memcpy(writePos, Buffer, Size);
			// Advance seek pos
			mMappedSeekPos += Size;
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
		BMFH_VERBOSE_LOG(TXT("Write file success with {} bytes"), Size);
	}
	catch (std::exception& Exception)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BMFH_HANDLE_EXCPETIONS();
	}
	catch (...)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BMFH_HANDLE_EXCPETIONS2();
	}

	// Reset if necessary
	if (SeekCtx.bResetPos)
	{
		BMFH_VERBOSE_LOG(TXT("bResetPos after write"));
		Seek(Prev_Tell, EWhence::Begin);
	}
	// Erase if necessary
	else if (SeekCtx.bEraseSeekPos)
	{
		BMFH_VERBOSE_LOG(TXT("bEraseSeekPos after write"));
		Seek(0 - static_cast<int64_t>(Size), EWhence::Current);
	}

	return *this;
}

IFileHandle::OpRetType FBoostMapFileHandle::Flush()
{
	BMFH_HANDLE_STATUS(Status_InOut);
	BMFH_HANDLE_ENSURE(*Status_InOut, TXT("File operation continue with failed status"));
	BMFH_HANDLE_ASSERT(mOpened, TXT("File operation continue w/o open"));
	BMFH_HANDLE_ASSERT(mFileOptions.eFileMode & EFileMode::W, TXT("File operation cannot flush"));
	BMFH_RECRSIVE_LOCK();

	try
	{
		BMFH_SCOPE_LOCK();
		{
			// Mapped files are always flushed by the OS
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
		BMFH_VERBOSE_LOG(TXT("Flush file"));
	}
	catch (std::exception& Exception)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BMFH_HANDLE_EXCPETIONS();
	}
	catch (...)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BMFH_HANDLE_EXCPETIONS2();
	}

	return *this;
}

IFileHandle::OpRetType FBoostMapFileHandle::Seek(int64_t Offset, EWhence Whence)
{
	BMFH_HANDLE_STATUS(Status_InOut);
	BMFH_HANDLE_ENSURE(*Status_InOut, TXT("File operation continue with failed status"));
	BMFH_HANDLE_ASSERT(mOpened, TXT("File operation continue w/o open"));
	BMFH_RECRSIVE_LOCK();

	try
	{
		BMFH_SCOPE_LOCK();
		{
			switch (Whence)
			{
				case EWhence::Begin:
					mMappedSeekPos = Offset;
					break;
				case EWhence::Current:
					mMappedSeekPos += Offset;
					break;
				case EWhence::End:
					int64_t Size = static_cast<int64_t>(mMappedFile.size());
					mMappedSeekPos = Size - Offset;
					break;
			}
			BMFH_HANDLE_ENSURE(mMappedSeekPos >= 0, TXT("Map file seek out of range: {}"), mMappedSeekPos);
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
		BMFH_VERBOSE_LOG(TXT("Seek success given offset {} with {}"), Offset, HLVM_ENUM_TO_TCHAR(Whence));
	}
	catch (std::exception& Exception)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BMFH_HANDLE_EXCPETIONS();
	}
	catch (...)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BMFH_HANDLE_EXCPETIONS2();
	}

	return *this;
}

IFileHandle::IFileHandle::OpRetType FBoostMapFileHandle::Tell(int64_t& Offset)
{
	BMFH_HANDLE_STATUS(Status_InOut);
	BMFH_HANDLE_ENSURE(*Status_InOut, TXT("File operation continue with failed status"));
	BMFH_HANDLE_ASSERT(mOpened, TXT("File operation continue w/o open"));
	BMFH_RECRSIVE_LOCK();

	try
	{
		BMFH_SCOPE_LOCK();
		{
			Offset = mMappedSeekPos;
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
		BMFH_VERBOSE_LOG(TXT("Tell success with offset {}"), Offset);
	}
	catch (std::exception& Exception)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BMFH_HANDLE_EXCPETIONS();
	}
	catch (...)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BMFH_HANDLE_EXCPETIONS2();
	}

	return *this;
}

IFileHandle::OpRetType FBoostMapFileHandle::Size(size_t& Size)
{
	BMFH_HANDLE_STATUS(Status_InOut);
	BMFH_HANDLE_ENSURE(*Status_InOut, TXT("File operation continue with failed status"));
	BMFH_RECRSIVE_LOCK();

	try
	{
		BMFH_SCOPE_LOCK();
		{
			Size = mMappedFile.size();
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
		BMFH_VERBOSE_LOG(TXT("Size success with size {}"), Size);
	}
	catch (std::exception& Exception)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BMFH_HANDLE_EXCPETIONS();
	}
	catch (...)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BMFH_HANDLE_EXCPETIONS2();
	}

	return *this;
}

IFileHandle::OpRetType FBoostMapFileHandle::Truncate(size_t Size)
{
	BMFH_HANDLE_STATUS(Status_InOut);
	BMFH_HANDLE_ENSURE(*Status_InOut, TXT("File operation continue with failed status"));
	BMFH_RECRSIVE_LOCK();

	try
	{
		BMFH_SCOPE_LOCK();
		{
			boost::system::error_code ec;
			boost::filesystem::resize_file(mFilePath.ToCharCStr(), Size, ec);
			BMFH_HANDLE_ENSURE(!ec, TXT("File truncate failed"));
		}

		{
			/**
			 * Since truncation is same as writing, we need to finish lazy initialize the mapped file if required
			 */
			if (mMappedLazyInit)
			{
				MappedFileInit();
				BMFH_VERBOSE_LOG(TXT("Mapped File finish lazy init after truncation"));
			}
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
		BMFH_VERBOSE_LOG(TXT("Truncate success with size {}"), Size);
	}
	catch (std::exception& Exception)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BMFH_HANDLE_EXCPETIONS();
	}
	catch (...)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BMFH_HANDLE_EXCPETIONS2();
	}

	return *this;
}

HLVM_NODISCARD std::shared_ptr<IFFileStat> FBoostMapFileHandle::Stat(const FPath& FilePath)
{
	std::shared_ptr<IFFileStat> Stat;
	BMFH_HANDLE_STATUS(Status_InOut);
	BMFH_HANDLE_ENSURE2(*Status_InOut, TXT("File operation continue with failed status"));
	BMFH_RECRSIVE_LOCK();

	try
	{
		BMFH_SCOPE_LOCK();
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
		BMFH_VERBOSE_LOG2(TXT("Stat success"));
	}
	catch (std::exception& Exception)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BMFH_HANDLE_EXCPETIONS();
	}
	catch (...)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BMFH_HANDLE_EXCPETIONS2();
	}

	return Stat;
}

HLVM_NODISCARD FConstByteBuffer FBoostMapFileHandle::GetMappedBufferReadOnly() const
{
	BMFH_HANDLE_ASSERT(mOpened, TXT("File operation continue w/o open"));
	BMFH_HANDLE_ASSERT(!mMappedLazyInit, TXT("MappedFile still not init"));
	BMFH_HANDLE_ASSERT(mMappedFile.is_open(), TXT("MappedFile file open failed"));
	BMFH_HANDLE_ASSERT(mFileOptions.eFileMode & EFileMode::R && !(mFileOptions.eFileMode & EFileMode::W), TXT("File operation cannot flush"));
	return FConstByteBuffer{
		reinterpret_cast<const TBYTE*>(mMappedFile.const_data()), mMappedFile.size()
	};
}

void FBoostMapFileHandle::MappedFileInit()
{
	BMFH_HANDLE_ASSERT(!mMappedFile.is_open(), TXT("MappedFile file already opened!"));
	boost::iostreams::mapped_file_params params;
	params.path = mFilePath.ToCharCStr();
	params.flags = (mFileOptions.eFileMode & EFileMode::W) ? boost::iostreams::mapped_file::readwrite : boost::iostreams::mapped_file::readonly;
	mMappedFile.open(params);
	BMFH_HANDLE_ENSURE(mMappedFile.is_open(), TXT("MappedFile file open failed"));

	// Set seek pos to the end of file if ate mode
	if (mFileOptions.eFileMode & EFileMode::E)
	{
		size_t FileSize = mMappedFile.size();
		mMappedSeekPos = static_cast<int64_t>(FileSize);
		BMFH_HANDLE_ASSERT(mMappedSeekPos > 0, TXT("MappedFile file seek <= 0 with file size {}"), FileSize);
	}

	mMappedLazyInit = false;
}

const void* FBoostMapFileHandle::MappedFileCurPos_R(int64_t Offset) const
{
	auto _Offeset = mMappedSeekPos + Offset;
	auto _Size = static_cast<int64_t>(mMappedFile.size());
	BMFH_HANDLE_ASSERT(_Offeset >= 0 && _Offeset < _Size, FString::Format(TXT("MappedFileCurPos_R {} out of range [0,{})"), _Offeset, _Size));
	return (&(mMappedFile.const_data()[_Offeset]));
}

void* FBoostMapFileHandle::MappedFileCurPos_W(int64_t Offset)
{
	auto _Offeset = mMappedSeekPos + Offset;
	auto _Size = static_cast<int64_t>(mMappedFile.size());
	BMFH_HANDLE_ASSERT(_Offeset >= 0 && _Offeset < _Size, FString::Format(TXT("MappedFileCurPos_W {} out of range [0,{})"), _Offeset, _Size));
	return (&(mMappedFile.data()[_Offeset]));
}
