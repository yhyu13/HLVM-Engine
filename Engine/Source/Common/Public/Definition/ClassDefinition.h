/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#pragma once

// 定义一个类，禁止复制和移动
#define NOCOPY(Class)             \
	Class(const Class&) = delete; \
	Class& operator=(const Class&) = delete

#define NOMOVE(Class)              \
	Class(const Class&&) = delete; \
	Class& operator=(const Class&&) = delete

#define NOCOPYMOVE(Class) \
	NOCOPY(Class);         \
	NOMOVE(Class)

// 定义一个类，禁止实例化
#define NOINSTANTIATE(Class)                     \
	Class() = delete;                        \
	~Class() = delete;                       \
	Class(const Class&) = delete;            \
	Class(const Class&&) = delete;           \
	Class& operator=(const Class&) = delete; \
	Class& operator=(const Class&&) = delete

#define SC1 static_cast
#define S_C(type, value) SC1<type>((value))
#define SPC1 static_pointer_cast
#define SP_C(type, value) SPC1<type>((value))
#define DC1 dynamic_cast
#define D_C(type, value) DC1<type>((value))
#define DPC1 dynamic_pointer_cast
#define DP_C(type, value) DPC1<type>((value))
#define CC1 const_cast
#define C_C(type, value) CC1<type>((value))
#define RC1 reinterpret_cast
#define R_C(type, value) RC1<type>((value))

#define MS1 std::make_shared
#define MU1 std::make_unique
#define MAKE_SHARED(type, ...) MS1<type>(__VA_ARGS__)
#define MAKE_UNIQUE(type, ...) MU1<type>(__VA_ARGS__)

template<typename T>
using TSharePtr = std::shared_ptr<T>;
template<typename T>
using TSharedPtr = TSharePtr<T>;

template<typename T>
using TUniquePtr = std::unique_ptr<T>;
template<typename T>
using TUniquedPtr = TUniquePtr<T>;

// Offset of a struct member. (Copy from UE5)
#ifdef __clang__
	#define STRUCT_OFFSET( struc, member )  __builtin_offsetof(struc, member)
#else
	#define STRUCT_OFFSET( struc, member )  offsetof(struc, member)
#endif
