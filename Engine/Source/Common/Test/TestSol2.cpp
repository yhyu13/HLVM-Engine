/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#include "Test.h"

#include "Core/Scripting/Lua/Sol.h"
#include "ThirdParty/Effil.h"

DECLARE_LOG_CATEGORY(LogTest)

#define TEST_SOL_LUAJIT 1

template <typename A, typename B>
auto my_add(A a, B b)
{
	return a + b;
}

// https://github.com/ThePhD/sol2/blob/develop/examples/source/tutorials/writing_template_functions.cpp
RECORD(sol2_writing_template_functions_test)
{
	HLVM_SOL_STATE(lua);

	// adds 2 integers
	auto int_function_pointer = &my_add<int, int>;
	lua["my_int_add"] = int_function_pointer;

	// concatenates 2 strings
	auto string_function_pointer = &my_add<std::string, std::string>;
	lua["my_string_combine"] = string_function_pointer;

	lua.script("my_num = my_int_add(1, 2)");
	int my_num = lua["my_num"];
	HLVM_ENSURE(my_num == 3, TXT("my_num should be 3"));

	lua.script(
		"my_str = my_string_combine('bark bark', ' woof "
		"woof')");
	std::string my_str = lua["my_str"];
	HLVM_ENSURE(my_str == "bark bark woof woof", TXT("my_str should be bark bark woof woof"));
}

// https://github.com/ThePhD/sol2/blob/develop/examples/source/tutorials/writing_overloaded_template_functions.cpp
RECORD(sol2_writing_overloaded_template_functions_test)
{
	HLVM_SOL_STATE(lua);

	auto int_function_pointer = &my_add<int, int>;
	auto string_function_pointer = &my_add<std::string, std::string>;
	// adds 2 integers, or "adds" (concatenates) two strings
	lua["my_combine"] = sol::overload(
		int_function_pointer, string_function_pointer);

	lua.script("my_num = my_combine(1, 2)");
	lua.script(
		"my_str = my_combine('bark bark', ' woof woof')");
	int			my_num = lua["my_num"];
	std::string my_str = lua["my_str"];
	HLVM_ENSURE(my_num == 3, TXT("my_num should be 3"));
	HLVM_ENSURE(my_str == "bark bark woof woof", TXT("my_str should be bark bark woof woof"));
}

struct my_class
{
	int a = 0;

	my_class(int x)
		: a(x)
	{
	}

	int func()
	{
		++a; // increment a by 1
		return a;
	}
};

// https://github.com/ThePhD/sol2/blob/develop/examples/source/tutorials/writing_member_functions.cpp
RECORD(sol2_writing_member_functions_test)
{
	HLVM_SOL_STATE(lua);
#if TEST_SOL_LUAJIT
	lua.open_libraries();
#else
	lua.open_libraries(sol::lib::base);
#endif

	// Here, we are binding the member function and a class
	// instance: it will call the function on the given class
	// instance
	lua.set_function(
		"my_class_func", &my_class::func, my_class(0));

	// We do not pass a class instance here:
	// the function will need you to pass an instance of
	// "my_class" to it in lua to work, as shown below
	lua.set_function("my_class_func_2", &my_class::func);

	// With a pre-bound instance:
	lua.script(R"(
			first_value = my_class_func()
			second_value = my_class_func()
			assert(first_value == 1)
			assert(second_value == 2)
		)");

	// With no bound instance:
	lua.set("obj", my_class(24));
	// Calls "func" on the class instance
	// referenced by "obj" in Lua
	lua.script(R"(
			third_value = my_class_func_2(obj)
			fourth_value = my_class_func_2(obj)
			assert(third_value == 25)
			assert(fourth_value == 26)
		)");
}

static std::string my_function(size_t D_count, std::string original)
{
	// Create a string with the letter 'D' "D_count" times,
	// append it to 'original'
	return original + std::string(D_count, 'D');
}

