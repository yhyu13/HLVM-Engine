/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Platform/FileSystem/Boost/BoostFileHandle.h"
#include "Core/Log.h"

DELCARE_LOG_CATEGORY(LogBoostFileHandle)
DEFINE_LOG_CATEGORY(LogBoostFileHandle)

#define SCOPE_LOCK()                                                                \
	ATOMIC_LOCK_GUARD(mLock);                                                       \
	boost::interprocess::sharable_lock<boost::interprocess::file_lock> __lock_file; \
	if (mFileLock)                                                                  \
	__lock_file = MoveTemp(boost::interprocess::sharable_lock<boost::interprocess::file_lock>(*mFileLock))

#define HANDLE_EXCPETIONS() HandleException(Status_InOut, TO_TCHAR_STR(__FUNCTION__), Exception)
#define HANDLE_EXCPETIONS2() HandleException2(Status_InOut, TO_TCHAR_STR(__FUNCTION__))

#define HANDLE_ASSERT(x, ...) HLVM_ASSERT(x, TXT("File {} : {}"), *mFilePath, ##__VA_ARGS__)
#define HANDLE_ENSURE(x, ...) HLVM_ENSURE(x, TXT("File {} : {}"), *mFilePath, ##__VA_ARGS__)
#define VERBOSE_LOG(...)                                                                         \
	do                                                                                           \
	{                                                                                            \
		if (Status_InOut->bVerbose)                                                              \
			HLVM_LOG(LogBoostFileHandle, trace, TXT("File {} : {}"), *mFilePath, ##__VA_ARGS__); \
	}                                                                                            \
	while (0)
#define HANDLE_ENSURE2(x, ...) HLVM_ENSURE(x, TXT("File {} : {}"), *FilePath, ##__VA_ARGS__)
#define VERBOSE_LOG2(...)                                                                       \
	do                                                                                          \
	{                                                                                           \
		if (Status_InOut->bVerbose)                                                             \
			HLVM_LOG(LogBoostFileHandle, trace, TXT("File {} : {}"), *FilePath, ##__VA_ARGS__); \
	}                                                                                           \
	while (0)

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
		FFileOpStatus __Status_InOut;
		Close(&__Status_InOut);
	}
}

IFileHandle::OpRetType FBoostFileHandle::Open(const FPath& FilePath, const FFileOptions& Options, OpStatusType Status_InOut)
{
	mFilePath = FilePath;
	mFileOptions = Options;
	if (mFileOptions.eFileMapped == EFileMapped::Mapped)
	{
		VERBOSE_LOG(TXT("Create Mapped file"));
		mMappedFile = boost::iostreams::mapped_file();
	}
	else if (mFileOptions.eFileMapped == EFileMapped::NoMapped)
	{
		VERBOSE_LOG(TXT("Create file stream"));
		mFStream = std::fstream();
	}
	else
	{
		HANDLE_ASSERT(false, TXT("Not implemented"));
	}

	if (mFileOptions.eFileAsync == EFileAsync::Async)
	{
		HANDLE_ASSERT(false, TXT("Not implemented"));
	}

	if (mFileOptions.eFileLock & EFileLock::InterProcessLock)
	{
		VERBOSE_LOG(TXT("Create interprocess file lock"));
		mFileLock = boost::interprocess::file_lock(FilePath);
	}
	else if (mFileOptions.eFileLock & EFileLock::ThreadLock)
	{
		VERBOSE_LOG(TXT("Create thrad lock"));
		mLock = FAtomicFlag();
	}
	else if (mFileOptions.eFileLock == EFileLock::NoLock)
	{
	}
	else
	{
		HANDLE_ASSERT(false, TXT("Not implemented"));
	}

	try
	{
		SCOPE_LOCK();

		if (mFileOptions.eFileMapped == EFileMapped::Mapped)
		{
			HANDLE_ASSERT(mMappedFile, TXT("MappedFile file null"));
			boost::iostreams::mapped_file_params params;
			params.path = FilePath.ToCharStr();
			params.flags = (mFileOptions.eFileMode & EFileMode::W) ? boost::iostreams::mapped_file::readwrite : boost::iostreams::mapped_file::readonly;
			mMappedFile->open(params);
			HANDLE_ENSURE(mMappedFile->is_open(), TXT("MappedFile file open failed"));
			if (mFileOptions.eFileMode & EFileMode::E)
			{
				mMappedSeekPos = static_cast<int64_t>(mMappedFile->size());
			}
		}
		else
		{
			HANDLE_ASSERT(mFStream, TXT("FStream file null"));
			mFStream->open(FilePath, static_cast<std::ios::openmode>(mFileOptions.eFileMode));
			HANDLE_ENSURE(mFStream->is_open(), TXT("FStream open failed"));
		}
		mOpened = true;
		Status_InOut->eFileOpStatus = EFileOpStatus::Success;
		VERBOSE_LOG(FString::Format(TXT("Open success with {}"), TO_TCHAR_STR(magic_enum::enum_name(mFileOptions.eFileMode).data())));
	}
	catch (std::exception& Exception)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		HANDLE_EXCPETIONS();
	}
	catch (...)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		HANDLE_EXCPETIONS2();
	}

	return *this;
}

