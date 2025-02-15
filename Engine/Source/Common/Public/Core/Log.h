/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once
#include "Common.h"
#include "String.h"
#include "Core/Parallel/ParallelDefinition.h"
#include "Template/ExpressionTemplate.tpp"

#include <fmt/xchar.h>

#ifdef SPDLOG_ACTIVE_LEVEL
	// Undefine spdlog default SPDLOG_ACTIVE_LEVEL
	#undef SPDLOG_ACTIVE_LEVEL
#endif
#define SPDLOG_ACTIVE_LEVEL 0
#define HLVM_SPDLOG_USE_ASYNC (!HLVM_BUILD_DEBUG)
#include <spdlog/spdlog.h>
#if HLVM_SPDLOG_USE_ASYNC
	#include <spdlog/async.h>
#endif
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>

#include <forward_list>

struct FLogCatgegory
{
	NOCOPYMOVE(FLogCatgegory)
	FLogCatgegory() = delete;
	constexpr explicit FLogCatgegory(const TCHAR* CategoryName,
		const spdlog::level::level_enum			  MinimumLogLevel =
#if !HLVM_BUILD_RELEASE
			spdlog::level::trace
#else // Release build forbid all traces logs to reduce log frequences, change this behavior as your need though
			spdlog::level::info
#endif
		)
		: Name(CategoryName), LogLevel(MinimumLogLevel)
	{
	}
	const TCHAR*					Name;
	const spdlog::level::level_enum LogLevel; // Minimum Log level, below this level will ignore
};

