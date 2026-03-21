/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#pragma once

#include "GlobalDefinition.h"
#include <regex>

#ifndef HLVM_PACKED_TOKEN_FILE_EXT
	#define HLVM_PACKED_TOKEN_FILE_EXT TXT(".toc")
#endif

#ifndef HLVM_PACKED_CONTAINER_FILE_EXT
	#define HLVM_PACKED_CONTAINER_FILE_EXT TXT(".cas")
#endif

#ifndef HLVM_PACKED_FILE_MATCH_PATTERN
// Match pattern for token file, e.g. packed-456789, where 456789 is the CL
	#define HLVM_PACKED_FILE_MATCH_PATTERN std::regex(R"(packed-\S*)")
#endif

#ifndef HLVM_PACKED_PATCH_FILE_MATCH_PATTERN
// Match pattern for patch file, e.g. packed-456790-pat, where 456790 is the patch CL
	#define HLVM_PACKED_PATCH_FILE_MATCH_PATTERN std::regex(R"(packed-\S*-\d+-pat)")
#endif

/**
 * Gen signature file for both token and container file
 */
#ifndef HLVM_PACKED_FILE_WITH_SIGNATURE
	#define HLVM_PACKED_FILE_WITH_SIGNATURE 1
#endif

/**
 * Encrypt token file for as a curties protection
 */
#ifndef HLVM_PACKED_TOKEN_FILE_WITH_ENCRYPTION
	#define HLVM_PACKED_TOKEN_FILE_WITH_ENCRYPTION 1
#endif

/**
 * Use token file debug json of as the token file instead of using packed token file
 * Not recommanded unless you are required to query the pack file for file and directory status
 */
#ifndef HLVM_PACKED_TOKEN_FILE_USE_DEBUG_JSON_INSTAED
	#define HLVM_PACKED_TOKEN_FILE_USE_DEBUG_JSON_INSTAED 0
#endif