IFileHandle::OpRetType FBoostFileHandle::Close(OpStatusType Status_InOut)
{
	HANDLE_ENSURE(*Status_InOut, TXT("File operation continue with failed status"));
	HANDLE_ENSURE(mOpened, TXT("File operation continue w/o open"));

	try
	{
		SCOPE_LOCK();
		Status_InOut->Reset();

		if (mFileOptions.eFileMapped == EFileMapped::Mapped)
		{
			mMappedFile->close();
		}
		else
		{
			mFStream->close();
		}

		if (Status_InOut->bCancelByUser) [[unlikely]]
		{
			Status_InOut->eFileOpStatus = EFileOpStatus::Canceled;
		}
		else [[likely]]
		{
			Status_InOut->eFileOpStatus = EFileOpStatus::Success;
		}
		mOpened = false;
		VERBOSE_LOG(TXT("Close file success"));
	}
	catch (std::exception& Exception)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		HANDLE_EXCPETIONS();
	}
	catch (...)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		HANDLE_EXCPETIONS2();
	}

	return *this;
}

IFileHandle::OpRetType FBoostFileHandle::Read(void* Buffer, size_t Size, int64_t Offset, OpStatusType Status_InOut)
{
	HANDLE_ENSURE(*Status_InOut, TXT("File operation continue with failed status"));
	HANDLE_ENSURE(mOpened, TXT("File operation continue w/o open"));
	HANDLE_ENSURE(mFileOptions.eFileMode & EFileMode::R, TXT("File operation cannot read"));

	try
	{
		SCOPE_LOCK();
		if (mFileOptions.eFileMapped == EFileMapped::Mapped)
		{
			HANDLE_ASSERT(!IsPointerOverlap(Buffer, Size, MappedFileCurPos_R(Offset)), TXT("mMappedFile overlap with read region"));
			std::memcpy(Buffer, MappedFileCurPos_R(Offset), Size);
		}
		else
		{
			if (Offset != 0)
			{
				mFStream->seekg(static_cast<std::streamoff>(Offset));
			}
			mFStream->read(reinterpret_cast<char*>(Buffer), static_cast<std::streamsize>(Size));
		}

		if (Status_InOut->bCancelByUser) [[unlikely]]
		{
			Status_InOut->eFileOpStatus = EFileOpStatus::Canceled;
		}
		else [[likely]]
		{
			Status_InOut->eFileOpStatus = EFileOpStatus::Success;
		}
		VERBOSE_LOG(FString::Format(TXT("Read file success at offset {} with {} bytes"), Offset, Size));
	}
	catch (std::exception& Exception)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		HANDLE_EXCPETIONS();
	}
	catch (...)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		HANDLE_EXCPETIONS2();
	}

	return *this;
}

