/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once
#include "FileSystemDefinition.h"
#include "Template/PointerTemplate.tpp"
#include "Path.h"
#include "Core/Assert.h"

/**
 * @brief 文件打开模式，和python类似
 */
enum class EFileMode : TUINT8
{
	R = std::ios::in,
	W = std::ios::out,
	A = std::ios::app,
	T = std::ios::trunc,
	E = std::ios::ate,
	B = std::ios::binary,
	// Do not use above flags, use combo flags below
	RW = R | W | T,
	RWB = R | W | T | B,
	RWA = R | W | A,
	RWAB = R | W | A | B,
	WA = W | A | E,
	WAB = W | A | E | B,
	RB = R | B,
	WB = W | T | B,
};

inline bool operator&(EFileMode a, EFileMode b)
{
	return static_cast<TUINT8>(a) & static_cast<TUINT8>(b);
}

inline TUINT8 operator|(EFileMode a, EFileMode b)
{
	return static_cast<TUINT8>(a) | static_cast<TUINT8>(b);
}

inline bool operator==(EFileMode a, EFileMode b)
{
	return static_cast<TUINT8>(a) == static_cast<TUINT8>(b);
}

inline bool operator!=(EFileMode a, EFileMode b)
{
	return static_cast<TUINT8>(a) != static_cast<TUINT8>(b);
}

enum class EFileMapped : TUINT8
{
	Default = 0,
	NoMapped = 1,
	Mapped = 2,
};

enum class EFileAsync : TUINT8
{
	Default = 0,
	NoAsync = 1,
	Async = 2,
};

enum class EFileLock : TUINT8
{
	NoLock = 0,
	ThreadLock = 1 << 0,
	InterProcessLock = 1 << 1,
	FullLock = ThreadLock | InterProcessLock
};
inline bool operator&(EFileLock a, EFileLock b)
{
	return static_cast<TUINT8>(a) & static_cast<TUINT8>(b);
}

/**
 * @brief 文件操作选项
 */
struct FFileOptions
{
	EFileMode	eFileMode{ EFileMode::R };
	EFileMapped eFileMapped{ EFileMapped::Default };
	EFileAsync	eFileAsync{ EFileAsync::Default };
	EFileLock	eFileLock{ EFileLock::NoLock };
};
HLVM_INLINE_VAR const FFileOptions GReadOnlyFileOptions{
	.eFileMode = EFileMode::R,
	.eFileMapped = EFileMapped::Default,
	.eFileAsync = EFileAsync::Default,
	.eFileLock = EFileLock::NoLock,
};

/**
 * @brief 文件操作状态, report by underlying file system
 */
enum class EFileOpStatus : TUINT8
{
	Waiting = 0,
	Success = 1,
	Failed = 2,
	Canceled = 3,
};

enum class EFileOpErrorNo : TUINT8
{
	OK = 0,
	TimeOut = 1,
	NotSupported = 2,
	NotExist = 3,
	AccessDeny = 4,
	AlreadyOpen = 5,
	Unknown,
};

/**
 * @brief 文件操作状态
 */
struct FFileOpStatus
{
	EFileOpStatus  eFileOpStatus{ EFileOpStatus::Waiting };
	EFileOpErrorNo eFileOpErrorNo{ EFileOpErrorNo::OK }; // 错误码
	BIT_FLAG(bSupressFailExceptions){ false };			 // 是否忽略错误时候抛出的异常
#if HLVM_BUILD_DEBUG
	BIT_FLAG(bVerbose){ true };
#else
	BIT_FLAG(bVerbose){ false };
#endif
	volatile BIT_FLAG(bCancelByUser){ false }; // 是否由用户取消

	void Reset() noexcept
	{
		eFileOpStatus = EFileOpStatus::Waiting;
		bCancelByUser = false;
		eFileOpErrorNo = EFileOpErrorNo::OK;
	}

	bool IsSuccess() const noexcept
	{
		return eFileOpStatus == EFileOpStatus::Success && eFileOpErrorNo == EFileOpErrorNo::OK;
	}

	bool IsFailed() const noexcept
	{
		return eFileOpStatus == EFileOpStatus::Failed || eFileOpErrorNo != EFileOpErrorNo::OK;
	}

	bool IsCanceled() const noexcept
	{
		return bCancelByUser || eFileOpStatus == EFileOpStatus::Canceled;
	}

