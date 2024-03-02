/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Platform/FileSystem/Boost/BoostFileHandle.h"
#include "Core/Log.h"

#include <boost/interprocess/sync/sharable_lock.hpp>
#include <magic_enum_all.hpp>

DELCARE_LOG_CATEGORY(LogBoostFileHandle)
DEFINE_LOG_CATEGORY(LogBoostFileHandle)

#define BFH_RECRSIVE_LOCK() ATOMIC_LOCK_GUARD(mRecursiveLock)

#define BFH_SCOPE_LOCK()                                                            \
	boost::interprocess::sharable_lock<boost::interprocess::file_lock> __lock_file; \
	if (mFileLock)                                                                  \
	__lock_file = MoveTemp(boost::interprocess::sharable_lock<boost::interprocess::file_lock>(*mFileLock))

#define BFH_HANDLE_EXCPETIONS() HandleException(Status_InOut, TO_TCHAR_STR(__FUNCTION__), Exception)
#define BFH_HANDLE_EXCPETIONS2() HandleException2(Status_InOut, TO_TCHAR_STR(__FUNCTION__))

#define BFH_HANDLE_ASSERT(x, ...) HLVM_ASSERT(x, TXT("File {} : {}"), *mFilePath, FString::Format(__VA_ARGS__))
#define BFH_HANDLE_ENSURE(x, ...) HLVM_ENSURE(x, TXT("File {} : {}"), *mFilePath, FString::Format(__VA_ARGS__))
#define BFH_HANDLE_ENSURE2(x, ...) HLVM_ENSURE(x, TXT("File {} : {}"), *FilePath, FString::Format(__VA_ARGS__))
#define BFH_VERBOSE_LOG(...)                                                                                    \
	do                                                                                                          \
	{                                                                                                           \
		if (Status_InOut->bVerbose)                                                                             \
			HLVM_LOG(LogBoostFileHandle, trace, TXT("File {} : {}"), *mFilePath, FString::Format(__VA_ARGS__)); \
	}                                                                                                           \
	while (0)
#define BFH_VERBOSE_LOG2(...)                                                                                  \
	do                                                                                                         \
	{                                                                                                          \
		if (Status_InOut->bVerbose)                                                                            \
			HLVM_LOG(LogBoostFileHandle, trace, TXT("File {} : {}"), *FilePath, FString::Format(__VA_ARGS__)); \
	}                                                                                                          \
	while (0)

#define BFH_HANDLE_STATUS(Status) OpStatusType Status = &FileOpStatus

FBoostFileStat::FBoostFileStat(const FPath& Path)
{
	boost::system::error_code ec;
	fs = boost::filesystem::status(Path.ToCharStr(), ec);
	HLVM_ENSURE(!ec, TXT("File {} stat failed"), *Path);
}

bool FBoostFileStat::IsDirectory() const
{
	return boost::filesystem::is_directory(fs);
}
bool FBoostFileStat::Exists() const
{
	return boost::filesystem::exists(fs);
}
bool FBoostFileStat::IsFile() const
{
	return boost::filesystem::is_regular_file(fs)
		&& !boost::filesystem::is_directory(fs)
		&& !boost::filesystem::is_symlink(fs);
}
bool FBoostFileStat::IsLink() const
{
	return !boost::filesystem::is_regular_file(fs)
		&& !boost::filesystem::is_directory(fs)
		&& boost::filesystem::is_symlink(fs);
}

FBoostFileHandle::~FBoostFileHandle()
{
	if (mOpened)
	{
		Close();
	}
}

