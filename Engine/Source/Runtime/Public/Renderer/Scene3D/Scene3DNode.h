/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#pragma once

#include "Renderer/Mesh/IMesh.h"
#include "Renderer/Mesh/StaticMesh.h"
#include "Renderer/Material/IMaterial.h"
#include "Renderer/Material/PBRMaterial.h"


#include "Core/Container/ContainerDefinition.h"
#include <memory>
#include <functional>

class FScene3DLoader;

/**
 * @brief Representation of a scene node in the overall scene
 * 
 * A scene node contains a hierarchy of meshes with their materials.
 * Used to represent loaded 3D models from scene files (glTF, FBX, etc).
 */
class FScene3DNode
{
	friend FScene3DLoader;

public:
	//! Mesh hierarchy entry: (tree level, mesh shared pointer),
	//! choose vector over map since we need iterate through hierarchy
	using MeshHierarchy = TVector<std::pair<uint32_t, std::shared_ptr<IMesh>>>;

public:
	//! Default constructor
	FScene3DNode() = default;

	/**
	 * @brief Construct with scene node name
	 * @param Name Scene node name
	 */
	explicit FScene3DNode(const FString& Name)
		: SceneNodeName(Name)
	{
	}

	//! Destructor
	//! Destructor (explicitly defaulted to prevent deprecated copy warnings)
	~FScene3DNode() = default;

	//! Explicitly declare copy constructor to avoid deprecation warnings
	FScene3DNode(const FScene3DNode&) = default;
	FScene3DNode& operator=(const FScene3DNode&) = default;

	/**
	 * @brief Get all mesh data from the scene
	 * @return Vector of all mesh data shared pointers
	 */
	TVector<std::shared_ptr<IMesh>> GetAllMesh() const;

	/**
	 * @brief Get all mesh data from the scene
	 * @return Vector of all mesh data shared pointers
	 */
	TVector<std::shared_ptr<FStaticMesh>> GetAllStaticMesh() const;

	/**
	 * @brief Get all materials from the scene
	 * @return Vector of all material shared pointers
	 */
	TVector<std::shared_ptr<FPBRMaterial>> GetAllMaterial() const;

	/**
	 * @brief Get scene node name
	 * @return Scene node name
	 */
	const FString& GetName() const
	{
		return SceneNodeName;
	}

	/**
	 * @brief Check if scene is empty
	 * @return true if no meshes, false otherwise
	 */
	bool IsEmpty() const
	{
		return MeshTree.empty();
	}

	/**
	 * @brief Get number of meshes in the scene
	 * @return Mesh count
	 */
	TSIZE NumMeshes() const
	{
		return MeshTree.Num();
	}

	//! Iterator support (non-const)
	auto begin() { return MeshTree.begin(); }
	auto end() { return MeshTree.end(); }
	
	//! Iterator support (const)
	auto begin() const { return MeshTree.begin(); }
	auto end() const { return MeshTree.end(); }
	auto cbegin() const { return MeshTree.cbegin(); }
	auto cend() const { return MeshTree.cend(); }

public:
	//! Scene node name
	FString SceneNodeName{ TXT("None") };

	//! Mesh hierarchy tree: (level, mesh) pairs
	MeshHierarchy MeshTree;

	//! Mesh material map: (mesh, material) pairs ( one mesh multi material )
	TMap<std::shared_ptr<IMesh>, TSmallVector8<std::shared_ptr<FPBRMaterial>>> MeshMultiMaterialMap;

	//! Material mesh map: (material, mesh) pairs ( one material one mesh )
	TMap<std::shared_ptr<FPBRMaterial>, std::shared_ptr<IMesh>> MaterialOneMeshMap;
};
