/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#ifndef HLVM_BUILD_DEBUG
	#define HLVM_BUILD_DEBUG 0
#endif

#ifndef HLVM_BUILD_DEVELOPMENT
	#define HLVM_BUILD_DEVELOPMENT 0
#endif

#ifndef HLVM_BUILD_RELEASE
	#define HLVM_BUILD_RELEASE 0
#endif

#if HLVM_BUILD_RELEASE + HLVM_BUILD_DEBUG + HLVM_BUILD_DEVELOPMENT != 1
	#error "HLVM_BUILD_RELEASE + HLVM_BUILD_DEBUG + HLVM_BUILD_DEVELOPMENT != 1"
#endif

/**
 * If HLVM_SHIPPING is defined, then the code will be compiled in a way that
 * is optimized for shipping. e.g. disable profiling and other developer only features
 */
#ifndef HLVM_SHIPPING
	#define HLVM_SHIPPING 0
#endif

#include "Definition/KeywordDefinition.h"
#include "Definition/ClassDefinition.h"
#include "Definition/TypeDefinition.h"
#include "Definition/MiscDefinition.h"
#include "Definition/StringDefinition.h"
#include "Definition/MacroDefinition.h"
#include "Definition/EnumDefinition.h"

#include "UserPredefined.gen.h"
