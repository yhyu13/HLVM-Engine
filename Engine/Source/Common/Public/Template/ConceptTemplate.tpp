/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include <concepts>

/**
 * Concept template T is integral or pointer
 */
template <typename T>
concept CIntegral = std::is_integral<T>::value;

/**
 * Concept template T is integral or pointer
 */
template <typename T>
concept CPointer = std::is_pointer<T>::value;
