/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once

/**
 * Global Defintion
 */
#include "GlobalDefinition.h"
/**
 * Templates
 */
#include "Template/GlobalTemplate.tpp"

/**
 * Module Definition
 */
#include "Platform/PlatformDefinition.h"
#include "Core/Container/ContainerDefinition.h"
#include "Core/Parallel/ParallelDefinition.h"
#include "Core/Scripting/Lua/Sol.h"
#include "Utility/Profiler/ProfilerDefinition.h"

/**
 * Global
 */
#include "Global.h"

/**
 * 3rd party includes for Common
 */
#include <ylt/struct_pack.hpp>
#include <ylt/struct_json/json_reader.h>
#include <ylt/struct_json/json_writer.h>
#include <ylt/thirdparty/async_simple/coro/Lazy.h>
#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <magic_enum_all.hpp>

/**
 * Boost include
 */
#define BOOST_STACKTRACE_USE_BACKTRACE
#include <boost/stacktrace.hpp>
#include <boost/filesystem.hpp>