// https://github.com/ThePhD/sol2/blob/develop/examples/source/tutorials/writing_functions.cpp
RECORD(sol2_writing_functions_test)
{
	HLVM_SOL_STATE(lua);

	lua["my_func"] = my_function;			  // way 1
	lua.set("my_func", my_function);		  // way 2
	lua.set_function("my_func", my_function); // way 3

	// This function is now accessible as 'my_func' in
	// lua scripts / code run on this state:
	lua.script("some_str = my_func(1, 'Da')");

	// Read out the global variable we stored in 'some_str' in
	// the quick lua code we just executed
	std::string some_str = lua["some_str"];
	HLVM_ENSURE(some_str == "DaD", TXT("some_str should be DaD"));
}

// https://github.com/ThePhD/sol2/blob/develop/examples/source/tutorials/write_variables_demo.cpp
RECORD(sol2_write_variables_demo_test)
{
	HLVM_SOL_STATE(lua);

// open those basic lua libraries
// again, for print() and other basic utilities
#if TEST_SOL_LUAJIT
	lua.open_libraries();
#else
	lua.open_libraries(sol::lib::base);
#endif
	// value in the global table
	lua["bark"] = 50;

	// a table being created in the global table
	lua["some_table"] = lua.create_table_with("key0",
		24,
		"key1",
		25,
		lua["bark"],
		"the key is 50 and this string is its value!");

	// Run a plain ol' string of lua code
	// Note you can interact with things set through sol in C++
	// with lua! Using a "Raw String Literal" to have multi-line
	// goodness:
	// http://en.cppreference.com/w/cpp/language/string_literal
	lua.script(R"(

	print(some_table[50])
	print(some_table["key0"])
	print(some_table["key1"])

	-- a lua comment: access a global in a lua script with the _G table
	print(_G["bark"])

	)");
}

// https://github.com/ThePhD/sol2/blob/develop/examples/source/tutorials/variables_demo.cpp
RECORD(sol2_variables_demo_test)
{
	HLVM_SOL_STATE(lua);
	/*
	lua.script_file("variables.lua");
	*/
	lua.script(R"(
config = {
	fullscreen = false,
	resolution = { x = 1024, y = 768 }
}
	)");
	// the type "sol::state" behaves
	// exactly like a table!
	bool isfullscreen = lua["config"]
						   ["fullscreen"]; // can get nested variables
	sol::table config = lua["config"];
	HLVM_ENSURE(!isfullscreen, TXT("isfullscreen should be false"));

	// can also get it using the "get" member function
	// auto replaces the unqualified type name
	auto resolution = config.get<sol::table>("resolution");

	// table and state can have multiple things pulled out of it
	// too
	std::tuple<int, int> xyresolutiontuple = resolution.get<int, int>("x", "y");
	HLVM_ENSURE(std::get<0>(xyresolutiontuple) == 1024, TXT("xyresolutiontuple[0] should be 1024"));
	HLVM_ENSURE(std::get<1>(xyresolutiontuple) == 768, TXT("xyresolutiontuple[1] should be 768"));

	// test variable
	auto bark = lua["config"]["bark"];
	if (bark.valid())
	{
		// branch not taken: config and/or bark are not
		// variables
	}
	else
	{
		// Branch taken: config and bark are existing variables
	}

	// can also use optional
	sol::optional<int> not_an_integer = lua["config"]["fullscreen"];
	if (not_an_integer)
	{
		// Branch not taken: value is not an integer
	}

	sol::optional<bool> is_a_boolean = lua["config"]["fullscreen"];
	if (is_a_boolean)
	{
		// Branch taken: the value is a boolean
	}

	sol::optional<double> does_not_exist = lua["not_a_variable"];
	if (does_not_exist)
	{
		// Branch not taken: that variable is not present
	}

	// this will result in a value of '24'
	// (it tries to get a number, and fullscreen is
	// not a number
	int is_defaulted = lua["config"]["fullscreen"].get_or(24);
	HLVM_ENSURE(is_defaulted == 24, TXT("is_defaulted should be 24"));

	// This will result in the value of the config, which is
	// 'false'
	bool is_not_defaulted = lua["config"]["fullscreen"];
	HLVM_ENSURE(!is_not_defaulted, TXT("is_not_defaulted should be false"));
}

