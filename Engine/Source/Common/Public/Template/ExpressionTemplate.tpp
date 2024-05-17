/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include <ctre.hpp>

constexpr auto ctre_MatchFunctionCall(std::u8string_view sv) noexcept
{
	return ctre::match<R"(\w+\((.*?)\)\w*)">(sv);
};

constexpr auto ctre_MatchAssignment(std::u8string_view sv) noexcept
{
	return ctre::match<R"(.*?[^=<>]=[^=<>].*?)">(sv);
};

constexpr const char* ct_strrchr(const char* str, int ch)
{
	const char* last = nullptr;
	for (const char* p = str; *p != '\0'; ++p)
	{
		if (*p == ch)
		{
			last = p;
		}
	}
	return last;
};

/**
 * https://artificial-mind.net/blog/2020/10/31/constexpr-for
 */
template <auto Start, auto End, auto Inc, class IteraType, class F>
constexpr void ct_for(F&& f)
{
	if constexpr (Start < End)
	{
		f(std::integral_constant<IteraType, Start>());
		ct_for<Start + Inc, End, Inc, IteraType>(f);
	}
}

template <typename T>
constexpr size_t ct_GetArraySize(T Array[])
{
	return sizeof(Array) / sizeof(T);
}

template <typename... TArgs>
constexpr size_t ct_GetArgsCount(TArgs&&...)
{
	return sizeof...(TArgs);
}

#define HLVM_GET_ARGS_COUNT(...) ct_GetArgsCount(__VA_ARGS__)
