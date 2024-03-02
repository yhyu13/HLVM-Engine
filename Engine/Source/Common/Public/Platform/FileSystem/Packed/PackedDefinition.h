/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "GlobalDefinition.h"
#include <boost/regex/v5/regex_search.hpp>

#ifndef HLVM_PACKED_TOKEN_EXT
	#define HLVM_PACKED_TOKEN_EXT TXT(".tok")
#endif

#ifndef HLVM_PACKED_CONTAINER_EXT
	#define HLVM_PACKED_CONTAINER_EXT TXT(".cot")
#endif

#ifndef HLVM_PACKED_FILE_PATTERN
	#define HLVM_PACKED_FILE_PATTERN boost::regex(R"(packed-\S*)")
#endif

#ifndef HLVM_PACKED_PATCH_FILE_PATTERN
	#define HLVM_PACKED_PATCH_FILE_PATTERN boost::regex(R"(packed-\S*-\d+-pat)")
#endif

#ifndef HLVM_PACKED_FILE_WITH_SIGNATURE
	#define HLVM_PACKED_FILE_WITH_SIGNATURE 1
#endif

#ifndef HLVM_PACKED_TOKEN_FILE_WITH_ENCRYPTION
	#define HLVM_PACKED_TOKEN_FILE_WITH_ENCRYPTION 1
#endif
