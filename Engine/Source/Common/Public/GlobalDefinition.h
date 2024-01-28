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

// 定义一个类，禁止复制和移动
#define NOCOPY(Class)                        \
	Class(const Class&) = delete;            \
	Class(const Class&&) = delete;           \
	Class& operator=(const Class&) = delete; \
	Class& operator=(const Class&&) = delete;

// 定义一个类，禁止实例化
#define NOINSTANT(Class)                     \
	Class() = delete;                        \
	~Class() = delete;                       \
	Class(const Class&) = delete;            \
	Class(const Class&&) = delete;           \
	Class& operator=(const Class&) = delete; \
	Class& operator=(const Class&&) = delete;

#define TOKENPASTE(x, y) x##y
#define TOKENPASTE2(x, y) TOKENPASTE(x, y)
#define PADDING(size) uint8_t TOKENPASTE2(padding_, __LINE__)[size]