IFileHandle::OpRetType FBoostFileHandle::Write(const void* Buffer, size_t Size, int64_t Offset, OpStatusType Status_InOut)
{
	HANDLE_ENSURE(*Status_InOut, TXT("File operation continue with failed status"));
	HANDLE_ENSURE(mOpened, TXT("File operation continue w/o open"));
	HANDLE_ENSURE(mFileOptions.eFileMode & EFileMode::W, TXT("File operation cannot write"));

	try
	{
		SCOPE_LOCK();
		if (mFileOptions.eFileMapped == EFileMapped::Mapped)
		{
			HANDLE_ASSERT(!IsPointerOverlap(MappedFileCurPos_W(Offset), Size, Buffer), TXT("mMappedFile overlap with write region"));
			std::memcpy(MappedFileCurPos_W(Offset), Buffer, Size);
		}
		else
		{
			if (Offset != 0)
			{
				mFStream->seekp(static_cast<std::streamoff>(Offset));
			}
			mFStream->write(reinterpret_cast<const char*>(Buffer), static_cast<std::streamsize>(Size));
		}

		if (Status_InOut->bCancelByUser) [[unlikely]]
		{
			Status_InOut->eFileOpStatus = EFileOpStatus::Canceled;
		}
		else [[likely]]
		{
			Status_InOut->eFileOpStatus = EFileOpStatus::Success;
		}
		VERBOSE_LOG(FString::Format(TXT("Write file success at offset {} with {} bytes"), Offset, Size));
	}
	catch (std::exception& Exception)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		HANDLE_EXCPETIONS();
	}
	catch (...)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		HANDLE_EXCPETIONS2();
	}

	return *this;
}

IFileHandle::OpRetType FBoostFileHandle::Flush(OpStatusType Status_InOut)
{
	HANDLE_ENSURE(*Status_InOut, TXT("File operation continue with failed status"));
	HANDLE_ENSURE(mOpened, TXT("File operation continue w/o open"));
	HANDLE_ENSURE(mFileOptions.eFileMode & EFileMode::W, TXT("File operation cannot flush"));

	try
	{
		SCOPE_LOCK();
		if (mFileOptions.eFileMapped == EFileMapped::Mapped)
		{
			// Mapped files are always flushed by the OS
		}
		else
		{
			mFStream->flush();
		}

		if (Status_InOut->bCancelByUser) [[unlikely]]
		{
			Status_InOut->eFileOpStatus = EFileOpStatus::Canceled;
		}
		else [[likely]]
		{
			Status_InOut->eFileOpStatus = EFileOpStatus::Success;
		}
		VERBOSE_LOG(TXT("Flush file"));
	}
	catch (std::exception& Exception)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		HANDLE_EXCPETIONS();
	}
	catch (...)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		HANDLE_EXCPETIONS2();
	}

	return *this;
}

IFileHandle::OpRetType FBoostFileHandle::Seek(int64_t Offset, EWhence Whence, OpStatusType Status_InOut)
{
	HANDLE_ENSURE(*Status_InOut, TXT("File operation continue with failed status"));
	HANDLE_ENSURE(mOpened, TXT("File operation continue w/o open"));

	try
	{
		SCOPE_LOCK();
		if (mFileOptions.eFileMapped == EFileMapped::Mapped)
		{
			int64_t Size = static_cast<int64_t>(mMappedFile->size());
			switch (Whence)
			{
				case EWhence::Beg:
					mMappedSeekPos = Offset;
					break;
				case EWhence::Cur:
					mMappedSeekPos += Offset;
					break;
				case EWhence::End:
					mMappedSeekPos = Size - Offset;
					break;
			}
			HANDLE_ENSURE(mMappedSeekPos >= 0 && mMappedSeekPos <= Size, TXT("Map file seek out of range"));
		}
		else
		{
			mFStream->seekg(static_cast<std::streamoff>(Offset), static_cast<std::ios_base::seekdir>(Whence));
		}

		if (Status_InOut->bCancelByUser) [[unlikely]]
		{
			Status_InOut->eFileOpStatus = EFileOpStatus::Canceled;
		}
		else [[likely]]
		{
			Status_InOut->eFileOpStatus = EFileOpStatus::Success;
		}
		VERBOSE_LOG(FString::Format(TXT("Seek success at offset {} with {}"), Offset, TO_TCHAR_STR(magic_enum::enum_name(Whence).data())));
	}
	catch (std::exception& Exception)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		HANDLE_EXCPETIONS();
	}
	catch (...)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		HANDLE_EXCPETIONS2();
	}

	return *this;
}

