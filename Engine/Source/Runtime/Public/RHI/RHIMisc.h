// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RHIDefinition.h"

// Structure for specifying clear values for render targets and depth-stencil targets
struct FClearValueBinding
{
	// Union to store either a color clear value or a depth-stencil clear value
	union
	{
		struct
		{
			float R, G, B, A; // Color clear value (RGBA)
		} Color;

		struct
		{
			float Depth;      // Depth clear value
			TUINT8 Stencil;    // Stencil clear value
		} DepthStencil;
	} ClearValue;

	// Enumeration to specify the type of clear value
	enum class EClearType : TUINT8
	{
		None,       // No clear value
		Color,      // Color clear value
		DepthStencil// Depth-stencil clear value
	};

	EClearType ClearType; // Type of clear value

	// Default constructor (no clear value)
	FClearValueBinding()
		: ClearType(EClearType::None)
	{
		std::memset(&this->ClearValue, 0, sizeof(ClearValue));
	}

	// Constructor for color clear value
	FClearValueBinding(float InR, float InG, float InB, float InA)
		: ClearType(EClearType::Color)
	{
		ClearValue.Color.R = InR;
		ClearValue.Color.G = InG;
		ClearValue.Color.B = InB;
		ClearValue.Color.A = InA;
	}

	// Constructor for depth-stencil clear value
	FClearValueBinding(float InDepth, TUINT8 InStencil)
		: ClearType(EClearType::DepthStencil)
	{
		ClearValue.DepthStencil.Depth = InDepth;
		ClearValue.DepthStencil.Stencil = InStencil;
	}

	// Static helper functions for common clear values
	static FClearValueBinding None() { return FClearValueBinding(); }
	static FClearValueBinding Transparent() { return FClearValueBinding(0.0f, 0.0f, 0.0f, 0.0f); }
	static FClearValueBinding Black() { return FClearValueBinding(0.0f, 0.0f, 0.0f, 1.0f); }
	static FClearValueBinding White() { return FClearValueBinding(1.0f, 1.0f, 1.0f, 1.0f); }
	static FClearValueBinding DepthOne() { return FClearValueBinding(1.0f, 0); }
	static FClearValueBinding DepthZero() { return FClearValueBinding(0.0f, 0); }

	// Equality operator
	bool operator==(const FClearValueBinding& Other) const
	{
		if (ClearType != Other.ClearType)
		{
			return false;
		}

		switch (ClearType)
		{
			case EClearType::Color:
				return ClearValue.Color.R == Other.ClearValue.Color.R &&
					ClearValue.Color.G == Other.ClearValue.Color.G &&
					ClearValue.Color.B == Other.ClearValue.Color.B &&
					ClearValue.Color.A == Other.ClearValue.Color.A;

			case EClearType::DepthStencil:
				return ClearValue.DepthStencil.Depth == Other.ClearValue.DepthStencil.Depth &&
					ClearValue.DepthStencil.Stencil == Other.ClearValue.DepthStencil.Stencil;

			case EClearType::None:
				return false;
		}
	}

	// Inequality operator
	bool operator!=(const FClearValueBinding& Other) const
	{
		return !(*this == Other);
	}
};
