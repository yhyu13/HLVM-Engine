/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Core/Encrypt/RSA.h"
#include "Core/Assert.h"
#include "Utility/ScopedTimer.h"
#include "Platform/FileSystem/Boost/BoostMapFileHandle.h"

DECLARE_LOG_CATEGORY(LogRSA)

constexpr size_t SignatureDigestSize = 4096;

HLVM_NODISCARD std::vector<TUINT8> FRSA::Encrypt(const FConstByteBuffer& Buffer)
{
	using namespace hlvm_private;
	HLVM_SCOPED_TIMER(FString::Format(TXT("RSA Encrypt size {}"), Buffer.size()));
	const bool bValidBuffer = Buffer.size() > 0;
	HLVM_ASSERT(bValidBuffer, TXT("Buffer must has content"));

	Botan::PK_Encryptor_EME enc(**公钥, 随机, 算法一);
	std::vector<TUINT8>	out = enc.encrypt(R_C(const TUINT8*, Buffer.data()), Buffer.size(), 随机);
	const bool				bValidOut = out.size() > 0;
	HLVM_ASSERT(bValidOut, TXT("Out must has content"));

	HLVM_LOG(LogRSA, trace, TXT("PCKS8 Encrypt Buffer size {}, out size {}"), Buffer.size(), out.size());

	return out;
}

HLVM_NODISCARD Botan::secure_vector<TUINT8> FRSA::Decrypt(const FConstByteBuffer& Buffer)
{
	using namespace hlvm_private;
	HLVM_SCOPED_TIMER(FString::Format(TXT("RSA Decrypt size {}"), Buffer.size()));
	const bool bValidBuffer = Buffer.size() > 0;
	HLVM_ASSERT(bValidBuffer, TXT("Buffer must has content"));

	Botan::PK_Decryptor_EME		  dec(**私钥, 随机, 算法一);
	Botan::secure_vector<TUINT8> out = dec.decrypt(R_C(const TUINT8*, Buffer.data()), Buffer.size());
	const bool					  bValidOut = out.size() > 0;
	HLVM_ASSERT(bValidOut, TXT("Out must has content"));

	HLVM_LOG(LogRSA, trace, TXT("PCKS8 Decrypt Buffer size {}, out size {}"), Buffer.size(), out.size());

	return out;
}

static void digest_buffer(const FConstByteBuffer& Buffer, std::span<TUINT8>& output_buffer)
{
	using namespace hlvm_private;
	auto   input_buffer = R_C(const TUINT8*, Buffer.data());
	auto   input_len = Buffer.size();
	size_t per_offset = input_len / SignatureDigestSize + (input_len % SignatureDigestSize == 0 ? 0 : 1);

	size_t j = 0;
	for (size_t i = 0; i < input_len && j < SignatureDigestSize; i += per_offset, ++j)
	{
		output_buffer[j] = input_buffer[i];
	}
}

void FRSA::SignToFile(const FConstByteBuffer& Buffer, const FPath& signature_path)
{
	using namespace hlvm_private;
	HLVM_SCOPED_TIMER(FString::Format(TXT("RSA SignToFile size {} path {}"), Buffer.size(), *signature_path));
	const bool bValidBuffer = Buffer.size() > 0;
	HLVM_ASSERT(bValidBuffer, TXT("Buffer must has content"));

	/**
	 *    1. Digest the digestBuffer
	 */
	TUINT8 digest[SignatureDigestSize] = { 0 };
	auto	digestBuffer = TO_SPAN(digest, SignatureDigestSize);
	digest_buffer(Buffer, digestBuffer);
	Botan::PK_Signer	 签名(**私钥, 随机, 算法二);
	std::vector<TUINT8> signature = 签名.sign_message(digestBuffer, 随机);
	const bool			 bValidSig = signature.size() > 0;
	HLVM_ASSERT(bValidSig, TXT("signature must has content"));

	/**
	 *   2. Encode the signature
	 */
	auto			 base64 = Botan::base64_encode(signature);
	FBoostMapFileHandle fileHandle;
	FFileOptions	 Options{ .eFileMode = EFileMode::W, .eFileMapped = EFileMapped::Mapped, .eFileLock = EFileLock::FullLock };
	fileHandle.Open(signature_path, Options)
		.Write(base64.data(), base64.size());
}

void FRSA::SignToFile(const FPath& FilePath, const FPath& signature_path)
{
	FBoostMapFileHandle fileHandle;
	FFileOptions	 Options{ .eFileMode = EFileMode::RB, .eFileMapped = EFileMapped::Mapped, .eFileLock = EFileLock::FullLock };
	fileHandle.Open(FilePath, Options);
	auto Buffer = fileHandle.GetMappedBufferReadOnly();
	SignToFile(Buffer, signature_path);
}

HLVM_NODISCARD bool FRSA::VerifyFileSignature(const FConstByteBuffer& Buffer, const FPath& signature_path)
{
	using namespace hlvm_private;
	HLVM_SCOPED_TIMER(FString::Format(TXT("RSA VerifyFromFile size {} path {}"), Buffer.size(), *signature_path));
	const bool bValidBuffer = Buffer.size() > 0;
	HLVM_ASSERT(bValidBuffer, TXT("Buffer must has content"));

	TUINT8 digest[SignatureDigestSize] = { 0 };
	auto	digestBuffer = TO_SPAN(digest, SignatureDigestSize);
	digest_buffer(Buffer, digestBuffer);
	{
		/**
		 *  1. Read the signature
		 */
		FBoostMapFileHandle  signatureHandle;
		FFileOptions	  sigOptions{ .eFileMode = EFileMode::R, .eFileMapped = EFileMapped::Mapped, .eFileLock = EFileLock::FullLock };
		std::vector<char> base64_signature;
		size_t			  sigSize;
		signatureHandle.Open(signature_path, sigOptions).Size(sigSize);
		base64_signature.resize(sigSize);
		signatureHandle.Read(base64_signature.data(), base64_signature.size());

		/**
		 *  2. Decode the signature
		 */
		Botan::secure_vector<TUINT8> signature = Botan::base64_decode(base64_signature.data(), base64_signature.size(),
			false);
		const bool					  bValidSig = signature.size() > 0;
		HLVM_ASSERT(bValidSig, TXT("signature must has content"));

		Botan::PK_Verifier 验证(**公钥, 算法二);
		return 验证.verify_message(digestBuffer, signature);
	}
};

HLVM_NODISCARD bool FRSA::VerifyFileSignature(const FPath& FilePath, const FPath& signature_path)
{
	FBoostMapFileHandle fileHandle;
	FFileOptions	 Options{ .eFileMode = EFileMode::RB, .eFileMapped = EFileMapped::Mapped, .eFileLock = EFileLock::FullLock };
	fileHandle.Open(FilePath, Options);
	auto Buffer = fileHandle.GetMappedBufferReadOnly();
	return VerifyFileSignature(Buffer, signature_path);
};
