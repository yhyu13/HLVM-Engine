/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Platform/FileSystem/Packed/PackedFileHandle.h"
#include "Platform/FileSystem/Boost/BoostFileHandle.h"
#include "Core/Log.h"

#include <magic_enum_all.hpp>

DELCARE_LOG_CATEGORY(LogPackedFileHandle)
DEFINE_LOG_CATEGORY(LogPackedFileHandle)

#define PFH_SCOPE_LOCK()

#define PFH_HANDLE_EXCPETIONS() HandleException(Status_InOut, TO_TCHAR_STR(__FUNCTION__), Exception)
#define PFH_HANDLE_EXCPETIONS2() HandleException2(Status_InOut, TO_TCHAR_STR(__FUNCTION__))

#define PFH_HANDLE_ASSERT(x, ...) HLVM_ASSERT(x, TXT("File {} : {}"), *mFilePath, FString::Format(__VA_ARGS__))
#define PFH_HANDLE_ENSURE(x, ...) HLVM_ENSURE(x, TXT("File {} : {}"), *mFilePath, FString::Format(__VA_ARGS__))
#define PFH_HANDLE_ENSURE2(x, ...) HLVM_ENSURE(x, TXT("File {} : {}"), *FilePath, FString::Format(__VA_ARGS__))
#define PFH_VERBOSE_LOG(...)                                                                                     \
	do                                                                                                           \
	{                                                                                                            \
		if (Status_InOut->bVerbose)                                                                              \
			HLVM_LOG(LogPackedFileHandle, trace, TXT("File {} : {}"), *mFilePath, FString::Format(__VA_ARGS__)); \
	}                                                                                                            \
	while (0)
#define PFH_VERBOSE_LOG2(...)                                                                                   \
	do                                                                                                          \
	{                                                                                                           \
		if (Status_InOut->bVerbose)                                                                             \
			HLVM_LOG(LogPackedFileHandle, trace, TXT("File {} : {}"), *FilePath, FString::Format(__VA_ARGS__)); \
	}                                                                                                           \
	while (0)

#define PFH_HANDLE_STATUS(Status) OpStatusType Status = &FileOpStatus

FPackedFileHandle::~FPackedFileHandle()
{
	if (mOpened)
	{
		Close();
	}
}

IFileHandle::OpRetType FPackedFileHandle::Open(const FPath& FilePath, const FFileOptions&)
{
	PFH_HANDLE_STATUS(Status_InOut);
	PFH_HANDLE_ENSURE(*Status_InOut, TXT("File operation continue with failed status"));
	PFH_HANDLE_ASSERT(!mOpened, TXT("File operation begin with another already open file"));

	mFileOptions = sDefaultFileOptions;
	mFilePath = FilePath;
	const bool _noExtension = !mFilePath.has_extension();
	PFH_HANDLE_ASSERT(_noExtension, TXT("Packed file path input should not have extension"));
	if (boost::regex_search(mFilePath.ToCharStr(), HLVM_PACKED_PATCH_FILE_PATTERN))
	{
		mPackedFileType = EPackedFileType::Patch;
	}
	else if (boost::regex_search(mFilePath.ToCharStr(), HLVM_PACKED_FILE_PATTERN))
	{
		mPackedFileType = EPackedFileType::Base;
	}
	else
	{
		PFH_HANDLE_ENSURE(false, TXT("Packed file path does not match any pack file type"));
	}

	{
		PFH_VERBOSE_LOG(TXT("Create Mapped file"));
	}

	try
	{
		{
			// TODO
			// Validate token and container file signature
			// Decompress and read and build all token entries (async?)
			{
				auto TokenFilePath = mFilePath.ChangeExtension(HLVM_PACKED_TOKEN_EXT);

				const bool exist = FPath::Exists(TokenFilePath);
				PFH_HANDLE_ASSERT(exist, TXT("Packed token file does not exist"));

				FBoostFileHandle fileHandle;
				size_t			 fileSize = 0;
				fileHandle.Open(TokenFilePath, mFileOptions)
					.Size(fileSize);
				// Read binary
				PFH_HANDLE_ASSERT(fileSize > 0, TXT("Packed token file size invalid {}"), fileSize);
				TVector<std::byte> TokenData{ fileSize };
				fileHandle.Read(TokenData.data(), TokenData.size(), { .Offset = 0, .Whence = EWhence::Begin })
					.Close();

				// Decryption & Decompression
				{
					// Actually do not encrypt the token file as it cost too much time, 10x slower
					// auto Decrypted = FRSA::PCKS8_Decrypt(TokenData);
					auto& Decrypted = TokenData;
					auto  Decompressed = FZstd::Decompress({ R_C(std::byte*, Decrypted.data()), Decrypted.size() });

					const char* lineSeparator = TO_CHAR_STR(HLVM_JSONL_LINE_SEPARATOR);
					const char* lineStart = R_C(const char*, Decompressed.data());
					const char* lineEnd = lineStart;
					const char* tokenDataEnd = lineStart + Decompressed.size();

					while (lineEnd < tokenDataEnd)
					{
						if (*lineEnd == lineSeparator[0] && *(lineEnd + 1) == lineSeparator[1] && *(lineEnd + 2) == lineSeparator[2])
						{
							std::cout << "Extracted line: " << std::string(lineStart, lineEnd) << std::endl;

							FPackedTokenEntry Entry;
							bool			  bSuccess = SetSerialized(Entry, std::span<const std::byte>{ R_C(const std::byte*, lineStart), S_C(size_t, lineEnd - lineStart) });
							assert(bSuccess);
							std::cout << "Entry: " << Entry.PathHash << std::endl;

							lineStart = lineEnd + 3;
							lineEnd = lineStart;
						}
						else
						{
							++lineEnd;
						}
					}
					// Print the last line
					if (lineStart != lineEnd)
					{
						std::cout << "Extracted line: " << std::string(lineStart, lineEnd) << std::endl;
					}
				}
			}
			auto ContainerFilePath = mFilePath.ChangeExtension(HLVM_PACKED_CONTAINER_EXT);
		}

		mOpened = true;
		Status_InOut->eFileOpStatus = EFileOpStatus::Success;
		PFH_VERBOSE_LOG(TXT("Open success with mode {}"), TO_TCHAR_STR(magic_enum::enum_name(mFileOptions.eFileMode).data()));
	}
	catch (std::exception& Exception)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		PFH_HANDLE_EXCPETIONS();
	}
	catch (...)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		PFH_HANDLE_EXCPETIONS2();
	}

	return *this;
}

