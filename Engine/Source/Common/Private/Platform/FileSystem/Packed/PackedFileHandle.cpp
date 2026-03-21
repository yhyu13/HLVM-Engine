/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#include "Platform/FileSystem/Packed/PackedFileHandle.h"
#include "Platform/FileSystem/Boost/BoostMapFileHandle.h"
#include "Core/Parallel/Async/_Deprecated/WorkStealFiberPool.h"
#include "Core/Log.h"

#include <boost/interprocess/mapped_region.hpp>
#include <magic_enum_all.hpp>

DECLARE_LOG_CATEGORY(LogPackedFileHandle)

#define PFH_SCOPE_LOCK()

#define PFH_HANDLE_EXCPETIONS() HandleException(Status_InOut, TO_TCHAR_CSTR(__FUNCTION__), Exception)
#define PFH_HANDLE_EXCPETIONS2() HandleException2(Status_InOut, TO_TCHAR_CSTR(__FUNCTION__))

#define PFH_HANDLE_ASSERT(x, ...) HLVM_ASSERT_F(x, TXT("File {} : {}"), *mFilePath, FString::Format(__VA_ARGS__))
#define PFH_HANDLE_ENSURE(x, ...) HLVM_ENSURE_F(x, TXT("File {} : {}"), *mFilePath, FString::Format(__VA_ARGS__))
#define PFH_HANDLE_ENSURE2(x, ...) HLVM_ENSURE_F(x, TXT("File {} : {}"), *FilePath, FString::Format(__VA_ARGS__))
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