IFileHandle::OpRetType FBoostFileHandle::Open(const FPath& FilePath, const FFileOptions& Options)
{
	BFH_HANDLE_STATUS(Status_InOut);
	BFH_HANDLE_ENSURE(*Status_InOut, TXT("File operation continue with failed status"));
	BFH_HANDLE_ASSERT(!mOpened, TXT("File operation begin with another already open file"));

	mFilePath = FilePath;
	mFileOptions = Options;
	if (mFileOptions.eFileMapped == EFileMapped::Mapped)
	{
		BFH_VERBOSE_LOG(TXT("Create Mapped file"));
		mMappedFile = new boost::iostreams::mapped_file();
	}
	else if (mFileOptions.eFileMapped == EFileMapped::NoMapped)
	{
		BFH_VERBOSE_LOG(TXT("Create file stream"));
		mFStream = new std::fstream();
	}
	else
	{
		HLVM_NOT_IMPLEMENTED();
	}

	if (mFileOptions.eFileAsync == EFileAsync::Async)
	{
		HLVM_NOT_IMPLEMENTED();
	}

	if (mFileOptions.eFileLock & EFileLock::ThreadLock)
	{
		BFH_VERBOSE_LOG(TXT("Create thread lock"));
		mRecursiveLock = FRecursiveAtomicFlag();
	}

	try
	{
		if (mFileOptions.eFileMapped == EFileMapped::Mapped)
		{
			mMappedLazyInit = false;
			{
				auto file = std::fstream();
				file.open(FilePath, static_cast<std::ios::openmode>(mFileOptions.eFileMode | EFileMode::E));
				BFH_HANDLE_ENSURE(file.is_open(), TXT("file open failed"));
				if (file.tellg() <= 0)
				{
					mMappedLazyInit = true;
				}
				file.close();
			}
			if (!mMappedLazyInit)
			{
				MappedFileLazyInit();
			}
			else
			{
				BFH_VERBOSE_LOG(TXT("Mapped File requires lazy init"));
			}
		}
		else
		{
			BFH_HANDLE_ASSERT(mFStream, TXT("FStream file null"));
			mFStream->open(FilePath, static_cast<std::ios::openmode>(mFileOptions.eFileMode));
			BFH_HANDLE_ENSURE(mFStream->is_open(), TXT("FStream open failed"));
		}

		/**
		 * File lock can only init after success of open an existing file
		 */
		if (mFileOptions.eFileLock & EFileLock::InterProcessLock)
		{
			BFH_VERBOSE_LOG(TXT("Create interprocess file lock"));
			mFileLock = new boost::interprocess::file_lock(FilePath);
		}

		mOpened = true;
		Status_InOut->eFileOpStatus = EFileOpStatus::Success;
		BFH_VERBOSE_LOG(TXT("Open success with mode {}"), TO_TCHAR_STR(magic_enum::enum_name(mFileOptions.eFileMode).data()));
	}
	catch (std::exception& Exception)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BFH_HANDLE_EXCPETIONS();
	}
	catch (...)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BFH_HANDLE_EXCPETIONS2();
	}

	return *this;
}

IFileHandle::OpRetType FBoostFileHandle::Close()
{
	BFH_HANDLE_STATUS(Status_InOut);
	BFH_HANDLE_ENSURE(*Status_InOut, TXT("File operation continue with failed status"));
	BFH_HANDLE_ASSERT(mOpened, TXT("File operation continue w/o open"));

	try
	{
		BFH_SCOPE_LOCK();

		if (mFileOptions.eFileMapped == EFileMapped::Mapped)
		{
			mMappedFile->close();
		}
		else
		{
			mFStream->close();
			BFH_HANDLE_ENSURE(!mFStream->fail(), TXT("File operation failed"));
		}

		{
			mMappedSeekPos = 0;
			HLVM_DELETE(mMappedFile);
			HLVM_DELETE(mFStream);
			HLVM_DELETE(mFileLock);
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
		BFH_VERBOSE_LOG(TXT("Close file success"));
	}
	catch (std::exception& Exception)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BFH_HANDLE_EXCPETIONS();
	}
	catch (...)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BFH_HANDLE_EXCPETIONS2();
	}

	return *this;
}

