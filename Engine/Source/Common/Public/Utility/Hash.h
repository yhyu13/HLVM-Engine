/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "Core/String.h"

#include <boost/algorithm/hex.hpp>
#include <boost/uuid/detail/md5.hpp>
#include <boost/uuid/detail/sha1.hpp>

class FMD5
{
public:
	struct Digest
	{
		TBYTE digest[16];
		static_assert(sizeof(boost::uuids::detail::md5::digest_type) == sizeof(digest), "MD5Digest size is not 16 bytes");

		Digest()
		{
			FMemory::MemzeroArray(&digest);
		}
		explicit Digest(boost::uuids::detail::md5::digest_type&& data);

		HLVM_NODISCARD FString ToString() const;
		HLVM_NODISCARD bool Valid() const;

		// compare operator
		bool operator==(const FMD5::Digest& other) const
		{
			return FMemory::Memcmp(digest, other.digest, sizeof(digest)) == 0;
		}

		bool operator!=(const FMD5::Digest& other) const
		{
			return !(*this == other);
		}
	};
public:
	static FMD5::Digest Hash(const void* data, size_t size, FMD5::Digest* prevHash = nullptr);
};

class FSHA1
{
public:
	struct Digest
	{
		TBYTE digest[20];
		static_assert(sizeof(boost::uuids::detail::sha1::digest_type) == sizeof(digest), "SHA1Digest size is not 20 bytes");

		Digest()
		{
			FMemory::MemzeroArray(&digest);
		}
		explicit Digest(boost::uuids::detail::sha1::digest_type&& data);

		FString ToString() const;
		HLVM_NODISCARD bool Valid() const;

		// compare operator
		bool operator==(const FSHA1::Digest& other) const
		{
			return FMemory::Memcmp(digest, other.digest, sizeof(digest)) == 0;
		}

		bool operator!=(const FSHA1::Digest& other) const
		{
			return !(*this == other);
		}
	};

public:
	static FSHA1::Digest Hash(const void* data, size_t size, FSHA1::Digest* prevHash = nullptr);
};