IFileHandle::OpRetType FPackedFileHandle::Close()
{
	PFH_HANDLE_STATUS(Status_InOut);
	PFH_HANDLE_ENSURE(*Status_InOut, TXT("File operation continue with failed status"));
	PFH_HANDLE_ASSERT(mOpened, TXT("File operation continue w/o open"));

	try
	{
		PFH_SCOPE_LOCK();

		{
			mContainerMappedFile.close();
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
		PFH_VERBOSE_LOG(TXT("Close file success"));
	}
	catch (std::exception& Exception)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		PFH_HANDLE_EXCPETIONS();
	}
	catch (...)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		PFH_HANDLE_EXCPETIONS2();
	}

	return *this;
}

IFileHandle::OpRetType FPackedFileHandle::Read(void* Buffer, size_t Size, const FFileSeekCtx& SeekCtx)
{
	PFH_HANDLE_STATUS(Status_InOut);
	PFH_HANDLE_ENSURE(*Status_InOut, TXT("File operation continue with failed status"));
	PFH_HANDLE_ASSERT(mOpened, TXT("File operation continue w/o open"));
	PFH_HANDLE_ASSERT(mFileOptions.eFileMode & EFileMode::R, TXT("File operation cannot read"));
	PFH_HANDLE_ASSERT(Size > 0, TXT("Buffer size invalid {}"), Size);

	int64_t mContainerMappedSeekPos = SeekCtx.Offset;
	// validate seek
	PFH_HANDLE_ASSERT(SeekCtx.Whence == sDefaultFileSeekCtx.Whence, TXT("Seek ctx whence invalid {}"), TO_TCHAR_STR(magic_enum::enum_name(SeekCtx.Whence).data()));
	PFH_HANDLE_ASSERT(SeekCtx.bResetPos == sDefaultFileSeekCtx.bResetPos, TXT("Seek ctx bResetPos invalid {}"), SeekCtx.bResetPos);
	PFH_HANDLE_ASSERT(SeekCtx.bEraseSeekPos == sDefaultFileSeekCtx.bEraseSeekPos, TXT("Seek ctx bEraseSeekPos invalid {}"), SeekCtx.bEraseSeekPos);

	try
	{
		PFH_SCOPE_LOCK();

		{
			size_t	FileSize = mContainerMappedFile.size();
			int64_t rest_size = static_cast<int64_t>(FileSize) - (mContainerMappedSeekPos);
			// Check space avilable for reading
			const bool available = rest_size >= static_cast<int64_t>(Size);
			PFH_HANDLE_ASSERT(available, TXT("mMappedFile size is not enough for read. SeekPos {}, File Size {}, available size {} Buffer Size {}"),
				mContainerMappedSeekPos, FileSize, rest_size, Size);

			auto readPos = MappedFileCurPos_R(mContainerMappedSeekPos);
			// Check buffer and mmap no overlapping
			const bool overlap = IsPointerOverlap(Buffer, Size, readPos, static_cast<size_t>(rest_size));
			PFH_HANDLE_ASSERT(!overlap, TXT("mMappedFile overlap with read region. SeekPos {}, File Size {}, available size {} Buffer Size {}"),
				mContainerMappedSeekPos, FileSize, rest_size, Size);

			// do the mmap
			std::memcpy(Buffer, readPos, Size);
		}

		if (Status_InOut->bCancelByUser) [[unlikely]]
		{
			Status_InOut->eFileOpStatus = EFileOpStatus::Canceled;
		}
		else [[likely]]
		{
			Status_InOut->eFileOpStatus = EFileOpStatus::Success;
		}
		PFH_VERBOSE_LOG(TXT("Read file success with {} bytes"), Size);
	}
	catch (std::exception& Exception)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		PFH_HANDLE_EXCPETIONS();
	}
	catch (...)
	{
		Status_InOut->eFileOpStatus = EFileOpStatus::Failed;
		PFH_HANDLE_EXCPETIONS2();
	}

	return *this;
}

