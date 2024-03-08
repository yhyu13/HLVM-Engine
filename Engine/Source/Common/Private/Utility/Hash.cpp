/**
 * Copyright (c) 2024. MIT License. All rights reserved.
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

FMD5Digest FMD5Hash::Hash(const void* data, size_t size)
{
	boost::uuids::detail::md5 hash;
	hash.process_bytes(data, size);
	boost::uuids::detail::md5::digest_type result;
	hash.get_digest(result);
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
