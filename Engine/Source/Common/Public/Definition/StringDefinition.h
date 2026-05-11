/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#pragma once

// Use char for best compatibility with other libraries
#define TCHAR char8_t
static_assert(sizeof(TCHAR) == sizeof(char), "TCHAR is not char in size");

//  Use utf8 for all string literal
//  U8_STRING("Hello World!")
#define _U8_STRING(str) u8##str
#define TXT(str) _U8_STRING(str)
#define STRTIFY(x) TXT(#x)

#define _LITERAL(x) #x
#define LITERAL(x) _LITERAL(x)

/**
 * Here we refer "cstr" as const pointer of some char type (char, char16_t, char32_t, wchar_t)
 */
#define TO_TCHAR_CSTR(x) reinterpret_cast<const TCHAR*>((x))
#define TCHARSTR(x) TO_TCHAR_CSTR(x)
#define TO_CHAR_CSTR(x) reinterpret_cast<const char*>((x))
#define CHARSTR(x) TO_CHAR_CSTR(x)
