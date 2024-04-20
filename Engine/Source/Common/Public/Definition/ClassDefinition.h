/**
 * Copyright (c) 2024. MIT License. All rights reserved.
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
