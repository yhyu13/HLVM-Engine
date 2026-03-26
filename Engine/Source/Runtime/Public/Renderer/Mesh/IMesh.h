/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#pragma once

#include "Math/MathGLM.h"
#include "Core/Container/ContainerDefinition.h"
#include "Definition/TypeDefinition.h"

/**
 * @brief Vertex format for static mesh (44 bytes)
 */
struct FVertex
{
	FVec3 Position; // 12 bytes
	FVec3 Normal;	// 12 bytes
	FVec2 UV;		// 8 bytes
	FVec3 Tangent;	// 12 bytes

	FVertex() = default;

	FVertex(const FVec3& InPosition, const FVec3& InNormal,
		const FVec2& InUV, const FVec3& InTangent)
		: Position(InPosition)
		, Normal(InNormal)
		, UV(InUV)
		, Tangent(InTangent)
	{
	}
};

/**
 * @brief Base interface for all mesh types
 *
 * Provides common mesh operations for both static and animated meshes.
 * Follows HLVM naming conventions: I prefix for interfaces.
 */
class IMesh
{
public:
	virtual ~IMesh() = default;

	//! Get mesh name
	const FString& GetName() const { return Name; }
	void		   SetName(const FString& InName) { Name = InName; }

	//! Check if mesh has valid data
	virtual bool IsValid() const = 0;

	//! Get vertex count
	virtual TUINT64 NumVertices() const = 0;

	//! Get index count
	virtual TUINT64 NumIndices() const = 0;

	//! Get triangle count
	virtual TUINT64 NumTriangles() const = 0;

	//! Get raw vertex data (for static meshes)
	virtual const TVector<FVertex>& GetVertices() const = 0;

	//! Get raw index data (for static meshes)
	virtual const TVector<uint32_t>& GetIndices() const = 0;

protected:
	FString Name;
};
