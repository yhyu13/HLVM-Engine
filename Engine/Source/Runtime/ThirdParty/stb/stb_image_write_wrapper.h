/**
 * stb_image_write_wrapper.h
 * Wrapper header to include stb_image_write.h with warnings suppressed.
 */
#ifndef HLVM_STB_IMAGE_WRITE_WRAPPER_H
#define HLVM_STB_IMAGE_WRITE_WRAPPER_H

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#pragma clang diagnostic ignored "-Wunused-macros"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#endif

#include "stb_image_write.h"

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#endif // HLVM_STB_IMAGE_WRITE_WRAPPER_H
