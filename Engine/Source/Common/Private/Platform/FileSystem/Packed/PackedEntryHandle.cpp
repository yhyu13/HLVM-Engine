/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#include "Platform/FileSystem/Packed/PackedEntryHandle.h"
#include "Platform/FileSystem/Packed/PackedPlatformFile.h"
#include "Core/Log.h"

#include <boost/interprocess/mapped_region.hpp>
#include <magic_enum_all.hpp>

DECLARE_LOG_CATEGORY(LogPackedEntryHandle)

#define PEH_SCOPE_LOCK()

#define PEH_HANDLE_EXCPETIONS() HandleException(Status_InOut, TO_TCHAR_CSTR(__FUNCTION__), Exception)
#define PEH_HANDLE_EXCPETIONS2() HandleException2(Status_InOut, TO_TCHAR_CSTR(__FUNCTION__))

#define PEH_HANDLE_ASSERT(x, ...) HLVM_ASSERT_F(x, TXT("File {} : {}"), *mFilePath, FString::Format(__VA_ARGS__))
#define PEH_HANDLE_ENSURE(x, ...) HLVM_ENSURE_F(x, TXT("File {} : {}"), *mFilePath, FString::Format(__VA_ARGS__))
#define PEH_HANDLE_ENSURE2(x, ...) HLVM_ENSURE_F(x, TXT("File {} : {}"), *FilePath, FString::Format(__VA_ARGS__))
#define PEH_VERBOSE_LOG(...)                                                                                      \
	do                                                                                                            \
	{                                                                                                             \
		if (Status_InOut->bVerbose)                                                                               \
			HLVM_LOG(LogPackedEntryHandle, trace, TXT("File {} : {}"), *mFilePath, FString::Format(__VA_ARGS__)); \
	}                                                                                                             \
	while (0)
#define PEH_VERBOSE_LOG2(...)                                                                                    \
	do                                                                                                           \
	{                                                                                                            \
		if (Status_InOut->bVerbose)                                                                              \
			HLVM_LOG(LogPackedEntryHandle, trace, TXT("File {} : {}"), *FilePath, FString::Format(__VA_ARGS__)); \
	}                                                                                                            \
	while (0)

#define PEH_HANDLE_STATUS(Status) OpStatusType Status = &FileOpStatus

FPackedEntryHandle::~FPackedEntryHandle()
{
	if (mOpened)
	{
		Close();
	}
}

IFileHandle::OpRetType FPackedEntryHandle::Open(const FPath& FilePath, const FFileOptions& Options)
{
	using namespace boost::interprocess;

	PEH_HANDLE_STATUS(Status_InOut);
	PEH_HANDLE_ENSURE(*Status_InOut, TXT("File operation continue with failed status"));
	PEH_HANDLE_ASSERT(!mOpened, TXT("File operation begin with another already open file"));
	PEH_HANDLE_ASSERT(Options.eFileMode == sDefaultEntryOptions.eFileMode, TXT("File option eFileMode invalid {}"), HLVM_E2TCHAR(Options.eFileMode));
	PEH_HANDLE_ASSERT(Options.eFileMapped == sDefaultEntryOptions.eFileMapped, TXT("File option eFileMapped invalid {}"), HLVM_E2TCHAR(Options.eFileMapped));
	PEH_HANDLE_ASSERT(Options.eFileAsync == sDefaultEntryOptions.eFileAsync, TXT("File option eFileAsync invalid {}"), HLVM_E2TCHAR(Options.eFileAsync));
	PEH_HANDLE_ASSERT(Options.eFileLock == sDefaultEntryOptions.eFileLock, TXT("File option eFileLock invalid {}"), HLVM_E2TCHAR(Options.eFileLock));

	mFileOptions = Options;
	mFilePath = FilePath;

	try
	{
		// TODO : detail logging for buffer exchanges
		{
			mQuickFind = FPackedPlatformFile::Get()->QuickFindPackedEntry(mFilePath);

			auto Data = mQuickFind.Data;
			auto Fragment = mQuickFind.Fragment;
			Fragment->Open();
			auto RefCount = mQuickFind.RefCount;
			RefCount->fetch_add(1, std::memory_order_relaxed);
			auto RawBuffer = Fragment->GetSubRegion(*Data);

			// Decryption
			TVector<TBYTE> DecryptedBuffer;
			switch (Data->EncryptType)
			{
				case EEncryptType::RSA_PKCS8:
				{
					auto _Decrypt = FRSA::Decrypt(RawBuffer);
					DecryptedBuffer.resize(_Decrypt.size());
					std::memcpy(DecryptedBuffer.data(), _Decrypt.data(), _Decrypt.size());
				}
				break;
				case EEncryptType::BASE64:
				{
					auto _Decrypt = Botan::base64_decode(
						std::string_view(R_C(const char*, RawBuffer.data()), RawBuffer.size()));
					DecryptedBuffer.resize(_Decrypt.size());
					std::memcpy(DecryptedBuffer.data(), _Decrypt.data(), _Decrypt.size());
				}
				break;
				case EEncryptType::No:
				{
					DecryptedBuffer.resize(RawBuffer.size());
					std::memcpy(DecryptedBuffer.data(), RawBuffer.data(), RawBuffer.size());
				}
				break;
				case EEncryptType::_NUM:
				default:
					HLVM_ENSURE_F(false, TXT("Unknow encrypt type"));
					break;
			}

			// Decompression
			switch (Data->CompressType)
			{
				case ECompressType::ZSTD_1:
				case ECompressType::ZSTD_4:
					mContentBuffer = FZstd::Decompress(DecryptedBuffer);
					break;
				case ECompressType::No:
					mContentBuffer = MoveTemp(DecryptedBuffer);
					break;
				case ECompressType::_NUM:
				default:
					HLVM_ENSURE_F(false, TXT("Unknow compress type"));
					break;
			}
		}
		mOpened = true;
		Status_InOut->eFileOpStatus = EFileOpStatus::Success;
		PEH_VERBOSE_LOG(TXT("Open success with content size {}"), mContentBuffer.size());
	}
	catch (std::exception& Exception)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		PEH_HANDLE_EXCPETIONS();
	}
	catch (...)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		PEH_HANDLE_EXCPETIONS2();
	}

	return *this;
}