	operator bool() const noexcept
	{
		return (IsSuccess() || !IsFailed()) && !IsCanceled();
	}
};

enum class EWhence : TUINT8
{
	Begin = std::ios::beg,
	Current = std::ios::cur,
	End = std::ios::end,
};
inline bool operator&(EWhence a, EWhence b)
{
	return static_cast<TUINT8>(a) == static_cast<TUINT8>(b);
}

struct FFileSeekCtx
{
	int64_t Offset{ 0 };
	EWhence Whence{ EWhence::Current };
	BIT_FLAG(bResetPos){ false };	  // Reset seek pos to previous location after reading and writing, so to ignore the effect of this file seeking
	BIT_FLAG(bEraseSeekPos){ false }; // Erase the effect of advancing seek pos when reading and writing, so to keep our seek pos unchanged

	/**
	 * see if non trivial seek, which requires extra calling seek
	 */
	bool NonTrivialSeek() const
	{
		return !(Offset == 0 && Whence == EWhence::Current);
	}
};
HLVM_INLINE_VAR const FFileSeekCtx GFileSeekBegCtx{ 0, EWhence::Begin, false, false };
HLVM_INLINE_VAR const FFileSeekCtx GFileSeekEndCtx{ 0, EWhence::End, false, false };
HLVM_INLINE_VAR const FFileSeekCtx GFileSeekCurCtx{ 0, EWhence::Current, false, false };

class IFFileStat
{
public:
	NOCOPYMOVE(IFFileStat)
	IFFileStat() = default;
	virtual ~IFFileStat() = default;

	virtual bool IsDirectory() const = 0;
	virtual bool Exists() const = 0;
	virtual bool IsFile() const = 0;
	virtual bool IsLink() const = 0;
};

class IFileHandle
{
public:
	using OpRetType = IFileHandle&;
	using OpStatusType = FFileOpStatus*;

	NOCOPYMOVE(IFileHandle)
	IFileHandle() = default;
	virtual ~IFileHandle() = default;

	virtual OpRetType Open(const FPath& FilePath, const FFileOptions& Options = GReadOnlyFileOptions) = 0;
	virtual OpRetType Close() = 0;
	virtual OpRetType Read(void* Buffer, size_t Size, const FFileSeekCtx& SeekCtx = GFileSeekCurCtx) = 0;
	virtual OpRetType Write(const void* Buffer, size_t Size, const FFileSeekCtx& SeekCtx = GFileSeekCurCtx) = 0;
	virtual OpRetType Flush() = 0;
	virtual OpRetType Seek(int64_t Offset, EWhence Whence = EWhence::Begin) = 0;
	virtual OpRetType Tell(int64_t& Offset) = 0;
	virtual OpRetType Size(size_t& Size) = 0;

	virtual OpRetType								   Truncate(size_t Size) = 0;
	HLVM_NODISCARD virtual std::shared_ptr<IFFileStat> Stat(const FPath& FilePath) = 0;

	/**
	 * Generic methods
	 */
	HLVM_NODISCARD virtual bool IsOpen() const final
	{
		return mOpened;
	}

	HLVM_NODISCARD virtual FPath GetPath() const final
	{
		return mFilePath;
	}

	HLVM_NODISCARD virtual FFileOptions GetOption() const final
	{
		return mFileOptions;
	}

	HLVM_NODISCARD virtual TVector<TBYTE> GetFileContent(IFileHandle* fileHandle) final
	{
		TVector<TBYTE> TokenData{};
		size_t		   fileSize = 0;
		HLVM_ENSURE(fileHandle->IsOpen(), TXT("file {} is not opened yet"), *fileHandle->GetPath());
		fileHandle->Size(fileSize);
		if (fileSize > 0)
		{
			TokenData.resize(fileSize);
			fileHandle->Read(TokenData.data(), TokenData.size(), { .Offset = 0, .Whence = EWhence::Begin });
		}
		return TokenData;
	}

public:
	operator bool() const noexcept
	{
		return S_C(bool, FileOpStatus);
	}

	FFileOpStatus FileOpStatus;

protected:
	void HandleException(const OpStatusType& Status_InOut, const TCHAR* Function, const std::exception& Exception);
	void HandleException2(const OpStatusType& Status_InOut, const TCHAR* Function);

	FPath		 mFilePath;
	FFileOptions mFileOptions;
	BIT_FLAG(mOpened){ false };
};
