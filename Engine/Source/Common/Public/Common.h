/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

/**
 * Defintion
 */
#include "GlobalDefinition.h"
#include "Platform/PlatformDefinition.h"
#include "Core/Container/ContainerDefinition.h"
#include "Core/Parallel/ParallelDefinition.h"

#include "Template/GlobalTemplate.tpp"

#include "Global.h"

/**
 * 3rd party includes
 */
#include <ylt/struct_pack.hpp>
#include <ylt/struct_json/json_reader.h>
#include <ylt/struct_json/json_writer.h>
#include <ylt/thirdparty/async_simple/coro/Lazy.h>
#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <magic_enum_all.hpp>

#define BOOST_STACKTRACE_USE_BACKTRACE
/**
 * Boost include
 */
#include <boost/stacktrace.hpp>
#include <boost/filesystem.hpp>

#if !HLVM_SHIPPING
	#if defined(__clang__) || defined(__GNUC__)
		#define TracyFunction __PRETTY_FUNCTION__
	#elif defined(_MSC_VER)
		#define TracyFunction __FUNCSIG__
	#endif
	#include <tracy/Tracy.hpp>
	#include <tracy/TracyC.h>
#endif
