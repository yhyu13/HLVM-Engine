/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#pragma once
#include "Common.h"
#include "String.h"

#include <fmt/xchar.h>
#define SPDLOG_ACTIVE_LEVEL 0
#define HLVM_SPDLOG_USE_ASYNC 1 //! HLVM_BUILD_DEBUG
#include <spdlog/spdlog.h>
#if HLVM_SPDLOG_USE_ASYNC
	#include <spdlog/async.h>
#endif
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>

#include <memory>
#include <forward_list>
#include <atomic>

struct FLogCatgegory
{
	NOCOPY(FLogCatgegory)
	FLogCatgegory() = delete;
	explicit FLogCatgegory(const TCHAR* CategoryName, const spdlog::level::level_enum Level = spdlog::level::trace)
		: Name(CategoryName), LogLevel(Level)
	{
	}
	const TCHAR*			  Name;
	spdlog::level::level_enum LogLevel;
};

// Macro for declare a log category
#define DELCARE_LOG_CATEGORY(category) \
	extern std::unique_ptr<FLogCatgegory> category;

DELCARE_LOG_CATEGORY(LogTemp)
DELCARE_LOG_CATEGORY(LogEngine)
DELCARE_LOG_CATEGORY(LogGame)
DELCARE_LOG_CATEGORY(LogEditor)

// Define a logger category in Log.cpp file or other .cpp file
#define DEFINE_LOG_CATEGORY(category) \
	std::unique_ptr<FLogCatgegory> category = std::make_unique<FLogCatgegory>(TXT(#category));
#define DEFINE_LOG_CATEGORY2(category, _level) \
	std::unique_ptr<FLogCatgegory> category = std::make_unique<FLogCatgegory>(TXT(#category), spdlog::level::_level);

/**
 * @brief FLogContext is a structure that contains information about a log message,
	including the log category, log level, file name, and line number.
 *
 */
struct FLogContext
{
	const FLogCatgegory*	  Category;
	spdlog::level::level_enum LogLevel;
	const TCHAR*			  FileName;
	int						  Line;
};

/**
 * @brief FLogDevice is designed to be extended by different log device classes,
	and the Sink function should be implemented accordingly to log messages to the specific device type
*/
class FLogDevice
{
public:
	NOCOPY(FLogDevice)
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
	bool bEnable = true;
};

/**
 * @brief FLogRedirector is a singleton class that manages all log devices.
 *
 */
class FLogRedirector
{
public:
	using ContainerType = std::forward_list<std::shared_ptr<FLogDevice>>;

	NOCOPY(FLogRedirector)
	FLogRedirector() = default;

	static FLogRedirector* Get()
	{
		static FLogRedirector* instance = new FLogRedirector();
		return instance;
	}

	// Formats the message before sending it to the sink
	template <typename... Args>
	static FString FormatBeforeSink(const FLogContext& Context, const TCHAR* fmt, Args&&... args)
	{
		FString Message = FString::Format(TXT("{0}:[{2}:{3}] {1}"), Context.Category->Name, fmt, Context.FileName, Context.Line);
		return FString::Format(*Message, std::forward<Args>(args)...);
	}

	// Sends the message to all devices
	template <typename... Args>
	void Pump(const FLogContext& Context, const TCHAR* fmt, Args&&... args)
	{
		FString Message;
		for (auto& Device : LogDevices)
		{
			// Send to all devices
			if (Device->AllowSink(Context))
			{
				// If the message is empty, format it first, and reuse it
				if (Message.empty())
				{
					Message = MoveTemp(FormatBeforeSink(Context, fmt, std::forward<Args>(args)...));
				}
				Device->Sink(Context, Message);
			}
		}
	}

	// Adds a new device to the list of devices
	void AddDevice(const std::shared_ptr<FLogDevice>& Device)
	{
		LogDevices.push_front(Device);
	}

	ContainerType AllDevices() const
	{
		return LogDevices;
	}

private:
	ContainerType LogDevices;
};

// Macro for logging with category
#define HLVM_LOG(_Category, _level, fmt, ...)                                                 \
	FLogRedirector::Get()->Pump(FLogContext{                                                  \
									.Category = static_cast<FLogCatgegory*>(_Category.get()), \
									.LogLevel = spdlog::level::_level,                        \
									.FileName = __FILENAME__,                                 \
									.Line = __LINE__ },                                       \
		fmt, ##__VA_ARGS__)

/**
 * @brief FSpdlogConsoleDevice is a log device that logs to the console.
 *
 */
class FSpdlogConsoleDevice final : public FLogDevice
{
public:
	NOCOPY(FSpdlogConsoleDevice)

	FSpdlogConsoleDevice();
	~FSpdlogConsoleDevice();

	// Log the message
	virtual void Sink(const FLogContext& Context, const FString& Message) const override;

public:
	// The asynchronous logger
	std::shared_ptr<spdlog::logger> AsyncLogger;
	// The error logger
	std::shared_ptr<spdlog::logger> ImmediateLogger;
};