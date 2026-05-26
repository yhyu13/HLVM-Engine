/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#pragma once

#include "Renderer/Mesh/IMesh.h"
#include "Core/Assert.h"

/**
 * @brief Static mesh implementation with vertex/index data
 *
 * Combines functionality of old FMesh and FMeshData.
 * Follows HLVM naming: F prefix for classes.
 */
class FStaticMesh : public IMesh
{
public:
	using VertexContainer = TVector<FVertex>;
	using IndexContainer = TVector<uint32_t>;

public:
	FStaticMesh() = default;

	explicit FStaticMesh(const FString& InName)
	{
		Name = InName;
	}

	FStaticMesh(const FString& InName, const VertexContainer& InVertices,
		const IndexContainer& InIndices)
		: Vertices(InVertices)
		, Indices(InIndices)
	{
		Name = InName;
	}

	~FStaticMesh() override = default;

	bool IsValid() const override
	{
		return !Vertices.empty() && !Indices.empty();
	}

	TUINT64 NumVertices() const override
	{
		return static_cast<TUINT64>(Vertices.Num());
	}

	TUINT64 NumIndices() const override
	{
		return static_cast<TUINT64>(Indices.Num());
	}

	TUINT64 NumTriangles() const override
	{
		return Indices.Num() / 3;
	}

	const VertexContainer& GetVertices() const override
	{
		return Vertices;
	}

	const IndexContainer& GetIndices() const override
	{
		return Indices;
	}

	// Helper methods
	void Clear()
	{
		Vertices.clear();
		Indices.clear();
	}

	void ReserveVertices(TSIZE Count)
	{
		Vertices.reserve(Count);
	}

	void ReserveIndices(TSIZE Count)
	{
		Indices.reserve(Count);
	}

	void AddVertex(const FVertex& Vertex)
	{
		Vertices.emplace_back(Vertex);
	}

	void AddTriangle(uint32_t Index0, uint32_t Index1, uint32_t Index2)
	{
		Indices.emplace_back(Index0);
		Indices.emplace_back(Index1);
		Indices.emplace_back(Index2);
	}

private:
	VertexContainer Vertices;
	IndexContainer	Indices;

public:
	// World transform for instancing support (set during scene loading)
	glm::mat4 WorldTransform{ 1.0f };
};