IFileHandle::OpRetType FBoostFileHandle::Read(void* Buffer, size_t Size, const FFileSeekCtx& SeekCtx)
{
	BFH_HANDLE_STATUS(Status_InOut);
	BFH_HANDLE_ENSURE(*Status_InOut, TXT("File operation continue with failed status"));
	BFH_HANDLE_ASSERT(mOpened, TXT("File operation continue w/o open"));
	BFH_HANDLE_ASSERT(mFileOptions.eFileMode & EFileMode::R, TXT("File operation cannot read"));
	BFH_HANDLE_ASSERT(Size > 0, TXT("Buffer size invalid {}"), Size);
	BFH_RECRSIVE_LOCK();

	// tell if necessary
	int64_t Prev_Tell = { -1 };
	if (SeekCtx.bResetPos)
	{
		Tell(Prev_Tell);
		BFH_HANDLE_ASSERT(Prev_Tell > 0, TXT("Tell failed before reset pos"), Prev_Tell);
	}
	// Seek if necessary
	if (SeekCtx.NonTrivialSeek())
	{
		Seek(SeekCtx.Offset, SeekCtx.Whence);
	}

	try
	{
		BFH_SCOPE_LOCK();
		if (mFileOptions.eFileMapped == EFileMapped::Mapped)
		{
			BFH_HANDLE_ASSERT(!mMappedLazyInit, TXT("Mapped file not init!"));
			size_t	FileSize = mMappedFile->size();
			int64_t rest_size = static_cast<int64_t>(FileSize) - (mMappedSeekPos);
			// Check space available for reading
			const bool available = rest_size >= static_cast<int64_t>(Size);
			BFH_HANDLE_ASSERT(available, TXT("mMappedFile size is not enough for read. SeekPos {}, File Size {}, available size {} Buffer Size {}"),
				mMappedSeekPos, FileSize, rest_size, Size);

			auto readPos = MappedFileCurPos_R();
			// Check buffer and mmap no overlapping
			const bool overlap = IsPointerOverlap(Buffer, Size, readPos, static_cast<size_t>(rest_size));
			BFH_HANDLE_ASSERT(!overlap, TXT("mMappedFile overlap with read region. SeekPos {}, File Size {}, available size {} Buffer Size {}"),
				mMappedSeekPos, FileSize, rest_size, Size);

			// do the mmap
			std::memcpy(Buffer, readPos, Size);
			mMappedSeekPos += Size;
		}
		else
		{
			mFStream->read(reinterpret_cast<char*>(Buffer), static_cast<std::streamsize>(Size));
			BFH_HANDLE_ENSURE(!mFStream->fail(), TXT("File operation failed"));
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
		BFH_VERBOSE_LOG(TXT("Read file success with {} bytes"), Size);
	}
	catch (std::exception& Exception)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BFH_HANDLE_EXCPETIONS();
	}
	catch (...)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BFH_HANDLE_EXCPETIONS2();
	}

	// Reset if necessary
	if (SeekCtx.bResetPos)
		HLVM_UNLIKELY
		{
			BFH_VERBOSE_LOG(TXT("bResetPos after read"));
			Seek(Prev_Tell, EWhence::Begin);
		}
	// Erase if necessary
	else if (SeekCtx.bEraseSeekPos)
	{
		BFH_VERBOSE_LOG(TXT("bEraseSeekPos after read"));
		Seek(0 - static_cast<int64_t>(Size), EWhence::Current);
	}

	return *this;
}

