/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#include "Utility/Hash.h"
#include "Template/ReferenceTemplate.tpp"

FMD5Digest::FMD5Digest(boost::uuids::detail::md5::digest_type&& data)
{
	static_assert(sizeof(boost::uuids::detail::md5::digest_type) == sizeof(digest), "MD5Digest size is not 16 bytes");
	std::memmove(digest, &data, sizeof(boost::uuids::detail::md5::digest_type));
}

FString FMD5Digest::ToString() const
{
	FString		result;
	const char* char_digest = reinterpret_cast<const char*>(&digest);
	boost::algorithm::hex(char_digest, char_digest + sizeof(digest), std::back_inserter(result));
	return result;
}

bool FMD5Digest::Valid() const
{
	TBYTE digest2[16];
	FMemory::MemzeroArray(&digest2);
	return FMemory::Memcmp(digest, digest2, sizeof(digest)) != 0;
}

// Function to mix two MD5 hashes
HLVM_STATIC_FUNC boost::uuids::detail::md5 mix_md5_hashes(
	const boost::uuids::detail::md5::digest_type* hash1,
	const boost::uuids::detail::md5::digest_type* hash2)
{
	boost::uuids::detail::md5 md5;
	// Cast digest_type (array of uint32_t) to char* for processing
	const char* data1 = reinterpret_cast<const char*>(*hash1);
	const char* data2 = reinterpret_cast<const char*>(*hash2);

	md5.process_bytes(data1, sizeof(boost::uuids::detail::md5::digest_type));
	md5.process_bytes(data2, sizeof(boost::uuids::detail::md5::digest_type));

	return md5;
}

FMD5Digest FMD5Hash::Hash(const void* data, size_t size, FMD5Digest* prevHash)
{
	boost::uuids::detail::md5 hash;
	hash.process_bytes(data, size);
	boost::uuids::detail::md5::digest_type result;
	hash.get_digest(result);
	// Mix hash if possible
	if (prevHash)
	{
		hash = mix_md5_hashes(&result, reinterpret_cast<boost::uuids::detail::md5::digest_type*>(&prevHash->digest));
		hash.get_digest(result);
	}
	return FMD5Digest{ MoveTemp(result) };
}

FSHA1Digest::FSHA1Digest(boost::uuids::detail::sha1::digest_type&& data)
{
	static_assert(sizeof(boost::uuids::detail::sha1::digest_type) == sizeof(digest), "SHA1Digest size is not 20 bytes");
	std::memmove(digest, &data, sizeof(boost::uuids::detail::sha1::digest_type));
}

FString FSHA1Digest::ToString() const
{
	FString		result;
	const char* char_digest = reinterpret_cast<const char*>(&digest);
	boost::algorithm::hex(char_digest, char_digest + sizeof(digest), std::back_inserter(result));
	return result;
}

FSHA1Digest FSHA1Hash::Hash(const void* data, size_t size)
{
	boost::uuids::detail::sha1 hash;
	hash.process_bytes(data, size);
	boost::uuids::detail::sha1::digest_type result;
	hash.get_digest(result);
	return FSHA1Digest{ MoveTemp(result) };
}
