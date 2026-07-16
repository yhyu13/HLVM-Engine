/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#include "Common.h"
#include "Core/Log.h"

#if HLVM_SPDLOG_USE_ASYNC
// Initialize the thread pool for asynchronous logging
// Had to use global variable to avoid thread pool being released before program finishing
HLVM_STATIC_VAR std::shared_ptr<spdlog::details::thread_pool>* SpglogThreadPool = new std::shared_ptr<spdlog::details::thread_pool>(new spdlog::details::thread_pool(
	8192, 1, [] {}, [] {}));
#endif

FSpdlogConsoleDevice::FSpdlogConsoleDevice()
{
	constexpr auto pattern = "%^[%Y-%m-%d %H:%M:%S.%e] %l: %v%$";
	// Create the console sink
	auto						  stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
	std::vector<spdlog::sink_ptr> sinks{ stdout_sink };
#if HLVM_SPDLOG_USE_ASYNC
	// Create the asynchronous logger
	AsyncLogger = std::make_shared<spdlog::async_logger>("CONSOLE", sinks.begin(), sinks.end(), *SpglogThreadPool, spdlog::async_overflow_policy::block);
#else
	// Create the synchronous logger
	AsyncLogger = std::make_shared<spdlog::logger>("CONSOLE", sinks.begin(), sinks.end());
#endif
	// Set the log level
	AsyncLogger->set_level(spdlog::level::trace);
	AsyncLogger->set_pattern(pattern);
	// Register the logger
	spdlog::register_logger(AsyncLogger);

	// Create the error sink
	ImmediateLogger = std::make_shared<spdlog::logger>("CONSOLE_ERR", sinks.begin(), sinks.end());
	// Set the log level
	ImmediateLogger->set_level(spdlog::level::warn);
	AsyncLogger->set_pattern(pattern);
	// Register the logger
	spdlog::register_logger(ImmediateLogger);
}

FSpdlogConsoleDevice::~FSpdlogConsoleDevice()
{
	// Drop the logger
	spdlog::drop("CONSOLE");
	spdlog::drop("CONSOLE_ERR");
	// Set the logger to null
	AsyncLogger = nullptr;
	ImmediateLogger = nullptr;
}

// Log the message
HLVM_INLINE_FUNC void FSpdlogConsoleDevice::Sink(const FLogContext& Context, const FString& Message) const
{
	// Get the loggers
	const auto& Logger = (S_C(int, Context.LogLevel) >= S_C(int, spdlog::level::warn)) ? ImmediateLogger : AsyncLogger;
	// Log the message
	Logger->log(Context.LogLevel, TO_CHAR_CSTR(Message.c_str()));
}

FSpdlogFileDevice::FSpdlogFileDevice()
{
	constexpr auto pattern = "%^[%Y-%m-%d %H:%M:%S.%e] %l: %v%$";

	// Build log file path: {GExecutableDirectory}/{GExecutableName}.log
	const FPath LogFilePath = FPath::Combine(GExecutableDirectory, FString::Format(TXT("{}.log"), *GExecutableName));
	const FPath LogFilePath1 = FPath::Combine(GExecutableDirectory, FString::Format(TXT("{}_1.log"), *GExecutableName));
	const FPath LogFilePath2 = FPath::Combine(GExecutableDirectory, FString::Format(TXT("{}_2.log"), *GExecutableName));

	// Rotate existing logs on startup: keep up to 3 files (current + 2 backups), no per-file size limit
	if (FPath::Exists(LogFilePath2))
	{
		boost::filesystem::remove(LogFilePath2);
	}
	if (FPath::Exists(LogFilePath1))
	{
		boost::filesystem::rename(LogFilePath1, LogFilePath2);
	}
	if (FPath::Exists(LogFilePath))
	{
		boost::filesystem::rename(LogFilePath, LogFilePath1);
	}

	// Create the file sink (truncate to start a fresh log each run)
	auto					  file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(LogFilePath.ToCharCStr(), true);
	std::vector<spdlog::sink_ptr> sinks{ file_sink };
#if HLVM_SPDLOG_USE_ASYNC
	// Create the asynchronous logger
	AsyncLogger = std::make_shared<spdlog::async_logger>("FILE", sinks.begin(), sinks.end(), *SpglogThreadPool, spdlog::async_overflow_policy::block);
#else
	// Create the synchronous logger
	AsyncLogger = std::make_shared<spdlog::logger>("FILE", sinks.begin(), sinks.end());
#endif
	// Set the log level
	AsyncLogger->set_level(spdlog::level::trace);
	AsyncLogger->set_pattern(pattern);
	// Register the logger
	spdlog::register_logger(AsyncLogger);

	// Create the error logger
	ImmediateLogger = std::make_shared<spdlog::logger>("FILE_ERR", sinks.begin(), sinks.end());
	// Set the log level
	ImmediateLogger->set_level(spdlog::level::warn);
	ImmediateLogger->set_pattern(pattern);
	// Register the logger
	spdlog::register_logger(ImmediateLogger);
}

FSpdlogFileDevice::~FSpdlogFileDevice()
{
	// Drop the logger
	spdlog::drop("FILE");
	spdlog::drop("FILE_ERR");
	// Set the logger to null
	AsyncLogger = nullptr;
	ImmediateLogger = nullptr;
}

// Log the message
HLVM_INLINE_FUNC void FSpdlogFileDevice::Sink(const FLogContext& Context, const FString& Message) const
{
	// Get the loggers
	const auto& Logger = (S_C(int, Context.LogLevel) >= S_C(int, spdlog::level::warn)) ? ImmediateLogger : AsyncLogger;
	// Log the message
	Logger->log(Context.LogLevel, TO_CHAR_CSTR(Message.c_str()));
}
