/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
#pragma clang diagnostic ignored "-Wold-style-cast"
#pragma clang diagnostic ignored "-Wcast-qual"
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wmissing-variable-declarations"
#pragma clang diagnostic ignored "-Wswitch-enum"
#pragma clang diagnostic ignored "-Wdocumentation-unknown-command"
#pragma clang diagnostic ignored "-Wimplicit-int-float-conversion"
#pragma clang diagnostic ignored "-Wshorten-64-to-32"
#pragma clang diagnostic ignored "-Wctad-maybe-unsupported"
#pragma clang diagnostic ignored "-Wshadow-field"
#pragma clang diagnostic ignored "-Wshadow-uncaptured-local"
#pragma clang diagnostic ignored "-Wimplicit-int-conversion"
#pragma clang diagnostic ignored "-Wcast-align"
#include "./../../ThirdParty/effil/channel.h"
#include "./../../ThirdParty/effil/function.h"
#include "./../../ThirdParty/effil/garbage-collector.h"
#include "./../../ThirdParty/effil/gc-data.h"
#include "./../../ThirdParty/effil/gc-object.h"
#include "./../../ThirdParty/effil/logger.h"
#include "./../../ThirdParty/effil/lua-helpers.h"
#include "./../../ThirdParty/effil/notifier.h"
#include "./../../ThirdParty/effil/shared-table.h"
#include "./../../ThirdParty/effil/spin-mutex.h"
#include "./../../ThirdParty/effil/stored-object.h"
#include "./../../ThirdParty/effil/this-thread.h"
#include "./../../ThirdParty/effil/thread-handle.h"
#include "./../../ThirdParty/effil/thread-runner.h"
#include "./../../ThirdParty/effil/threading.h"
#include "./../../ThirdParty/effil/utils.h"

extern "C"
#ifdef _WIN32
	__declspec(dllexport)
#endif
		int luaopen_effil(lua_State* L);
#pragma clang diagnostic pop
