/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "Core/String.h"
#include "Template/GlobalTemplate.tpp"

#include <boost/algorithm/hex.hpp>
#include <boost/uuid/detail/md5.hpp>

class FMD5Hash
{
public:
	struct MD5Digest
	{
		std::byte digest[16];

		MD5Digest() = default;
		explicit MD5Digest(boost::uuids::detail::md5::digest_type&& data);

		FString ToString() const;
	};

	static MD5Digest Hash(const void* data, size_t size);
};