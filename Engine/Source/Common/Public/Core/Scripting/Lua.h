/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "GlobalDefinition.h"

#define SOL_ALL_SAFETIES_ON HLVM_BUILD_DEBUG
#include <sol/sol.hpp>

#include "Core/Log.h"

DECLARE_LOG_CATEGORY(LogLua)

namespace hlvm_lua
{
	HLVM_INLINE_FUNC void* lua_alloc(void* ud, void* ptr, size_t osize, size_t nsize)
	{
		(void)ud;
		(void)osize;
		if (nsize == 0)
		{
			free(ptr);
			ptr = nullptr;
		}
		else
		{
			ptr = realloc(ptr, nsize);
		}
		return ptr;
	}

	HLVM_INLINE_FUNC int lua_panic(lua_State* L)
	{
		HLVM_LOG(LogLua, err, TXT("Lua panic at {}!"), TO_TCHAR_STR(lua_tostring(L, -1)));
		return 0;
	}

	HLVM_MAYBEUNUSED HLVM_INLINE_FUNC lua_State* lua_newstate_alloc()
	{
		auto L = lua_newstate(lua_alloc, nullptr);
		if (L)
		{
			lua_atpanic(L, lua_panic);
		}
		return L;
	}
} // namespace hlvm_lua

#define HLVM_SOL_STATE(var) sol::state var(hlvm_lua::lua_panic, hlvm_lua::lua_alloc)
