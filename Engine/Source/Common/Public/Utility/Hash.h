/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "Core/String.h"

#include <boost/algorithm/hex.hpp>
#include <boost/uuid/detail/md5.hpp>
#include <boost/uuid/detail/sha1.hpp>

struct FMD5Digest
{
	std::byte digest[16];

	FMD5Digest() = default;
	explicit FMD5Digest(boost::uuids::detail::md5::digest_type&& data);

	FString ToString() const;
};

class FMD5Hash
{
public:
	static FMD5Digest Hash(const void* data, size_t size);
};

struct FSHA1Digest
{
	std::byte digest[20];

	FSHA1Digest() = default;
	explicit FSHA1Digest(boost::uuids::detail::sha1::digest_type&& data);

	FString ToString() const;
};

class FSHA1Hash
{
public:
	static FSHA1Digest Hash(const void* data, size_t size);
};
