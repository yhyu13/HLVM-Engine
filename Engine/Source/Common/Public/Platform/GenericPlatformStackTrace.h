/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#pragma once

#include "GenericPlatform.h"
#include "Core/String.h"

class FGenericPlatformStackTrace
{
public:
	NOCOPYMOVE(FGenericPlatformStackTrace);
	FGenericPlatformStackTrace() = default;

	/**
	 * Generic Platform method that get the stack trace string
	 * @param skip number of frame to skip, counting from bottom
	 * @param max_depth  max number of frame to get
	 * @return FStdString of the stack trace
	 */
	HLVM_NOINLINE_FUNC HLVM_STATIC_FUNC FStdString GetStackTrace(size_t skip = 0, size_t max_depth = 10);

	/**
	 * Generic Platform method that get the stack trace string
	 * @param skip number of frame to skip, counting from bottom
	 * @param max_depth  max number of frame to get
	 * @return std::ostringstream of the stack trace
	 */
	HLVM_NOINLINE_FUNC HLVM_STATIC_FUNC std::ostringstream GetStackTraceStream(size_t skip = 0, size_t max_depth = 10);

	// TODO Get stack address and so on
};
