/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once
#include <iostream>

/**
 * Templated printf that require now memory allocation, format only accept %% or %s for simplicity
 */
template <typename T>
const char* hlvm_printf_inner(const char* format, T value)
{
	std::cout << value;
	return format;
}

/**
 * Templated printf that require now memory allocation, format only accept %% or %s for simplicity
 */
template <typename T, typename... Args>
const char* hlvm_printf_inner(const char* format, T first, Args... args)
{
	std::cout << first;
	return hlvm_printf(format, args...);
}

/**
 * Templated printf that require now memory allocation, format only accept %% or %s for simplicity
 */
template <typename... Args>
const char* hlvm_printf(const char* format, Args... args)
{
	const char* current = format;
	while (*current)
	{
		if (*current == '%')
		{
			// %s or %%
			++current;
			if (*current == '%')
			{
				std::cout << '%';
			}
			else if (*current == 's')
			{
				++current;
				current = hlvm_printf_inner(current, args...);
				// If still has printable content, we continue. Else we stop
				if (*current)
				{
					continue;
				}
				return current;
			}
			else
			{
				// Invalid format!
				return current;
			}
		}
		else
		{
			std::cout << *current;
		}
		++current;
	}
	return current;
}
