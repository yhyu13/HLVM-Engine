/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#include "Test.h"

#include "Core/Scripting/Lua.h"

DECLARE_LOG_CATEGORY(LogTest)

RECORD(luajit_test)
{
	HLVM_PROFILE_CPU_NAMED("luajit_test");

	HLVM_LOG(LogTest, info, TXT("Test luajit_test!"));
	{
		int		   status, result, i;
		double	   sum;
		lua_State* L;

		/*
		 * All Lua contexts are held in this structure. We work with it almost
		 * all the time.
		 */
		L = hlvm_lua::lua_newstate_alloc();

		luaL_openlibs(L); /* Load Lua libraries */

		static const char* lua_sciprt1 = R"(
-- script.lua
-- Receives a table, returns the sum of its components.
io.write("The table the script received has:\n");
x = 0
for i = 1, #foo do
  print(i, foo[i])
  x = x + foo[i]
end
io.write("Returning data back to C\n");
return x
)";

		/* Load the file containing the script we are going to run */
		status = luaL_loadstring(L, lua_sciprt1);
		if (status)
		{
			/* If something went wrong, error message is at the top of */
			/* the stack */
			fprintf(stderr, "Couldn't load file: %s\n", lua_tostring(L, -1));
			exit(1);
		}

		/*
		 * Ok, now here we go: We pass data to the lua script on the stack.
		 * That is, we first have to prepare Lua's virtual stack the way we
		 * want the script to receive it, then ask Lua to run it.
		 */
		lua_newtable(L); /* We will pass a table */

		/*
		 * To put values into the table, we first push the index, then the
		 * value, and then call lua_rawset() with the index of the table in the
		 * stack. Let's see why it's -3: In Lua, the value -1 always refers to
		 * the top of the stack. When you create the table with lua_newtable(),
		 * the table gets pushed into the top of the stack. When you push the
		 * index and then the cell value, the stack looks like:
		 *
		 * <- [stack bottom] -- table, index, value [top]
		 *
		 * So the -1 will refer to the cell value, thus -3 is used to refer to
		 * the table itself. Note that lua_rawset() pops the two last elements
		 * of the stack, so that after it has been called, the table is at the
		 * top of the stack.
		 */
		for (i = 1; i <= 5; i++)
		{
			lua_pushnumber(L, i);	  /* Push the table index */
			lua_pushnumber(L, i * 2); /* Push the cell value */
			lua_rawset(L, -3);		  /* Stores the pair in the table */
		}

		/* By what name is the script going to reference our table? */
		lua_setglobal(L, "foo");

		/* Ask Lua to run our little script */
		result = lua_pcall(L, 0, LUA_MULTRET, 0);
		if (result)
		{
			fprintf(stderr, "Failed to run script: %s\n", lua_tostring(L, -1));
			exit(1);
		}

		/* Get the returned value at the top of the stack (index -1) */
		sum = lua_tonumber(L, -1);

		printf("Script returned: %.0f\n", sum);

		lua_pop(L, 1); /* Take the returned value out of the stack */
		lua_close(L);  /* Cya, Lua */
	}
}

#define SOL2_USE_LUAL 0

