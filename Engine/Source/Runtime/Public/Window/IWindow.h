/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "WindowDefinition.h"

HLVM_ENUM(EWindowType, TUINT8,
	NoRender,
	HeadlessVulkan,
	GLFW3Vulkan
	);

/**
 * @brief An interface class, declaring the behavior of a Window
 */
class IWindow
{
public:
	struct FExtent
	{
		TUINT32 Width;
		TUINT32 Height;
	};

	enum class EDisplayMode
	{
		NoRender,
		Headless,
		Fullscreen,
		FullscreenBorderless,
		FullscreenStretch,
		Windowed,
		Default = FullscreenBorderless
	};

	enum class EVsync
	{
		Off,
		On,
		Default = Off
	};

	struct FProperties
	{
		FString Title = TXT("HLVM Window");
		EDisplayMode Mode = EDisplayMode::Default;
		bool	Resizable = true;
		EVsync	VSync = EVsync::Default;
		FExtent Extent = { 1280, 720 };
		// Add more properties, e.g. monitor perference
	};

	struct FOptionalExtent
	{
		TOptional<TUINT32> Width;
		TOptional<TUINT32> Height;
	};

	struct FOptionalProperties
	{
		TOptional<FString> Title;
		TOptional<EDisplayMode>   Mode;
		TOptional<bool>	   Resizable;
		TOptional<EVsync>  VSync;
		FOptionalExtent	   Extent;
	};

public:
	NOCOPYMOVE(IWindow)
	IWindow() = default;
	virtual ~IWindow() = default;

	/**
	 * @brief Checks if the window should be closed
	 */
	virtual bool ShouldClose() = 0;

	/**
	 * @brief Handles the processing of all underlying window events
	 */
	virtual void ProcessEvents() = 0;

	/**
	 * @brief Requests to close the window
	 */
	virtual void Close() = 0;

	/**
	 * @return The dot-per-inch scale factor
	 */
	virtual float GetDPIScaleFactor() const = 0;

	/**
	 * @return The scale factor for systems with heterogeneous window and pixel coordinates
	 */
	virtual float GetContentScaleFactor() const = 0;

	/**
	 * @brief Attempt to resize the window - not guaranteed to change
	 *
	 * @param InExtent The preferred window extent
	 * @return FExtent The new window extent
	 */
	HLVM_INLINE_FUNC FExtent Resize(const FExtent& InExtent)
	{
		if (Properties.Resizable)
		{
			Properties.Extent = InExtent;
		}
		return Properties.Extent;
	}

	HLVM_INLINE_FUNC const FExtent& GetExtent() const
	{
		return Properties.Extent;
	}

	HLVM_INLINE_FUNC EDisplayMode GetWindowMode() const
	{
		return Properties.Mode;
	}

	HLVM_INLINE_FUNC const FProperties& GetProperties() const
	{
		return Properties;
	}

	HLVM_INLINE_FUNC EWindowType GetType() const
	{
		return Type;
	}

protected:
	FProperties Properties;
	EWindowType Type;
};