IFileHandle::IFileHandle::OpRetType FBoostFileHandle::Tell(int64_t& Offset, OpStatusType Status_InOut)
{
	HANDLE_ENSURE(*Status_InOut, TXT("File operation continue with failed status"));
	HANDLE_ENSURE(mOpened, TXT("File operation continue w/o open"));

	try
	{
		SCOPE_LOCK();
		if (mFileOptions.eFileMapped == EFileMapped::Mapped)
		{
			Offset = mMappedSeekPos;
		}
		else
		{
			Offset = static_cast<int64_t>(mFStream->tellg());
			HANDLE_ENSURE(Offset >= 0, TXT("File Tell failed"));
		}

		if (Status_InOut->bCancelByUser) [[unlikely]]
		{
			Status_InOut->eFileOpStatus = EFileOpStatus::Canceled;
		}
		else [[likely]]
		{
			Status_InOut->eFileOpStatus = EFileOpStatus::Success;
		}
		VERBOSE_LOG(FString::Format(TXT("Tell success with offset {}"), Offset));
	}
	catch (std::exception& Exception)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		HANDLE_EXCPETIONS();
	}
	catch (...)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		HANDLE_EXCPETIONS2();
	}

	return *this;
}

IFileHandle::OpRetType FBoostFileHandle::Size(size_t& Size, OpStatusType Status_InOut)
{
	HANDLE_ENSURE(*Status_InOut, TXT("File operation continue with failed status"));

	try
	{
		SCOPE_LOCK();
		if (mFileOptions.eFileMapped == EFileMapped::Mapped)
		{
			Size = mMappedFile->size();
		}
		else
		{
			auto curPos = mFStream->tellg();
			mFStream->seekg(0, static_cast<std::ios_base::seekdir>(EWhence::End));
			Size = static_cast<size_t>(mFStream->tellg());
			mFStream->seekg(curPos, static_cast<std::ios_base::seekdir>(EWhence::Beg));
		}

		if (Status_InOut->bCancelByUser) [[unlikely]]
		{
			Status_InOut->eFileOpStatus = EFileOpStatus::Canceled;
		}
		else [[likely]]
		{
			Status_InOut->eFileOpStatus = EFileOpStatus::Success;
		}
		VERBOSE_LOG(FString::Format(TXT("Size success with size {}"), Size));
	}
	catch (std::exception& Exception)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		HANDLE_EXCPETIONS();
	}
	catch (...)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		HANDLE_EXCPETIONS2();
	}

	return *this;
}

IFileHandle::OpRetType FBoostFileHandle::Truncate(size_t Size, OpStatusType Status_InOut)
{
	HANDLE_ENSURE(*Status_InOut, TXT("File operation continue with failed status"));

	try
	{
		SCOPE_LOCK();
		{
			boost::system::error_code ec;
			boost::filesystem::resize_file(mFilePath.ToCharStr(), Size, ec);
			HANDLE_ENSURE(!ec, TXT("File truncate failed"));
		}

		if (Status_InOut->bCancelByUser) [[unlikely]]
		{
			Status_InOut->eFileOpStatus = EFileOpStatus::Canceled;
		}
		else [[likely]]
		{
			Status_InOut->eFileOpStatus = EFileOpStatus::Success;
		}
		VERBOSE_LOG(FString::Format(TXT("Truncate success with size {}"), Size));
	}
	catch (std::exception& Exception)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		HANDLE_EXCPETIONS();
	}
	catch (...)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		HANDLE_EXCPETIONS2();
	}

	return *this;
}