// Macro for declare & define a log category, each unique log category should only call this macro once
#define DECLARE_LOG_CATEGORY(category) \
	HLVM_INLINE_VAR constexpr FLogCatgegory category = FLogCatgegory(TXT(#category));
#define DELCARE_LOG_CATEGORY2(category, _level) \
	HLVM_INLINE_VAR constexpr FLogCatgegory category = FLogCatgegory(TXT(#category), spdlog::level::_level);

/**
 * Define basic log categories
 */
// LogCrashDump is used for assertion
DECLARE_LOG_CATEGORY(LogCrashDump)
// Use LogTemp as default log category if you don't know what to use
DECLARE_LOG_CATEGORY(LogTemp)

/**
 * @brief FLogContext is a structure that contains information about a log message,
	including the log category, log level, file name, and line number.
 *
 */
struct FLogContext
{
	const FLogCatgegory*			Category;
	const spdlog::level::level_enum LogLevel;
	const TCHAR*					FileName;
	const int						Line;
};

/**
 * @brief FLogDevice is designed to be extended by different log device classes,
	and the Sink function should be implemented accordingly to log messages to the specific device type
*/
class FLogDevice
{
public:
	NOCOPYMOVE(FLogDevice)
	FLogDevice() = default;
	// Virtual destructor
	virtual ~FLogDevice() = default;

	// Log to device
	virtual void Sink(const FLogContext& Context, const FString& Message) const = 0;

	// Check if the log should be sent to this device
	bool AllowSink(const FLogContext& Context) const
	{
		// Check if the log level is higher than the category's log level, and the log level is not off
		return bEnable
			&& static_cast<int>(Context.LogLevel) >= static_cast<int>(Context.Category->LogLevel)
			&& static_cast<int>(Context.LogLevel) != SPDLOG_LEVEL_OFF;
	}

	void Disable()
	{
		bEnable = false;
	}

	void Enable()
	{
		bEnable = true;
	}

protected:
	BIT_FLAG(bEnable){ true };
};

/**
 * @brief FLogRedirector is a singleton class that manages all log devices.
 *
 */
class FLogRedirector
{
public:
	using ContainerType = std::forward_list<std::shared_ptr<FLogDevice>>;

	NOCOPYMOVE(FLogRedirector)
	FLogRedirector() = default;

	static FLogRedirector* Get()
	{
		static FLogRedirector instance = FLogRedirector();
		return &instance;
	}

	// Formats the message before sending it to the sink
	template <typename... Args>
	static FString FormatBeforeSink(const FLogContext& Context, const TCHAR* fmt, Args&&... args)
	{
#if 1
		FString Message = FString::Format(TXT("T[{4:#x}] {0}:[{2}:{3}] {1}"),
			Context.Category->Name,
			fmt,
			Context.FileName,
			Context.Line,
			GCurrentTID64);
#else
		FString Message = FString::Format(TXT("{0}:[{2}:{3}] {1}"), Context.Category->Name, fmt, Context.FileName, Context.Line);
#endif
		// check if args num is zero
		if constexpr (sizeof...(args) == 0)
		{
			return Message;
		}
		else
		{
			return FString::Format(*Message, std::forward<Args>(args)...);
		}
	}

	// Sends the message to all devices
	template <typename... Args>
	void Pump(const FLogContext& Context, const TCHAR* fmt, Args&&... args)
	{
		FString ReusedMessage;
		for (auto& Device : LogDevices)
		{
			// Send to all devices
			if (Device->AllowSink(Context))
			{
				// If the message is empty, format it first, and reuse it
				if (ReusedMessage.empty())
				{
					ReusedMessage = MoveTemp(FormatBeforeSink(Context, fmt, std::forward<Args>(args)...));
				}
				Device->Sink(Context, ReusedMessage);
			}
		}
	}

	// Adds a new device to the list of devices
	void AddDevice(const std::shared_ptr<FLogDevice>& Device)
	{
		LogDevices.push_front(Device);
	}

	template <typename T>
	void AddDevice()
	{
		std::shared_ptr<FLogDevice> Device = SP_C(FLogDevice, std::make_shared<T>());
		LogDevices.push_front(MoveTemp(Device));
	}

	ContainerType AllDevices() const
	{
		return LogDevices;
	}

private:
	ContainerType LogDevices;
};

// Macro for logging with category
#define HLVM_LOG(_Category, _level, fmt, ...)                                                          \
	do                                                                                                 \
	{                                                                                                  \
		if constexpr (static_cast<int>(spdlog::level::_level) >= static_cast<int>(_Category.LogLevel)) \
			FLogRedirector::Get()                                                                      \
				->Pump(FLogContext{                                                                    \
						   .Category = &_Category,                                                     \
						   .LogLevel = spdlog::level::_level,                                          \
						   .FileName = TO_TCHAR_CSTR(&std::string_view(ct_strrchr(__FILE__, '/'))[1]), \
						   .Line = __LINE__ },                                                         \
					fmt, ##__VA_ARGS__);                                                               \
	}                                                                                                  \
	while (0)

#define HLVM_CLOG(_COND, _Category, _level, fmt, ...)                                                      \
	do                                                                                                     \
	{                                                                                                      \
		if constexpr (static_cast<int>(spdlog::level::_level) >= static_cast<int>(_Category.LogLevel))     \
			if (static_cast<bool>(_COND))                                                                  \
				FLogRedirector::Get()                                                                      \
					->Pump(FLogContext{                                                                    \
							   .Category = &_Category,                                                     \
							   .LogLevel = spdlog::level::_level,                                          \
							   .FileName = TO_TCHAR_CSTR(&std::string_view(ct_strrchr(__FILE__, '/'))[1]), \
							   .Line = __LINE__ },                                                         \
						fmt, ##__VA_ARGS__);                                                               \
	}                                                                                                      \
	while (0)

#define HLVM_CLOG_ELSE_FATAL(_COND, _Category, _level, fmt, ...)                                                     \
	do                                                                                                               \
	{                                                                                                                \
		if constexpr (static_cast<int>(spdlog::level::_level) >= static_cast<int>(_Category.LogLevel))               \
			FLogRedirector::Get()                                                                                    \
				->Pump(FLogContext{                                                                                  \
						   .Category = &_Category,                                                                   \
						   .LogLevel = (static_cast<bool>(_COND)) ? spdlog::level::_level : spdlog::level::critical, \
						   .FileName = TO_TCHAR_CSTR(&std::string_view(ct_strrchr(__FILE__, '/'))[1]),               \
						   .Line = __LINE__ },                                                                       \
					fmt, ##__VA_ARGS__);                                                                             \
	}                                                                                                                \
	while (0)

/**
 * @brief FSpdlogConsoleDevice is a log device that logs to the console.
 *
 */
class FSpdlogConsoleDevice final : public FLogDevice
{
public:
	NOCOPYMOVE(FSpdlogConsoleDevice)

	FSpdlogConsoleDevice();
	~FSpdlogConsoleDevice() override;

	// Log the message
	virtual void Sink(const FLogContext& Context, const FString& Message) const override;

public:
	// The asynchronous logger
	std::shared_ptr<spdlog::logger> AsyncLogger;
	// The error logger
	std::shared_ptr<spdlog::logger> ImmediateLogger;
};
