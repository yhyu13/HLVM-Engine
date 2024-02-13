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
	return ctre::match<R"(.*?[^=]=[^=].*?)">(sv);
};
