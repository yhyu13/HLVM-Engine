/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

template <typename T>
struct TReferenceRemoved
{
	using Type = T;
};

template <typename T>
struct TReferenceRemoved<T&>
{
	using Type = T;
};

template <typename T>
struct TReferenceRemoved<T&&>
{
	using Type = T;
};