IFileHandle::OpRetType FBoostFileHandle::Write(const void* Buffer, size_t Size, const FFileSeekCtx& SeekCtx)
{
	BFH_HANDLE_STATUS(Status_InOut);
	BFH_HANDLE_ENSURE(*Status_InOut, TXT("File operation continue with failed status"));
	BFH_HANDLE_ASSERT(mOpened, TXT("File operation continue w/o open"));
	BFH_HANDLE_ASSERT(mFileOptions.eFileMode & EFileMode::W, TXT("File operation cannot write"));
	BFH_HANDLE_ASSERT(Size > 0, TXT("Buffer size invalid {}"), Size);
	BFH_RECRSIVE_LOCK();

	// tell if necessary
	int64_t Prev_Tell{ -1 };
	if (SeekCtx.bResetPos)
	{
		Tell(Prev_Tell);
		BFH_HANDLE_ASSERT(Prev_Tell > 0, TXT("Tell failed before reset pos"), Prev_Tell);
	}
	// Seek if necessary
	if (SeekCtx.NonTrivialSeek())
	{
		Seek(SeekCtx.Offset, SeekCtx.Whence);
	}

	try
	{
		BFH_SCOPE_LOCK();
		if (mFileOptions.eFileMapped == EFileMapped::Mapped)
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

				MappedFileLazyInit();
				BFH_VERBOSE_LOG(TXT("Mapped File finish lazy init before write"));
			}

			size_t	FileSize = mMappedFile->size();
			int64_t rest_size = static_cast<int64_t>(FileSize) - (mMappedSeekPos);
			// Resize file to fit write buffer if necessary
			if (rest_size < static_cast<int64_t>(Size))
			{
				auto new_size = static_cast<int64_t>(FileSize) - rest_size + static_cast<int64_t>(Size);
				mMappedFile->resize(new_size);
				BFH_VERBOSE_LOG(TXT("Mapped File resize before write from {} to {}"), FileSize, new_size);
				{
					FileSize = mMappedFile->size();
					rest_size = static_cast<int64_t>(FileSize) - (mMappedSeekPos);
					bool available = rest_size >= static_cast<int64_t>(Size);
					BFH_HANDLE_ASSERT(available, TXT("mMappedFile size is not enough for write. SeekPos {}, File Size {}, available size {} Buffer Size {}"),
						mMappedSeekPos, FileSize, rest_size, Size);
				}
			}

			auto writePos = MappedFileCurPos_W();
			// Check buffer and mmap no overlapping
			const bool overlap = IsPointerOverlap(Buffer, Size, writePos, static_cast<size_t>(rest_size));
			BFH_HANDLE_ASSERT(!overlap, TXT("mMappedFile overlap with write region. SeekPos {}, File Size {}, available size {} Buffer Size {}"),
				mMappedSeekPos, FileSize, rest_size, Size);

			// Do mmap
			std::memcpy(writePos, Buffer, Size);
			// Advance seek pos
			mMappedSeekPos += Size;
		}
		else
		{
			mFStream->write(reinterpret_cast<const char*>(Buffer), static_cast<std::streamsize>(Size));
			BFH_HANDLE_ENSURE(!mFStream->fail(), TXT("File operation failed"));
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
		BFH_VERBOSE_LOG(TXT("Write file success with {} bytes"), Size);
	}
	catch (std::exception& Exception)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BFH_HANDLE_EXCPETIONS();
	}
	catch (...)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BFH_HANDLE_EXCPETIONS2();
	}

	// Reset if necessary
	if (SeekCtx.bResetPos)
	{
		BFH_VERBOSE_LOG(TXT("bResetPos after write"));
		Seek(Prev_Tell, EWhence::Begin);
	}
	// Erase if necessary
	else if (SeekCtx.bEraseSeekPos)
	{
		BFH_VERBOSE_LOG(TXT("bEraseSeekPos after write"));
		Seek(0 - static_cast<int64_t>(Size), EWhence::Current);
	}

	return *this;
}

IFileHandle::OpRetType FBoostFileHandle::Flush()
{
	BFH_HANDLE_STATUS(Status_InOut);
	BFH_HANDLE_ENSURE(*Status_InOut, TXT("File operation continue with failed status"));
	BFH_HANDLE_ASSERT(mOpened, TXT("File operation continue w/o open"));
	BFH_HANDLE_ASSERT(mFileOptions.eFileMode & EFileMode::W, TXT("File operation cannot flush"));
	BFH_RECRSIVE_LOCK();

	try
	{
		BFH_SCOPE_LOCK();
		if (mFileOptions.eFileMapped == EFileMapped::Mapped)
		{
			// Mapped files are always flushed by the OS
		}
		else
		{
			mFStream->flush();
			BFH_HANDLE_ENSURE(!mFStream->fail(), TXT("File operation failed"));
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
		BFH_VERBOSE_LOG(TXT("Flush file"));
	}
	catch (std::exception& Exception)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BFH_HANDLE_EXCPETIONS();
	}
	catch (...)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BFH_HANDLE_EXCPETIONS2();
	}

	return *this;
}

