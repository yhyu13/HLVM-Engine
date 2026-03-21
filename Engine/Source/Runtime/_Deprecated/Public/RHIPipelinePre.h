/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#pragma once

#include "RHIDefinition.h"
#include "RHIResource.h"

// Structure describing a single vertex element
struct FVertexElement
{
	TUINT8								  StreamIndex; // The stream index (for multi-stream vertex buffers)
	TUINT8								  Offset;	   // Offset in bytes from the start of the vertex
	TEnumAsUnderlying<EVertexElementType> Type;		   // Type of the vertex element (e.g., float, int)
	TUINT8								  AttributeIndex;
	TUINT16								  Stride;
	TUINT8								  bUseInstance;

	// Constructor for easy initialization
	FVertexElement(
		TUINT8								  InStreamIndex,
		TUINT8								  InOffset,
		TEnumAsUnderlying<EVertexElementType> InType,
		TUINT8								  InAttributeIndex,
		TUINT16								  InStride,
		TUINT8								  InUseInstance = 0)
		: StreamIndex(InStreamIndex)
		, Offset(InOffset)
		, Type(InType)
		, AttributeIndex(InAttributeIndex)
		, Stride(InStride)
		, bUseInstance(InUseInstance)
	{
	}
};

// List of vertex elements (used to define a vertex layout)
using FVertexDeclarationElementList = TVector<FVertexElement>;

// Base class for RHI vertex declarations
class FRHIVertexDeclaration : virtual public IRHIResource
{
public:
	FRHIVertexDeclaration() = default;
	virtual ~FRHIVertexDeclaration() override = default;

	// Returns the type of the RHI resource
	virtual ERHIResourceType GetResourceType() const override { return ERHIResourceType::VertexDeclaration; }

	// Returns the list of vertex elements
	virtual const FVertexDeclarationElementList& GetVertexElements() const = 0;
};

// Smart pointer type for RHI vertex declarations
using FVertexDeclarationRHIRef = TRefCountPtr<FRHIVertexDeclaration>;

struct FBoundShaderStateInput
{
	FBoundShaderStateInput() = default;

	FBoundShaderStateInput(
		FVertexDeclarationRHIRef InVertexDeclarationRHI, FRHIShaderRef InVertexShaderRHI, FRHIShaderRef InPixelShaderRHI
#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
		,
		FRHIShaderRef InGeometryShaderRHI
#endif
		)
		: VertexDeclarationRHI(InVertexDeclarationRHI)
		, VertexShaderRHI(InVertexShaderRHI)
		, PixelShaderRHI(InPixelShaderRHI)
#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
		, GeometryShaderRHI(InGeometryShaderRHI)
#endif
	{
	}

#if PLATFORM_SUPPORTS_MESH_SHADERS
	FBoundShaderStateInput(
		FRHIShaderRef InMeshShaderRHI,
		FRHIShaderRef InTaskShader,
		FRHIShaderRef InPixelShaderRHI)
		: PixelShaderRHI(InPixelShaderRHI)
		, MeshShaderRHI(InMeshShaderRHI)
		, TaskShaderRHI(InTaskShader)
	{
	}
#endif

	FRHIShaderRef GetVertexShader() const { return VertexShaderRHI; }
	FRHIShaderRef GetPixelShader() const { return PixelShaderRHI; }

#if PLATFORM_SUPPORTS_MESH_SHADERS
	FRHIShaderRef GetMeshShader() const { return MeshShaderRHI; }
	void		  SetMeshShader(FRHIShaderRef InMeshShader) { MeshShaderRHI = InMeshShader; }
	FRHIShaderRef GetTaskShader() const { return TaskShaderRHI; }
	void		  SetTaskShader(FRHIShaderRef InTaskShader) { TaskShaderRHI = InTaskShader; }
#else
	constexpr FRHIShaderRef GetMeshShader() const { return nullptr; }
	void					SetMeshShader(FRHIShaderRef) {}
	constexpr FRHIShaderRef GetTaskShader() const { return nullptr; }
	void					SetTaskShader(FRHIShaderRef) {}
#endif

#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
	FRHIShaderRef GetGeometryShader() const { return GeometryShaderRHI; }
	void		  SetGeometryShader(FRHIShaderRef InGeometryShader) { GeometryShaderRHI = InGeometryShader; }
#else
	constexpr FRHIShaderRef GetGeometryShader() const { return nullptr; }
	void					SetGeometryShader(FRHIShaderRef) {}
#endif

	FVertexDeclarationRHIRef VertexDeclarationRHI = nullptr;
	FRHIShaderRef			 VertexShaderRHI = nullptr;
	FRHIShaderRef			 PixelShaderRHI = nullptr;

private:
#if PLATFORM_SUPPORTS_MESH_SHADERS
	FRHIShaderRef MeshShaderRHI = nullptr;
	FRHIShaderRef TaskShaderRHI = nullptr;
#endif
#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
	FRHIShaderRef GeometryShaderRHI = nullptr;
#endif
};
