/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "RHIDefinition.h"
#include "RHIResource.h"

// Enumeration of vertex element types
enum class EVertexElementType : TUINT8
{
	VET_Float1,		  // 1-component float
	VET_Float2,		  // 2-component float
	VET_Float3,		  // 3-component float
	VET_Float4,		  // 4-component float
	VET_Int1,		  // 1-component integer
	VET_Int2,		  // 2-component integer
	VET_Int3,		  // 3-component integer
	VET_Int4,		  // 4-component integer
	VET_UInt1,		  // 1-component unsigned integer
	VET_UInt2,		  // 2-component unsigned integer
	VET_UInt3,		  // 3-component unsigned integer
	VET_UInt4,		  // 4-component unsigned integer
	VET_PackedNormal, // Packed normal (4 bytes)
	VET_Color,		  // Color (4 bytes)
					  // Add other vertex element types as needed
};

// Enumeration of vertex element usage
enum class EVertexElementUsage : TUINT8
{
	Position,		   // Vertex position
	Normal,			   // Vertex normal
	Tangent,		   // Vertex tangent
	Binormal,		   // Vertex binormal
	Color,			   // Vertex color
	TextureCoordinate, // Texture coordinate
					   // Add other vertex element usages as needed
};

// Structure describing a single vertex element
struct FVertexElement
{
	TUINT16				StreamIndex; // The stream index (for multi-stream vertex buffers)
	TUINT16				Offset;		 // Offset in bytes from the start of the vertex
	EVertexElementType	Type;		 // Type of the vertex element (e.g., float, int)
	EVertexElementUsage Usage;		 // Usage of the vertex element (e.g., position, normal)
	TUINT8				UsageIndex;	 // Index for distinguishing between multiple elements with the same usage (e.g., multiple texture coordinates)

	// Constructor for easy initialization
	FVertexElement(TUINT16 InStreamIndex, TUINT16 InOffset, EVertexElementType InType, EVertexElementUsage InUsage, TUINT8 InUsageIndex = 0)
		: StreamIndex(InStreamIndex)
		, Offset(InOffset)
		, Type(InType)
		, Usage(InUsage)
		, UsageIndex(InUsageIndex)
	{
	}
};

// List of vertex elements (used to define a vertex layout)
using FVertexDeclarationElementList = TVector<FVertexElement>;

// Base class for RHI vertex declarations
class FRHIVertexDeclaration : virtual public FRHIResource
{
public:
	FRHIVertexDeclaration() = default;
	virtual ~FRHIVertexDeclaration() override = default;

	// Returns the type of the RHI resource
	virtual ERHIResourceType GetType() const override { return ERHIResourceType::VertexDeclaration; }

	// Returns the list of vertex elements
	virtual const FVertexDeclarationElementList& GetVertexElements() const = 0;
};

// Smart pointer type for RHI vertex declarations
using FVertexDeclarationRHIRef = TRefCountPtr<FRHIVertexDeclaration>;