IFileHandle::OpRetType FBoostFileHandle::Seek(int64_t Offset, EWhence Whence)
{
	BFH_HANDLE_STATUS(Status_InOut);
	BFH_HANDLE_ENSURE(*Status_InOut, TXT("File operation continue with failed status"));
	BFH_HANDLE_ASSERT(mOpened, TXT("File operation continue w/o open"));
	BFH_RECRSIVE_LOCK();

	try
	{
		BFH_SCOPE_LOCK();
		if (mFileOptions.eFileMapped == EFileMapped::Mapped)
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
					int64_t Size = static_cast<int64_t>(mMappedFile->size());
					mMappedSeekPos = Size - Offset;
					break;
			}
			BFH_HANDLE_ENSURE(mMappedSeekPos >= 0, TXT("Map file seek out of range: {}"), mMappedSeekPos);
		}
		else
		{
			mFStream->seekg(static_cast<std::streamoff>(Offset), static_cast<std::ios::seekdir>(Whence));
			BFH_HANDLE_ENSURE(!mFStream->fail(), TXT("File operation failed"));
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
		BFH_VERBOSE_LOG(TXT("Seek success given offset {} with {}"), Offset, TO_TCHAR_STR(magic_enum::enum_name(Whence).data()));
	}
	catch (std::exception& Exception)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BFH_HANDLE_EXCPETIONS();
	}
	catch (...)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BFH_HANDLE_EXCPETIONS2();
	}

	return *this;
}

IFileHandle::IFileHandle::OpRetType FBoostFileHandle::Tell(int64_t& Offset)
{
	BFH_HANDLE_STATUS(Status_InOut);
	BFH_HANDLE_ENSURE(*Status_InOut, TXT("File operation continue with failed status"));
	BFH_HANDLE_ASSERT(mOpened, TXT("File operation continue w/o open"));
	BFH_RECRSIVE_LOCK();

	try
	{
		BFH_SCOPE_LOCK();
		if (mFileOptions.eFileMapped == EFileMapped::Mapped)
		{
			Offset = mMappedSeekPos;
		}
		else
		{
			Offset = static_cast<int64_t>(mFStream->tellg());
			BFH_HANDLE_ENSURE(Offset >= 0, TXT("File Tell failed with value {}"), Offset);
			BFH_HANDLE_ENSURE(!mFStream->fail(), TXT("File operation failed"));
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
		BFH_VERBOSE_LOG(TXT("Tell success with offset {}"), Offset);
	}
	catch (std::exception& Exception)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BFH_HANDLE_EXCPETIONS();
	}
	catch (...)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BFH_HANDLE_EXCPETIONS2();
	}

	return *this;
}

IFileHandle::OpRetType FBoostFileHandle::Size(size_t& Size)
{
	BFH_HANDLE_STATUS(Status_InOut);
	BFH_HANDLE_ENSURE(*Status_InOut, TXT("File operation continue with failed status"));
	BFH_RECRSIVE_LOCK();

	try
	{
		BFH_SCOPE_LOCK();
		if (mFileOptions.eFileMapped == EFileMapped::Mapped)
		{
			Size = mMappedFile->size();
		}
		else
		{
			auto curPos = mFStream->tellg();
			mFStream->seekg(0, static_cast<std::ios::seekdir>(EWhence::End));
			Size = static_cast<size_t>(mFStream->tellg());
			mFStream->seekg(curPos, static_cast<std::ios::seekdir>(EWhence::Begin));
			BFH_HANDLE_ENSURE(!mFStream->fail(), TXT("File operation failed"));
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
		BFH_VERBOSE_LOG(TXT("Size success with size {}"), Size);
	}
	catch (std::exception& Exception)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BFH_HANDLE_EXCPETIONS();
	}
	catch (...)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BFH_HANDLE_EXCPETIONS2();
	}

	return *this;
}

