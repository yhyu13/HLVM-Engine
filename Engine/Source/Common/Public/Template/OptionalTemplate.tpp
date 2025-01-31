/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

template <typename T>
using TOptional = std::optional<T>;

// First, let's create a type trait to extract the inner type from an optional
template <typename T>
struct TOptionalRemoved
{
	using Type = T;
};

template <typename T>
struct TOptionalRemoved<std::optional<T>>
{
	using Type = T;
};
