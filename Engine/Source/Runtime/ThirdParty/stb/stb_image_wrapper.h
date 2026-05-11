/**
 * stb_image_wrapper.h
 * Wrapper header to include stb_image.h with warnings suppressed.
 * This is needed because stb_image.h uses old-style casts and other
 * patterns that violate strict warning flags (-Werror -Weverything).
 *
 * Usage: #include "stb_image_wrapper.h" (STB_IMAGE_IMPLEMENTATION must be
 * defined BEFORE including this header to generate implementations).
 */
#ifndef HLVM_STB_IMAGE_WRAPPER_H
#define HLVM_STB_IMAGE_WRAPPER_H

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#pragma clang diagnostic ignored "-Wunused-macros"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#endif

#include "stb_image.h"

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#endif // HLVM_STB_IMAGE_WRAPPER_H