IFileHandle::OpRetType FBoostFileHandle::Truncate(size_t Size)
{
	BFH_HANDLE_STATUS(Status_InOut);
	BFH_HANDLE_ENSURE(*Status_InOut, TXT("File operation continue with failed status"));
	BFH_RECRSIVE_LOCK();

	try
	{
		BFH_SCOPE_LOCK();
		{
			boost::system::error_code ec;
			boost::filesystem::resize_file(mFilePath.ToCharStr(), Size, ec);
			BFH_HANDLE_ENSURE(!ec, TXT("File truncate failed"));
		}

		if (mFileOptions.eFileMapped == EFileMapped::Mapped)
		{
			if (mMappedLazyInit)
			{
				MappedFileLazyInit();
				BFH_VERBOSE_LOG(TXT("Mapped File finish lazy init after truncation"));
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
		BFH_VERBOSE_LOG(TXT("Truncate success with size {}"), Size);
	}
	catch (std::exception& Exception)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BFH_HANDLE_EXCPETIONS();
	}
	catch (...)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BFH_HANDLE_EXCPETIONS2();
	}

	return *this;
}

HLVM_NODISCARD std::shared_ptr<IFFileStat> FBoostFileHandle::Stat(const FPath& FilePath)
{
	std::shared_ptr<IFFileStat> Stat;
	BFH_HANDLE_STATUS(Status_InOut);
	BFH_HANDLE_ENSURE2(*Status_InOut, TXT("File operation continue with failed status"));
	BFH_RECRSIVE_LOCK();

	try
	{
		BFH_SCOPE_LOCK();
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
		BFH_VERBOSE_LOG2(TXT("Stat success"));
	}
	catch (std::exception& Exception)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BFH_HANDLE_EXCPETIONS();
	}
	catch (...)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		BFH_HANDLE_EXCPETIONS2();
	}

	return Stat;
}

HLVM_NODISCARD FConstByteBuffer FBoostFileHandle::GetMappedBufferReadOnly() const
{
	BFH_HANDLE_ASSERT(mOpened, TXT("File operation continue w/o open"));
	BFH_HANDLE_ASSERT(mMappedFile, TXT("MappedFile file null"));
	BFH_HANDLE_ASSERT(!mMappedLazyInit, TXT("MappedFile still not init"));
	BFH_HANDLE_ASSERT(mFileOptions.eFileMode & EFileMode::R && !(mFileOptions.eFileMode & EFileMode::W), TXT("File operation cannot flush"));
	return FConstByteBuffer{
		reinterpret_cast<const TByte*>(mMappedFile->const_data()), mMappedFile->size()
	};
}

void FBoostFileHandle::MappedFileLazyInit()
{
	BFH_HANDLE_ASSERT(mMappedFile, TXT("MappedFile file null"));
	boost::iostreams::mapped_file_params params;
	params.path = mFilePath.ToCharStr();
	params.flags = (mFileOptions.eFileMode & EFileMode::W) ? boost::iostreams::mapped_file::readwrite : boost::iostreams::mapped_file::readonly;
	mMappedFile->open(params);
	BFH_HANDLE_ENSURE(mMappedFile->is_open(), TXT("MappedFile file open failed"));
	// Set seek pos to the end of file if ate mode
	if (mFileOptions.eFileMode & EFileMode::E)
	{
		size_t FileSize = mMappedFile->size();
		mMappedSeekPos = static_cast<int64_t>(FileSize);
		BFH_HANDLE_ASSERT(mMappedSeekPos > 0, TXT("MappedFile file seek <= 0 with file size {}"), FileSize);
	}

	mMappedLazyInit = false;
}

const void* FBoostFileHandle::MappedFileCurPos_R(int64_t Offset) const
{
	auto _Offeset = mMappedSeekPos + Offset;
	auto _Size = static_cast<int64_t>(mMappedFile->size());
	BFH_HANDLE_ASSERT(_Offeset >= 0 && _Offeset < _Size, FString::Format(TXT("MappedFileCurPos_R {} out of range [0,{})"), _Offeset, _Size));
	return (&(mMappedFile->const_data()[_Offeset]));
}

void* FBoostFileHandle::MappedFileCurPos_W(int64_t Offset)
{
	auto _Offeset = mMappedSeekPos + Offset;
	auto _Size = static_cast<int64_t>(mMappedFile->size());
	BFH_HANDLE_ASSERT(_Offeset >= 0 && _Offeset < _Size, FString::Format(TXT("MappedFileCurPos_W {} out of range [0,{})"), _Offeset, _Size));
	return (&(mMappedFile->data()[_Offeset]));
}
