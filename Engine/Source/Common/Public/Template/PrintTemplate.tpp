/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once
#include <iostream>

namespace hlvm_private
{
	/**
	 * Templated printf that require now memory allocation, format only accept %% or %s for simplicity
	 */
	template <typename T, typename U>
	const char* StreamPrintfInner(T* ostream, const char* format, U value)
	{
		*ostream << value;
		return format;
	}

	/**
	 * Templated printf that require now memory allocation, format only accept %% or %s for simplicity
	 */
	template <typename T, typename U, typename... Args>
	const char* StreamPrintfInner(T* ostream, const char* format, U first, Args... args)
	{
		*ostream << first;
		return StreamPrintf(ostream, format, args...);
	}
} // namespace hlvm_private

/**
 * Templated printf that require now memory allocation, format only accept %% or %s for simplicity
 */
template <typename T, typename... Args>
const char* StreamPrintf(T* ostream, const char* format, Args... args)
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
				*ostream << '%';
			}
			else if (*current == 's')
			{
				++current;
				current = hlvm_private::StreamPrintfInner(ostream, current, args...);
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
			*ostream << *current;
		}
		++current;
	}
	return current;
}
