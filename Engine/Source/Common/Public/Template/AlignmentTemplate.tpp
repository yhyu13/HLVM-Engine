/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "ConceptTemplate.tpp"

/**
 * Aligns a value to the next multiplier specified power of 2 alignment.
 *
 * @param Value The value to align.
 * @param Alignment The alignment to align to.
 * @return The aligned value.
 */
template <CIntegral T>
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
template <CIntegral T>
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
template <CIntegral T>
constexpr bool IsAligned(T Value, size_t Alignment)
{
	return (S_C(size_t, Value) & (Alignment - 1)) == 0;
}

/**
 * Aligns a value to the next multiplier specified power of 2 alignment.
 *
 * @param Value The value to align.
 * @param Alignment The alignment to align to.
 * @return The aligned value.
 */
template <CPointer T>
constexpr T AlignUp(T Value, uintptr_t Alignment)
{
	return R_C(T, (R_C(uintptr_t, Value) + Alignment - 1) & ~(Alignment - 1));
}

/**
 * Aligns a value to the previous multiplier specified power of 2 alignment.
 *
 * @param Value The value to align.
 * @param Alignment The alignment to align to.
 * @return The aligned value.
 */
template <CPointer T>
constexpr T AlignDown(T Value, uintptr_t Alignment)
{
	return R_C(T, R_C(uintptr_t, Value) & ~(Alignment - 1));
}

/**
 * Checks if a value is aligned to the specified power of 2 alignment.
 *
 * @param Value The value to check.
 * @param Alignment The alignment to check against.
 * @return True if the value is aligned, false otherwise.
 */
template <CPointer T>
constexpr bool IsAligned(T Value, uintptr_t Alignment)
{
	return (R_C(uintptr_t, Value) & (Alignment - 1)) == 0;
}