// https://github.com/ThePhD/sol2/blob/develop/examples/source/tutorials/reading_functions.cpp
RECORD(sol2_reading_functions_test)
{
	HLVM_SOL_STATE(lua);

	lua.script(R"(
			function f (a)
				return a + 5
			end
		)");

	// Get and immediately call
	int x = lua["f"](30);
	HLVM_ENSURE(x == 35, TXT("x should be 35"));

	// Store it into a variable first, then call
	sol::unsafe_function f = lua["f"];
	int					 y = f(20);
	HLVM_ENSURE(y == 25, TXT("y should be 25"));

	// Store it into a variable first, then call
	sol::protected_function safe_f = lua["f"];
	int						z = safe_f(45);
	HLVM_ENSURE(z == 50, TXT("z should be 50"));
}

struct my_type
{
	void stuff()
	{
	}
};

// https://github.com/ThePhD/sol2/blob/develop/examples/source/tutorials/pointer_lifetime.cpp
RECORD(sol2_pointer_lifetime_test)
{
	HLVM_SOL_STATE(lua);

	/*
	// AAAHHH BAD
	// dangling pointer!
	lua["my_func"] = []() -> my_type* { return new my_type(); };

	// AAAHHH!
	lua.set("something", new my_type());

	// AAAAAAHHH!!!
	lua["something_else"] = new my_type();
	*/

	// :ok:
	lua["my_func0"] = []() -> std::unique_ptr<my_type> {
		return std::make_unique<my_type>();
	};

	// :ok:
	lua["my_func1"] = []() -> std::shared_ptr<my_type> {
		return std::make_shared<my_type>();
	};

	// :ok:
	lua["my_func2"] = []() -> my_type { return my_type(); };

	// :ok:
	lua.set(
		"something", std::unique_ptr<my_type>(new my_type()));

	std::shared_ptr<my_type> my_shared = std::make_shared<my_type>();
	// :ok:
	lua.set("something_else", my_shared);

	// :ok:
	auto my_unique = std::make_unique<my_type>();
	lua["other_thing"] = std::move(my_unique);

	// :ok:
	lua["my_func5"] = []() -> my_type* {
		static my_type mt;
		return &mt;
	};

	// THIS IS STILL BAD DON'T DO IT AAAHHH BAD
	// return a unique_ptr that's empty instead
	// or be explicit!
	lua["my_func6"] = []() -> my_type* { return nullptr; };

	// :ok:
	lua["my_func7"] = []() -> std::nullptr_t { return nullptr; };

	// :ok:
	lua["my_func8"] = []() -> std::unique_ptr<my_type> {
		// default-constructs as a nullptr,
		// gets pushed as nil to Lua
		return std::unique_ptr<my_type>();
		// same happens for std::shared_ptr
	};

	// Acceptable, it will set 'something' to nil
	// (and delete it on next GC if there's no more references)
	lua.set("something", nullptr);

	// Also fine
	lua["something_else"] = nullptr;
}

// https://github.com/ThePhD/sol2/blob/develop/examples/source/tutorials/open_multiple_libraries.cpp
RECORD(sol2_open_multiple_libraries_test)
{
	HLVM_SOL_STATE(lua);
	lua.open_libraries(sol::lib::base,
		sol::lib::coroutine,
		sol::lib::string,
		sol::lib::io);

	lua.script("print('bark bark bark!')");
}

// https://github.com/ThePhD/sol2/blob/develop/examples/source/tutorials/object_lifetime.cpp
RECORD(sol2_object_lifetime_test)
{
	HLVM_SOL_STATE(lua);
#if TEST_SOL_LUAJIT
	lua.open_libraries();
#else
	lua.open_libraries(sol::lib::base);
#endif
	lua.script(R"(
	obj = "please don't let me die";
	)");

	sol::object keep_alive = lua["obj"];
	lua.script(R"(
	obj = nil;
	function say(msg)
		print(msg)
	end
	)");

	lua.collect_garbage();

	lua["say"](lua["obj"]);
	// still accessible here and still alive in Lua
	// even though the name was cleared
	std::string message = keep_alive.as<std::string>();
	std::cout << message << std::endl;

	// Can be pushed back into Lua as an argument
	// or set to a new name,
	// whatever you like!
	lua["say"](keep_alive);
}

