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

	struct Properties
	{
		FString Title = TXT("HLVM Window");
		EDisplayMode Mode = EDisplayMode::Default;
		bool	Resizable = true;
		EVsync	VSync = EVsync::Default;
		FUIntVec2 Extent = { 1280, 720 };
		// Add more properties, e.g. monitor perference
	};

	struct OptionalExtent
	{
		TOptional<TUINT32> Width;
		TOptional<TUINT32> Height;
	};

	struct OptionalProperties
	{
		TOptional<FString> Title;
		TOptional<EDisplayMode>   Mode;
		TOptional<bool>	   Resizable;
		TOptional<EVsync>  VSync;
		OptionalExtent	   Extent;
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
	HLVM_INLINE_FUNC FUIntVec2 Resize(const FUIntVec2& InExtent)
	{
		if (Property.Resizable)
		{
			Property.Extent = InExtent;
		}
		return Property.Extent;
	}

	HLVM_INLINE_FUNC const FUIntVec2& GetExtent() const
	{
		return Property.Extent;
	}

	HLVM_INLINE_FUNC EDisplayMode GetWindowMode() const
	{
		return Property.Mode;
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
	Properties Property;
	EWindowType Type;
};
