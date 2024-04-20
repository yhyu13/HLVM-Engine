/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

// Use char for best compatibility with other libraries
#define TCHAR char8_t
static_assert(sizeof(TCHAR) == sizeof(char), "TCHAR is not char in size");

//  Use utf8 for all string literal
//  U8_STRING("Hello World!")
#define U8_STRING(str) u8##str
#define TXT(str) U8_STRING(str)
#define STRTIFY(x) TXT(#x)
#define TO_TCHAR_STR(x) reinterpret_cast<const TCHAR*>((x))
#define TO_CHAR_STR(x) reinterpret_cast<const char*>((x))