// https://github.com/ThePhD/sol2/blob/develop/examples/source/tutorials/multiple_return.cpp
RECORD(sol2_multiple_return_test)
{
	HLVM_SOL_STATE(lua);

	lua.script("function f (a, b, c) return a, b, c end");

	std::tuple<int, int, int> result;
	result = lua["f"](1, 2, 3);
	HLVM_ENSURE(result == std::make_tuple(1, 2, 3), TXT("result should be 1, 2, 3"));
	int			a, b;
	std::string c;
	// NOTE: sol::tie, NOT std::tie
	// (ESS OH ELL prefix, not ESS TEE DEE prefix)
	sol::tie(a, b, c) = lua["f"](1, 2, "bark");
	HLVM_ENSURE(a == 1, TXT("a should be 1"));
	HLVM_ENSURE(b == 2, TXT("b should be 2"));
	HLVM_ENSURE(c == "bark", TXT("c should be 'bark'"));
}

// https://github.com/ThePhD/sol2/blob/develop/examples/source/tutorials/lazy_demo.cpp
RECORD(sol2_lazy_demo_test)
{
	HLVM_SOL_STATE(lua);

	auto barkkey = lua["bark"];
	HLVM_ENSURE(!barkkey.valid(), TXT("barkkey should not be valid"));

	barkkey = 50;
	HLVM_ENSURE(barkkey.valid(), TXT("barkkey should be valid"));

	auto barkkey2 = lua["bark"];
	HLVM_ENSURE(barkkey2.valid(), TXT("barkkey2 should be valid"));
}

// https://github.com/ThePhD/sol2/blob/develop/examples/source/tutorials/erase_demo.cpp
RECORD(sol2_erase_demo_test)
{
	HLVM_SOL_STATE(lua);
	lua["bark"] = 50;
	sol::optional<int> x = lua["bark"];
	// x will have a value
	HLVM_ENSURE(x, TXT("x should have a value"));

	lua["bark"] = sol::lua_nil;
	sol::optional<int> y = lua["bark"];
	// y will not have a value
	HLVM_ENSURE(!y, TXT("y should not have a value"));
}

struct Doge
{
	int tailwag = 50;

	Doge()
	{
	}

	Doge(int wags)
		: tailwag(wags)
	{
	}

	// Copy constructor
	Doge(const Doge& other)
	{
		std::cout << "Doge copy constructor called" << std::endl;
		tailwag = other.tailwag;
	}

	~Doge()
	{
		std::cout << "Doge at " << this
				  << " is being destroyed..." << std::endl;
	}
};

