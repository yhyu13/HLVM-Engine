/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "GlobalDefinition.h"

#ifndef HLVM_JSONL_FILE_EXT
	#define HLVM_JSONL_FILE_EXT TXT(".jsonl")
#endif

#ifndef HLVM_JSONL_LINE_SEPARATOR
	#define HLVM_JSONL_LINE_SEPARATOR TXT("\r\t\n")
// HLVM_INLINE_VAR const FConstByteBuffer HLVM_JSONL_LINE_SEPARATOR_BUFFER{ R_C(const TBYTE*, HLVM_JSONL_LINE_SEPARATOR), 3 };
#endif