RECORD(luajit_sol2_perf_test)
{
	HLVM_PROFILE_CPU_NAMED("luajit_test2");

	HLVM_LOG(LogTest, info, TXT("Test luajit_test2!"));
	{
		{
			static const char* lua_sciprt = R"(
local loop_count = 1000
local fun_pair = pairs

local t = {}
for i=1,100 do
    t[i] = i
end

for i=1,loop_count do
    for j=1,1000 do
        for k,v in fun_pair(t) do
            --
        end
    end
end
)";
			{
				int		   status, result;
				lua_State* L;
				L = hlvm_lua::lua_newstate_alloc();

				luaL_openlibs(L); /* Load Lua libraries */

				/* Load the file containing the script we are going to run */
				status = luaL_loadstring(L, lua_sciprt);
				if (status)
				{
					/* If something went wrong, error message is at the top of */
					/* the stack */
					fprintf(stderr, "Couldn't load file: %s\n", lua_tostring(L, -1));
					exit(1);
				}

				{
					FScopedTimerLog timer(TXT("LuaJIT pairs loop test"));
					/* Ask Lua to run our little script */
					result = lua_pcall(L, 0, LUA_MULTRET, 0);
					if (result)
					{
						fprintf(stderr, "Failed to run script: %s\n", lua_tostring(L, -1));
						exit(1);
					}
				}
				lua_close(L); /* Cya, Lua */
			}

			{
				HLVM_SOL_STATE(lua);
				lua.open_libraries(sol::lib::base, sol::lib::ffi, sol::lib::jit);

#if SOL2_USE_LUAL
				/* Load the file containing the script we are going to run */
				int status = luaL_loadstring(lua.lua_state(), lua_sciprt);
				if (status)
				{
					/* If something went wrong, error message is at the top of */
					/* the stack */
					fprintf(stderr, "Couldn't load file: %s\n", lua_tostring(lua.lua_state(), -1));
					exit(1);
				}
				{
					FScopedTimerLog timer(TXT("Sol2 pairs loop test"));
					/* Ask Lua to run our little script */
					int result = lua_pcall(lua.lua_state(), 0, LUA_MULTRET, 0);
					if (result)
					{
						fprintf(stderr, "Failed to run script: %s\n", lua_tostring(lua.lua_state(), -1));
						exit(1);
					}
				}
#else
				{
					FScopedTimerLog timer(TXT("Sol2 pairs loop test"));
					lua.unsafe_script(lua_sciprt);
				}
#endif
			}
		}

		{
			static const char* lua_sciprt = R"(
local loop_count = 1000
local fun_pair = ipairs

local t = {}
for i=1,100 do
    t[i] = i
end

for i=1,loop_count do
    for j=1,1000 do
        for k,v in fun_pair(t) do
            --
        end
    end
end
)";
			{
				int		   status, result;
				lua_State* L;
				L = hlvm_lua::lua_newstate_alloc();

				luaL_openlibs(L); /* Load Lua libraries */

				/* Load the file containing the script we are going to run */
				status = luaL_loadstring(L, lua_sciprt);
				if (status)
				{
					/* If something went wrong, error message is at the top of */
					/* the stack */
					fprintf(stderr, "Couldn't load file: %s\n", lua_tostring(L, -1));
					exit(1);
				}

				{
					FScopedTimerLog timer(TXT("LuaJIT ipairs loop test"));
					/* Ask Lua to run our little script */
					result = lua_pcall(L, 0, LUA_MULTRET, 0);
					if (result)
					{
						fprintf(stderr, "Failed to run script: %s\n", lua_tostring(L, -1));
						exit(1);
					}
				}
				lua_close(L); /* Cya, Lua */
			}

			{
				HLVM_SOL_STATE(lua);
				lua.open_libraries(sol::lib::base, sol::lib::ffi, sol::lib::jit);
#if SOL2_USE_LUAL
				/* Load the file containing the script we are going to run */
				int status = luaL_loadstring(lua.lua_state(), lua_sciprt);
				if (status)
				{
					/* If something went wrong, error message is at the top of */
					/* the stack */
					fprintf(stderr, "Couldn't load file: %s\n", lua_tostring(lua.lua_state(), -1));
					exit(1);
				}
				{
					FScopedTimerLog timer(TXT("Sol2 ipairs loop test"));
					/* Ask Lua to run our little script */
					int result = lua_pcall(lua.lua_state(), 0, LUA_MULTRET, 0);
					if (result)
					{
						fprintf(stderr, "Failed to run script: %s\n", lua_tostring(lua.lua_state(), -1));
						exit(1);
					}
				}
#else
				{
					FScopedTimerLog timer(TXT("Sol2 pairs loop test"));
					lua.unsafe_script(lua_sciprt);
				}
#endif
			}
		}
	}
}