IFileHandle::OpRetType FBoostFileHandle::Stat(std::shared_ptr<IFFileStat>& Stat, const FPath& FilePath, OpStatusType Status_InOut)
{
	HANDLE_ENSURE2(*Status_InOut, TXT("File operation continue with failed status"));

	try
	{
		SCOPE_LOCK();
		{
			Stat = std::make_shared<FBoostFileStat>(FilePath);
		}

		if (Status_InOut->bCancelByUser) [[unlikely]]
		{
			Status_InOut->eFileOpStatus = EFileOpStatus::Canceled;
		}
		else [[likely]]
		{
			Status_InOut->eFileOpStatus = EFileOpStatus::Success;
		}
		VERBOSE_LOG2(TXT("Stat success"));
	}
	catch (std::exception& Exception)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		HANDLE_EXCPETIONS();
	}
	catch (...)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		HANDLE_EXCPETIONS2();
	}

	return *this;
}

const void* FBoostFileHandle::MappedFileCurPos_R(int64_t Offset) const
{
	auto _Offeset = mMappedSeekPos + Offset;
	auto _Size = static_cast<int64_t>(mMappedFile->size());
	HANDLE_ASSERT(_Offeset >= 0 && _Offeset < _Size, FString::Format(TXT("MappedFileCurPos_R {} out of range [0,{})"), _Offeset, _Size));
	return (&(mMappedFile->const_data()[_Offeset]));
}

void* FBoostFileHandle::MappedFileCurPos_W(int64_t Offset)
{
	auto _Offeset = mMappedSeekPos + Offset;
	auto _Size = static_cast<int64_t>(mMappedFile->size());
	HANDLE_ASSERT(_Offeset >= 0 && _Offeset < _Size, FString::Format(TXT("MappedFileCurPos_W {} out of range [0,{})"), _Offeset, _Size));
	return (&(mMappedFile->data()[_Offeset]));
}

void FBoostFileHandle::HandleException(const OpStatusType& Status_InOut, const TCHAR* Function, const std::exception& Exception)
{
	FString Msg = FString::Format(TXT("File {}: calling '{}' return {} with errorNo {} and exception {}"),
		*mFilePath,
		Function,
		TO_TCHAR_STR(magic_enum::enum_name(Status_InOut->eFileOpStatus).data()),
		TO_TCHAR_STR(magic_enum::enum_name(Status_InOut->eFileOpErrorNo).data()),
		TO_TCHAR_STR(Exception.what()));
	if (!Status_InOut->bCancelByUser)
	{
		HLVM_LOG(LogBoostFileHandle, err, MoveTemp(Msg));
		if (!Status_InOut->bSupressFailExceptions)
		{
			throw Exception;
		}
	}
	else
	{
		HLVM_LOG(LogBoostFileHandle, warn, TXT("{} but canceled by user, so we continue."), MoveTemp(Msg));
	}
}

void FBoostFileHandle::HandleException2(const OpStatusType& Status_InOut, const TCHAR* Function)
{
	FString Msg = FString::Format(TXT("File {}: calling '{}' return {} with errorNo {}"),
		*mFilePath,
		Function,
		TO_TCHAR_STR(magic_enum::enum_name(Status_InOut->eFileOpStatus).data()),
		TO_TCHAR_STR(magic_enum::enum_name(Status_InOut->eFileOpErrorNo).data()));
	if (!Status_InOut->bCancelByUser)
	{
		HLVM_LOG(LogBoostFileHandle, err, MoveTemp(Msg));
		if (!Status_InOut->bSupressFailExceptions)
		{
			// 重新抛出了当前正在处理的异常
			throw;
		}
	}
	else
	{
		HLVM_LOG(LogBoostFileHandle, warn, TXT("{} but canceled by user, so we continue."), MoveTemp(Msg));
	}
}
