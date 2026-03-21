/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#pragma once

#include "WindowDefinition.h"
#include "Math/MathGLM.h"

HLVM_ENUM(EWindowType, TUINT8,
	GLFW3Vulkan);

/**
 * @brief An interface class, declaring the behavior of a Window
 */
class IWindow
{
public:
	enum class EDisplayMode
	{
		Windowed,
		Fullscreen,
		FullscreenBorderless,
	};

	enum class EVsync
	{
		Auto,
		Off,
		On,
	};

	struct Properties
	{
		FString		 Title = TXT("HLVM Window");
		EDisplayMode DisplayMode = EDisplayMode::Windowed;
		bool		 Resizable = true;
		bool		 StartMinimized = false;
		EVsync		 VSync = EVsync::Auto;
		FUInt2		 Extent = { 1280, 720 };
		FUInt2		 XY = { 100, 100 }; // Window position at start
										// Add more properties, e.g. monitor perference

		FString ToString() const
		{
			return FString::Format(
				TXT("Title: {0}, DisplayMode: {1}, Resizable: {2}, StartMinimized: {3}, VSync: {4}, Extent: {5}, XY: {6}"),
				Title,
				E2TCHAR(DisplayMode),
				Resizable,
				StartMinimized,
				E2TCHAR(VSync),
				::ToString(Extent),
				::ToString(XY));
		}
	};

public:
	NOCOPYMOVE(IWindow);
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
	HLVM_INLINE_FUNC FUInt2 Resize(const FUInt2& InExtent)
	{
		if (Property.Resizable)
		{
			Property.Extent = InExtent;
		}
		return Property.Extent;
	}

	HLVM_INLINE_FUNC const FUInt2& GetExtent() const
	{
		return Property.Extent;
	}

	HLVM_INLINE_FUNC EDisplayMode GetDisplayMode() const
	{
		return Property.DisplayMode;
	}

	HLVM_INLINE_FUNC const Properties& GetProperties() const
	{
		return Property;
	}

	HLVM_INLINE_FUNC EWindowType GetType() const
	{
		return Type;
	}

protected:
	Properties	Property;
	EWindowType Type;
};
