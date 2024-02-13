/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Ultility/Hash.h"
#include "Template/ReferenceTemplate.tpp"

FMD5Hash::MD5Digest::MD5Digest(boost::uuids::detail::md5::digest_type&& data)
{
	static_assert(sizeof(boost::uuids::detail::md5::digest_type) == sizeof(digest), "MD5Digest size is not 32 bytes");
	memmove(digest, &data, sizeof(boost::uuids::detail::md5::digest_type));
}

FString FMD5Hash::MD5Digest::ToString() const
{
	FString		result;
	const char* char_digest = reinterpret_cast<const char*>(&digest);
	boost::algorithm::hex(char_digest, char_digest + sizeof(digest), std::back_inserter(result));
	return result;
}

FMD5Hash::MD5Digest FMD5Hash::Hash(const void* data, size_t size)
{
	boost::uuids::detail::md5 hash;
	hash.process_bytes(data, size);
	boost::uuids::detail::md5::digest_type result;
	hash.get_digest(result);
	return MD5Digest{ MoveTemp(result) };
}