IFileHandle::OpRetType FPackedFileHandle::Open(const FPath& FilePath, const FFileOptions& Options)
{
	using namespace boost::interprocess;

	PFH_HANDLE_STATUS(Status_InOut);
	PFH_HANDLE_ENSURE(*Status_InOut, TXT("File operation continue with failed status"));
	PFH_HANDLE_ASSERT(!mOpened, TXT("File operation begin with another already open file"));
	PFH_HANDLE_ASSERT(Options.eFileMode == sDefaultFileOptions.eFileMode, TXT("File option eFileMode invalid {}"), E2TCHAR(Options.eFileMode));
	PFH_HANDLE_ASSERT(Options.eFileMapped == sDefaultFileOptions.eFileMapped, TXT("File option eFileMapped invalid {}"), E2TCHAR(Options.eFileMapped));
	PFH_HANDLE_ASSERT(Options.eFileAsync == sDefaultFileOptions.eFileAsync, TXT("File option eFileAsync invalid {}"), E2TCHAR(Options.eFileAsync));
	PFH_HANDLE_ASSERT(Options.eFileLock == sDefaultFileOptions.eFileLock, TXT("File option eFileLock invalid {}"), E2TCHAR(Options.eFileLock));

	mFileOptions = Options;
	mFilePath = FilePath;
	const bool _noExtension = !mFilePath.has_extension();
	PFH_HANDLE_ASSERT(_noExtension, TXT("Packed file path input should not have extension"));
	if (std::regex_search(mFilePath.ToCharCStr(), HLVM_PACKED_PATCH_FILE_MATCH_PATTERN))
	{
		mPackedFileType = EPackedFileType::Patch;
		std::smatch matches;
		std::regex_match(mFilePath.string(), matches, HLVM_PACKED_PATCH_FILE_MATCH_PATTERN);
		const bool bValid = matches.size() == 2;
		PFH_HANDLE_ASSERT(bValid, TXT("Patch regex matching failed with wrong size {}"), matches.size());
		try
		{
			// Find mount order from patch file name, e.g. packed-456790-pat, where 456790 is the patch CL
			mMountOrder = std::stoull(matches[1]);
			PFH_VERBOSE_LOG(TXT("Mount order: {}"), mMountOrder);
		}
		catch (const std::invalid_argument& e)
		{
			PFH_HANDLE_ENSURE(false, TXT("Invalid input: {}"), TO_TCHAR_CSTR(e.what()));
		}
	}
	else if (std::regex_search(mFilePath.ToCharCStr(), HLVM_PACKED_FILE_MATCH_PATTERN))
	{
		mPackedFileType = EPackedFileType::Base;
	}
	else
	{
		PFH_HANDLE_ASSERT(false, TXT("Packed file path does not match any pack file type"));
	}

	{
		PFH_VERBOSE_LOG(TXT("Create Mapped file"));
	}

	try
	{
		{
			auto cot_job = std::thread(
				[&]() {
					// Open container file with mmap
					{
						// Exist container file
						auto	   ContainerFilePath = mFilePath.ChangeExtension(HLVM_PACKED_CONTAINER_FILE_EXT);
						const bool exist = FPath::Exists(ContainerFilePath);
						PFH_HANDLE_ENSURE(exist, TXT("Packed container file does not exist"));

#if HLVM_PACKED_FILE_WITH_SIGNATURE
						// Verify container file signature
						PFH_HANDLE_ENSURE(FRSA::VerifyFileSignature(ContainerFilePath, ContainerFilePath.AppendExtension(HLVM_RSA_SIGNATURE_EXT)),
							TXT("Packed container file signature verification failed"));
#endif

						// Lock container file
						file_lock _Lock(ContainerFilePath);
						mContainerFileLock = MoveTemp(sharable_lock<file_lock>(_Lock));

						// Open container file
						mContainerMappedFile = MoveTemp(file_mapping(ContainerFilePath.ToCharCStr(), read_only));
						PFH_HANDLE_ENSURE(mContainerMappedFile.get_mapping_handle().handle, TXT("MappedFile file open failed"));
						PFH_VERBOSE_LOG(TXT("Container file opened {}"), *ContainerFilePath);
					}
				});
			HLVM_SCOPED_VARIABLE(
				ScopedCotJob, [] {}, [&] { cot_job.join(); });

			// Decompress and read and build all token entries
			{
				// Exists Token file path
				FPath	   TokenFilePath = mFilePath.ChangeExtension(HLVM_PACKED_TOKEN_FILE_EXT);
				const bool exist = FPath::Exists(TokenFilePath);
				PFH_HANDLE_ENSURE(exist, TXT("Packed token file does not exist"));

				// Lock token file
				file_lock _Lock(TokenFilePath);
				mTokenFileLock = MoveTemp(sharable_lock<file_lock>(_Lock));

				// Open token file
				FBoostMapFileHandle fileHandle;
				size_t				fileSize = 0;
				fileHandle.Open(TokenFilePath, mFileOptions)
					.Size(fileSize);
				// Read token file in 1 shot
				PFH_HANDLE_ENSURE(fileSize > 0, TXT("Packed token file size invalid {}"), fileSize);
				TVector<TBYTE> TokenData{ fileSize };
				fileHandle.Read(TokenData.data(), TokenData.size(), { .Offset = 0, .Whence = EWhence::Begin });

#if HLVM_PACKED_FILE_WITH_SIGNATURE
				// Verify Token file signature
				PFH_HANDLE_ENSURE(FRSA::VerifyFileSignature(TokenData, TokenFilePath.AppendExtension(HLVM_RSA_SIGNATURE_EXT)),
					TXT("Packed token file signature verification failed"));
#endif
				// Decryption & Decompression
				{
#if HLVM_PACKED_TOKEN_FILE_WITH_ENCRYPTION
					const auto Decrypted = FRSA::Decrypt(TokenData);
#else
					const auto& Decrypted = TokenData;
#endif
					const auto Decompressed = FZstd::Decompress({ R_C(const TBYTE*, Decrypted.data()), Decrypted.size() });

					/**
					 * Deserialize token entry from file buffer
					 */
					const TBYTE* lineStart = Decompressed.data();
					const TBYTE* lineEnd = lineStart + FPackedTokenEntry_SerializedSize;
					const TBYTE* tokenDataEnd = lineStart + Decompressed.size();

					size_t Num = 0;
					auto   ExtractTokenEntry = [&](FPackedTokenEntry& Entry) {
						  bool bSuccess = Entry.Deserialize(FConstByteBuffer{ lineStart, S_C(size_t, lineEnd - lineStart) });
						  PFH_HANDLE_ENSURE(bSuccess, TXT("Failed to deserialize entry #{}"), Num);
						  PFH_VERBOSE_LOG(TXT("Entry #{}:\n{}"), Num, TO_TCHAR_CSTR(Entry.ToJsonString().c_str()));
						  ++Num;
					};

					// Init containers
					mContainerFragments.clear();
					mContainerFragments.emplace_back(MoveTemp(FPackedContainerFragment()));
					mTokenEntryFragmentMap.clear();

					// Iterate through token entries
					bool bCurrentFragmentInit = false;
					while (lineEnd <= tokenDataEnd)
					{
						FPackedTokenEntry Entry;
						ExtractTokenEntry(Entry);

						// Init fragment if necessary
						FPackedContainerFragment& CurrentFragment = mContainerFragments.back();
						if (!bCurrentFragmentInit)
						{
							bCurrentFragmentInit = true;
							CurrentFragment.FragmentStartPos = Entry.Data.StartPos;
							CurrentFragment.ContainerFileMapping = &mContainerMappedFile;
						}

						// Fragment full, initiate another fragment
						if ((CurrentFragment.FragmentSize += Entry.Data.Size) >= FPackedContainerFragment::sSuggestedFragmentSize)
						{
							mContainerFragments.emplace_back(MoveTemp(FPackedContainerFragment()));
							bCurrentFragmentInit = false;
						}

						// Insert token entry
						{
							auto CurrentFragmentID = mContainerFragments.size() - 1;
							auto result = mTokenEntryFragmentMap.insert_or_assign(MoveTemp(Entry.PathHash), { MoveTemp(Entry.Data), S_C(uint32_t, CurrentFragmentID) });
							PFH_HANDLE_ENSURE(result.second, TXT("Key already exists, value updated from {} to {}"), R_C(uintptr_t, lineStart), R_C(uintptr_t, lineEnd));
						}
						lineStart = lineEnd;
						lineEnd = lineStart + FPackedTokenEntry_SerializedSize;
					}

					// Sanity check on we reach finish correctly
					PFH_HANDLE_ENSURE(lineStart == tokenDataEnd, TXT("Token data end not reached lineStart {} tokenDataEnd {}"), R_C(uintptr_t, lineStart), R_C(uintptr_t, tokenDataEnd));
					PFH_VERBOSE_LOG(TXT("TokenEntryMap elements: {}"), mTokenEntryFragmentMap.size());
					mContainerFragments.shrink_to_fit();
					PFH_VERBOSE_LOG(TXT("ContainerFragments elements: {}"), mContainerFragments.size());
				}
			}
		}

		mOpened = true;
		Status_InOut->eFileOpStatus = EFileOpStatus::Success;
		PFH_VERBOSE_LOG(TXT("Open success with mode {}"), E2TCHAR(mFileOptions.eFileMode));
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
	using namespace boost::interprocess;

	PFH_HANDLE_STATUS(Status_InOut);
	PFH_HANDLE_ENSURE(*Status_InOut, TXT("File operation continue with failed status"));
	PFH_HANDLE_ASSERT(mOpened, TXT("File operation continue w/o open"));

	try
	{
		PFH_SCOPE_LOCK();

		{
			mContainerFragments.clear();
			mTokenEntryFragmentMap.clear();
			{
				// Using swap dummy to unmap file on dummy destruction
				auto Dummy = file_mapping();
				mContainerMappedFile.swap(Dummy);
			}
			mTokenFileLock.release();
			mContainerFileLock.release();
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

IFileHandle::OpRetType FPackedFileHandle::Read(void*, size_t, const FFileSeekCtx&)
{
	// No point to implement this
	HLVM_NOT_IMPLEMENTED();
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

HLVM_NODISCARD std::shared_ptr<IFFileStat> FPackedFileHandle::Stat(const FPath&)
{
	// No point to implement this
	HLVM_NOT_IMPLEMENTED();
	return nullptr;
}