IFileHandle::OpRetType FPackedFileHandle::Write(const void*, size_t, const FFileSeekCtx&)
{
	// No point to implement this
	HLVM_NOT_IMPLEMENTED();
	return *this;
}

IFileHandle::OpRetType FPackedFileHandle::Flush()
{
	// No point to implement this
	HLVM_NOT_IMPLEMENTED();
	return *this;
}

IFileHandle::OpRetType FPackedFileHandle::Seek(int64_t, EWhence)
{
	// No point to implement this
	HLVM_NOT_IMPLEMENTED();
	return *this;
}

IFileHandle::IFileHandle::OpRetType FPackedFileHandle::Tell(int64_t&)
{
	// No point to implement this
	HLVM_NOT_IMPLEMENTED();
	return *this;
}

IFileHandle::OpRetType FPackedFileHandle::Size(size_t&)
{
	// No point to implement this
	HLVM_NOT_IMPLEMENTED();
	return *this;
}

IFileHandle::OpRetType FPackedFileHandle::Truncate(size_t)
{
	// No point to implement this
	HLVM_NOT_IMPLEMENTED();
	return *this;
}

std::shared_ptr<IFFileStat> FPackedFileHandle::Stat(const FPath&)
{
	// No point to implement this
	HLVM_NOT_IMPLEMENTED();
	return nullptr;
}

const void* FPackedFileHandle::MappedFileCurPos_R(int64_t Offset) const
{
	auto _Offeset = Offset;
	auto _Size = static_cast<int64_t>(mContainerMappedFile.size());
	PFH_HANDLE_ASSERT(_Offeset >= 0 && _Offeset < _Size, FString::Format(TXT("MappedFileCurPos_R {} out of range [0,{})"), _Offeset, _Size));
	return (&(mContainerMappedFile.const_data()[_Offeset]));
}

bool GetSerialized(const FPackedTokenEntry& Data, std::span<std::byte>& Buffer)
{
	const bool bValid = Buffer.size() == FPackedTokenEntry_SerializedSize;
	HLVM_ASSERT(bValid, TXT("Buffer size {} is not enough for serialized data size {}"), Buffer.size(), FPackedTokenEntry_SerializedSize);
	return std::memcpy(Buffer.data(), &Data, FPackedTokenEntry_SerializedSize) == Buffer.data();
}

bool SetSerialized(FPackedTokenEntry& Data, const std::span<const std::byte>& Buffer)
{
	const bool bValid = Buffer.size() == FPackedTokenEntry_SerializedSize;
	HLVM_ASSERT(bValid, TXT("Buffer size {} is not enough for serialized data size {}"), Buffer.size(), FPackedTokenEntry_SerializedSize);
	return std::memcpy(&Data, Buffer.data(), FPackedTokenEntry_SerializedSize) == &Data;
}
