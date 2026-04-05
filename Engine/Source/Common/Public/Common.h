/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#pragma once

/**
 * Global
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
#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <magic_enum_all.hpp>

/**
 * Boost include
 */
#define BOOST_STACKTRACE_USE_BACKTRACE
#include <boost/stacktrace.hpp>
#include <boost/filesystem.hpp>

/**
 * Minimal
 */
#include "CommonMinimal.h"