IFileHandle::OpRetType FPackedEntryHandle::Close()
{
	using namespace boost::interprocess;

	PEH_HANDLE_STATUS(Status_InOut);
	PEH_HANDLE_ENSURE(*Status_InOut, TXT("File operation continue with failed status"));
	PEH_HANDLE_ASSERT(mOpened, TXT("File operation continue w/o open"));

	try
	{
		PEH_SCOPE_LOCK();

		{
			auto Fragment = mQuickFind.Fragment;
			Fragment->Close();
			auto RefCount = mQuickFind.RefCount;
			RefCount->fetch_sub(1, std::memory_order_relaxed);
			mContentBuffer.clear();
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
		PEH_VERBOSE_LOG(TXT("Close file success"));
	}
	catch (std::exception& Exception)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		PEH_HANDLE_EXCPETIONS();
	}
	catch (...)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		PEH_HANDLE_EXCPETIONS2();
	}

	return *this;
}

IFileHandle::OpRetType FPackedEntryHandle::Read(void*, size_t, const FFileSeekCtx&)
{
	// No point to implement this
	HLVM_NOT_IMPLEMENTED();
	return *this;
}

IFileHandle::OpRetType FPackedEntryHandle::Write(const void*, size_t, const FFileSeekCtx&)
{
	// No point to implement this
	HLVM_NOT_IMPLEMENTED();
	return *this;
}

IFileHandle::OpRetType FPackedEntryHandle::Flush()
{
	// No point to implement this
	HLVM_NOT_IMPLEMENTED();
	return *this;
}

IFileHandle::OpRetType FPackedEntryHandle::Seek(int64_t, EWhence)
{
	// No point to implement this
	HLVM_NOT_IMPLEMENTED();
	return *this;
}

IFileHandle::IFileHandle::OpRetType FPackedEntryHandle::Tell(int64_t&)
{
	// No point to implement this
	HLVM_NOT_IMPLEMENTED();
	return *this;
}

IFileHandle::OpRetType FPackedEntryHandle::Size(size_t&)
{
	// No point to implement this
	HLVM_NOT_IMPLEMENTED();
	return *this;
}

IFileHandle::OpRetType FPackedEntryHandle::Truncate(size_t)
{
	// No point to implement this
	HLVM_NOT_IMPLEMENTED();
	return *this;
}

HLVM_NODISCARD std::shared_ptr<IFFileStat> FPackedEntryHandle::Stat(const FPath&)
{
	// No point to implement this
	HLVM_NOT_IMPLEMENTED();
	return nullptr;
}