// https://github.com/ThePhD/sol2/blob/develop/examples/source/tutorials/quick_n_dirty/userdata.cpp
RECORD(sol2_userdata_test)
{
	std::cout << "=== userdata ===" << std::endl;

	HLVM_SOL_STATE(lua);

	Doge dog{ 30 };

	// fresh one put into Lua
	lua["dog"] = Doge{};
	// Copy into lua: destroyed by Lua VM during garbage
	// collection
	lua["dog_copy"] = dog;
	// OR: move semantics - will call move constructor if
	// present instead Again, owned by Lua
	lua["dog_move"] = std::move(dog);
	lua["dog_unique_ptr"] = std::make_unique<Doge>(25);
	lua["dog_shared_ptr"] = std::make_shared<Doge>(31);

	// Identical to above
	Doge dog2{ 30 };
	lua.set("dog2", Doge{});
	lua.set("dog2_copy", dog2);
	lua.set("dog2_move", std::move(dog2));
	lua.set("dog2_unique_ptr",
		std::unique_ptr<Doge>(new Doge(25)));
	lua.set("dog2_shared_ptr",
		std::shared_ptr<Doge>(new Doge(31)));

	// Note all of them can be retrieved the same way:
	Doge& lua_dog = lua["dog"];
	Doge& lua_dog_copy = lua["dog_copy"];
	Doge& lua_dog_move = lua["dog_move"];
	Doge& lua_dog_unique_ptr = lua["dog_unique_ptr"];
	Doge& lua_dog_shared_ptr = lua["dog_shared_ptr"];
	HLVM_ENSURE(lua_dog.tailwag == 50, TXT("lua_dog should have 50 tailwags"));
	HLVM_ENSURE(lua_dog_copy.tailwag == 30, TXT("lua_dog_copy should have 30 tailwags"));
	HLVM_ENSURE(lua_dog_move.tailwag == 30, TXT("lua_dog_move should have 30 tailwags"));
	HLVM_ENSURE(lua_dog_unique_ptr.tailwag == 25, TXT("lua_dog_unique_ptr should have 25 tailwags"));
	HLVM_ENSURE(lua_dog_shared_ptr.tailwag == 31, TXT("lua_dog_shared_ptr should have 31 tailwags"));

	// lua will treat these types as opaque, and you will be
	// able to pass them around to C++ functions and Lua
	// functions alike

	// Use a C++ reference to handle memory directly
	// otherwise take by value, without '&'
	lua["f"] = [](Doge& _dog) {
		std::cout << "dog wags its tail " << _dog.tailwag
				  << " times!" << std::endl;
	};

	// if you bind a function using a pointer,
	// you can handle when `nil` is passed
	lua["handling_f"] = [](Doge* _dog) {
		if (_dog == nullptr)
		{
			std::cout << "dog was nil!" << std::endl;
			return;
		}
		std::cout << "dog wags its tail " << _dog->tailwag
				  << " times!" << std::endl;
	};

	lua.script(R"(
		f(dog)
		f(dog_copy)
		f(dog_move)
		f(dog_unique_ptr)
		f(dog_shared_ptr)

		-- C++ arguments that are pointers can handle nil
		handling_f(dog)
		handling_f(dog_copy)
		handling_f(dog_move)
		handling_f(dog_unique_ptr)
		handling_f(dog_shared_ptr)
		handling_f(nil)

		-- never do this
		-- f(nil)
	)");

	std::cout << std::endl;
}

// https://github.com/ThePhD/sol2/blob/develop/examples/source/tutorials/quick_n_dirty/userdata_memory_reference.cpp
RECORD(sol2_userdata_memory_reference_test)
{
	std::cout << "=== userdata memory reference ==="
			  << std::endl;

	HLVM_SOL_STATE(lua);
#if TEST_SOL_LUAJIT
	lua.open_libraries();
#else
	lua.open_libraries(sol::lib::base);
#endif
	Doge dog{}; // Kept alive somehow

	// Later...
	// The following stores a reference, and does not copy/move
	// lifetime is same as dog in C++
	// (access after it is destroyed is bad)
	lua["dog"] = &dog;
	// Same as above: respects std::reference_wrapper
	lua["dog"] = std::ref(dog);
	// These two are identical to above
	lua.set("dog", &dog);
	lua.set("dog", std::ref(dog));

	Doge& dog_ref = lua["dog"];		// References Lua memory
	Doge* dog_pointer = lua["dog"]; // References Lua memory
	Doge  dog_copy = lua["dog"];	// Copies, will not affect lua

	sol::constructors<Doge(), Doge(int), Doge(const Doge&)> DogeCtr{};
	lua.new_usertype<Doge>("Doge",
		DogeCtr,
		"tailwag",
		&Doge::tailwag);

	dog_copy.tailwag = 525;
	// Still 50
	lua.script("assert(dog.tailwag == 50)");

	dog_ref.tailwag = 100;
	// Now 100
	lua.script("assert(dog.tailwag == 100)");

	dog_pointer->tailwag = 345;
	// Now 345
	lua.script("assert(dog.tailwag == 345)");

	std::cout << std::endl;
}

#include "Core/Name.h"

