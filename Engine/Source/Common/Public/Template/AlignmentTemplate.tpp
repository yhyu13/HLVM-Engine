/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

/**
 * Concept template T is integral or pointer
 */
template <typename T>
concept CAlignable = std::is_integral<T>::value || std::is_pointer<T>::value;

/**
 * Aligns a value to the next multiplier specified power of 2 alignment.
 *
 * @param Value The value to align.
 * @param Alignment The alignment to align to.
 * @return The aligned value.
 */
template <CAlignable T>
constexpr T AlignUp(T Value, size_t Alignment)
{
	return S_C(T, (S_C(size_t, Value) + Alignment - 1) & ~(Alignment - 1));
}

/**
 * Aligns a value to the previous multiplier specified power of 2 alignment.
 *
 * @param Value The value to align.
 * @param Alignment The alignment to align to.
 * @return The aligned value.
 */
template <CAlignable T>
constexpr T AlignDown(T Value, size_t Alignment)
{
	return S_C(T, S_C(size_t, Value) & ~(Alignment - 1));
}

/**
 * Checks if a value is aligned to the specified power of 2 alignment.
 *
 * @param Value The value to check.
 * @param Alignment The alignment to check against.
 * @return True if the value is aligned, false otherwise.
 */
template <CAlignable T>
constexpr bool IsAligned(T Value, size_t Alignment)
{
	return (S_C(size_t, Value) & (Alignment - 1)) == 0;
}
