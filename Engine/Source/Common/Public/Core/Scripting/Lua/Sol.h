/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#include "GlobalDefinition.h"

#include "lua.hpp"
static_assert(LUA_VERSION_NUM == 501, "hlvm_lua only support lua 5.1");

/**
 * Sol config and safety:
 * https://sol2.readthedocs.io/en/latest/safety.html
 */
#define SOL_LUAJIT 1
#if !HLVM_BUILD_DEBUG
	// In Debug mode, if no memory alignment, would result in memory corruption at
	// sol's type_unique_cast method, try debug TestSol2.cpp::sol2_pointer_lifetime_test to verify it
	// So only loose memory alignment for release build
	#define SOL_NO_MEMORY_ALIGNMENT 1
#endif
#define SOL_ALL_SAFETIES_ON 1
#define SOL_PRINT_ERRORS 1
#if HLVM_BUILD_RELEASE
	#define SOL_NO_CHECK_NUMBER_PRECISION 1
#endif
#include <sol/sol.hpp>
static_assert(SOL_LUA_VERSION == 501, "hlvm_lua only support lua 5.1");

#include "Core/Log.h"

DECLARE_LOG_CATEGORY(LogLua)

namespace hlvm_lua
{
	// TODO : use a lua specific memory allocator?
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

#define HLVM_SOL_STATE(state_name) sol::state state_name(hlvm_lua::lua_panic, hlvm_lua::lua_alloc)
