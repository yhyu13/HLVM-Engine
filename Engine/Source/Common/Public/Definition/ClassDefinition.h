/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

// 定义一个类，禁止复制和移动
#define NOCOPY(Class)             \
	Class(const Class&) = delete; \
	Class& operator=(const Class&) = delete;

#define NOMOVE(Class)              \
	Class(const Class&&) = delete; \
	Class& operator=(const Class&&) = delete;

#define NOCOPYMOVE(Class) \
	NOCOPY(Class)         \
	NOMOVE(Class)

// 定义一个类，禁止实例化
#define NOINSTANT(Class)                     \
	Class() = delete;                        \
	~Class() = delete;                       \
	Class(const Class&) = delete;            \
	Class(const Class&&) = delete;           \
	Class& operator=(const Class&) = delete; \
	Class& operator=(const Class&&) = delete;

#define S_C(type, value) static_cast<type>((value))
#define SP_C(type, value) static_pointer_cast<type>((value))
#define D_C(type, value) dynamic_cast<type>((value))
#define DP_C(type, value) dynamic_pointer_cast<type>((value))
#define C_C(type, value) const_cast<type>((value))
#define R_C(type, value) reinterpret_cast<type>((value))

#define MAKE_SHARED(type, value) std::make_shared<type>((value))
#define MAKE_UNIQUE(type, value) std::make_unique<type>((value))

template<typename T>
using SharedRefCountPtr = std::shared_ptr<T>;

template<typename T>
using UniqueRefCountPtr = std::unique_ptr<T>;

// Offset of a struct member. (Copy from UE5)
#ifdef __clang__
	#define STRUCT_OFFSET( struc, member )  __builtin_offsetof(struc, member)
#else
	#define STRUCT_OFFSET( struc, member )  offsetof(struc, member)
#endif