RECORD(sol2_hlvm_refcount_test)
{
	HLVM_SOL_STATE(lua);
#if TEST_SOL_LUAJIT
	lua.open_libraries();
#else
	lua.open_libraries(sol::lib::base);
#endif
	sol::constructors<FName(),
		FName(const char*)>
		FNameCtr{};
	lua.new_usertype<FName>("FName",
		FNameCtr,
		"RefCount",
		&FName::RefCount,
		"ToCharCStr",
		&FName::ToCharCStr);

	FName name("test");
	HLVM_ENSURE(name.RefCount() == 1, TXT("name should have 1 reference"));

	// Lua Only store rc-obj as a raw pointer, so rc remain 1
	lua["name0"] = &(name);
	lua.script(R"(
        assert(name0:RefCount() == 1)
        print(name0:RefCount())
    )");

	// Lua Store a copy of rc-obj, so rc increase to 2 because now lua_state owns it too
	lua["name"] = CopyTemp(name);
	lua.script(R"(
        assert(name:RefCount() == 2)
        print(name:RefCount())
    )");

	// Lua Store a second copy of rc-obj, so rc increase to 3 because now lua_state owns it too
	lua["name2"] = CopyTemp(name);
	lua.script(R"(
        assert(name2:RefCount() == 3)
        print(name2:RefCount())
    )");

	// cpp create a third copy of rc-obj, so rc increase to 4 because now cpp owns it too
	FName name2 = CopyTemp(name);
	lua.script(R"(
        assert(name:RefCount() == 4)
        print(name:RefCount())
    )");
	lua.script(R"(
        assert(name2:RefCount() == 4)
        print(name2:RefCount())
    )");

	// lua create a new rc-obj which should has rc 1
	lua.script(R"(
        local name3 = FName.new("test2")
        assert(name3:RefCount() == 1)
        print(name3:RefCount())
    )");
}

#include "ThirdParty/Effil.h"

#define EFFILE_STRESS_TEST 0
#if EFFILE_STRESS_TEST
	#include <boost/process/env.hpp>
#endif

RECORD(sol2_effil_test)
{
	HLVM_PROFILE_CPU_NAMED("sol2_effil_test");
#if EFFILE_STRESS_TEST
	auto e = boost::this_process::environment();
	e["STRESS"] = "1";
#endif

	{
		auto	   CWD = boost::filesystem::current_path();
		const auto DataDir = FString::Format(TXT("{}/{}_Data"), *GExecutablePath, *GExecutableName);
		const bool bDataDirExist = FGenericPlatformFile::Get(EPlatformFileType::Local)->Exists(DataDir);
		if (bDataDirExist)
		{
			auto AllLuaFiles = FGenericPlatformFile::Get(EPlatformFileType::Local)->Glob(DataDir, TXT(R"(.*run_tests\.lua$)"), true);
			for (auto LuaFile : AllLuaFiles)
			{
				HLVM_LOG(LogTest, info, TXT("Test lua file: {}"), *LuaFile);
				/**
				 * Setting the current path to the lua file's parent path
				 */
				boost::filesystem::current_path(LuaFile.parent_path());

				HLVM_SOL_STATE(lua);
#if TEST_SOL_LUAJIT
				lua.open_libraries();
#else
				lua.open_libraries(sol::lib::base,
					sol::lib::package,
					sol::lib::string,
					sol::lib::table,
					sol::lib::debug,
					sol::lib::os,
					sol::lib::math);
#endif
				lua.require("effil", luaopen_effil, false);

				try
				{
					FScopedTimerLog timer(FString::Format(TXT("Test lua file: {}"), *LuaFile));
					auto			Result = lua.safe_script_file(LuaFile.ToCharCStr());
					if (!Result.valid())
					{
						sol::error err = Result;
						HLVM_LOG(LogTest, err, TXT("Test lua file: {} failed: {}"), *LuaFile, TO_TCHAR_CSTR(err.what()));
					}
				}
				catch (...)
				{
				}
			}
		}
		else
		{
			HLVM_LOG(LogTest, warn, TXT("Data dir not exist: {}"), *DataDir);
		}
		boost::filesystem::current_path(CWD);
	}
}
