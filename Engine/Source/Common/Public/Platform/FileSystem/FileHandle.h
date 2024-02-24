/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once
#include "FileSystemDefinition.h"
#include "Template/PointerTemplate.tpp"
#include "Path.h"

/**
 * @brief 文件打开模式，和python类似
 */
enum class EFileMode : uint8_t
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
};

static bool operator&(EFileMode a, EFileMode b)
{
	return static_cast<uint8_t>(a) & static_cast<uint8_t>(b);
}

static bool operator==(EFileMode a, EFileMode b)
{
	return static_cast<uint8_t>(a) == static_cast<uint8_t>(b);
}

static bool operator!=(EFileMode a, EFileMode b)
{
	return static_cast<uint8_t>(a) != static_cast<uint8_t>(b);
}

enum class EFileMapped : uint8_t
{
	NoMapped = 0,
	Mapped = 1,
};

enum class EFileAsync : uint8_t
{
	NoAsync = 0,
	Async = 1,
};

enum class EFileLock : uint8_t
{
	NoLock = 0,
	ThreadLock = 1 << 0,
	InterProcessLock = 1 << 1,
	FullLock = ThreadLock | InterProcessLock
};

static bool operator&(EFileLock a, EFileLock b)
{
	return static_cast<uint8_t>(a) & static_cast<uint8_t>(b);
}

/**
 * @brief 文件操作选项
 */
struct FFileOptions
{
	EFileMode	eFileMode{ EFileMode::R };
	EFileMapped eFileMapped{ EFileMapped::Mapped };
	EFileAsync	eFileAsync{ EFileAsync::NoAsync };
	EFileLock	eFileLock{ EFileLock::NoLock };
};

/**
 * @brief 文件操作状态, report by underlying file system
 */
enum class EFileOpStatus : uint8_t
{
	Waiting = 0,
	Success = 1,
	Failed = 2,
	Canceled = 3,
};

enum class EFileOpErrorNo : uint8_t
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

enum class EWhence : uint8_t
{
	Begin = std::ios::beg,
	Current = std::ios::cur,
	End = std::ios::end,
};

static bool operator&(EWhence a, EWhence b)
{
	return static_cast<uint8_t>(a) == static_cast<uint8_t>(b);
}

struct FFileSeekCtx
{
	int64_t Offset{ 0 };
	EWhence Whence{ EWhence::Current };
	BIT_FLAG(bResetPos){ false };	  // Reset seek pos to location before (offset, whence) is applied. This will override bEraseSeekPos
	BIT_FLAG(bEraseSeekPos){ false }; // Reset seek pos to (offset, whence) right before r/w. Otherwise, advance file pointer

	/**
	 * see if non trivial seek, which requires extra calling seek
	 */
	bool NonTrivialSeek() const
	{
		return !(Offset == 0 && Whence == EWhence::Current);
	}
};

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

	virtual OpRetType Open(const FPath& FilePath, const FFileOptions& Options = FFileOptions()) = 0;
	virtual OpRetType Close() = 0;
	virtual OpRetType Read(void* Buffer, size_t Size, const FFileSeekCtx& SeekCtx = FFileSeekCtx()) = 0;
	virtual OpRetType Write(const void* Buffer, size_t Size, const FFileSeekCtx& SeekCtx = FFileSeekCtx()) = 0;
	virtual OpRetType Flush() = 0;
	virtual OpRetType Seek(int64_t Offset, EWhence Whence = EWhence::Begin) = 0;
	virtual OpRetType Tell(int64_t& Offset) = 0;
	virtual OpRetType Size(size_t& Size) = 0;

	/**
	 * These methods can be static methods, but since we require inheritance, they have to be member virtual methods
	 */
	virtual OpRetType								  Truncate(size_t Size) = 0;
	[[nodiscard]] virtual std::shared_ptr<IFFileStat> Stat(const FPath& FilePath) = 0;

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
