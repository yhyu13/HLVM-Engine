/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#pragma once

#include "Assimp/AssimpSceneObject.h"
#include "Renderer/Scene3D/Scene3DNode.h"
#include "Platform/FileSystem/Path.h"

/**
 * @brief Static scene loader for 3D scene files
 *
 * Simplified version of Scene3DManager without ResourceManager dependencies.
 * Uses static methods instead of singleton pattern.
 */
class FScene3DLoader
{
public:
	struct AssetLoadingContext
	{
		AssetLoadingContext()
			: Level(0u)
		{
		}
		mutable TUINT32												Level;
		mutable TMap<const aiMesh*, std::shared_ptr<IMesh>>			MeshMeshMap;
		mutable TMap<const aiMaterial*, std::shared_ptr<IMaterial>> MaterialMaterialMap;
	};

public:
	//! Load scene from file path
	static std::shared_ptr<FScene3DNode> LoadFromFile(const FPath& ScenePath);

	//! Load scene with custom name
	static std::shared_ptr<FScene3DNode> LoadFromAssimp(
		const FPath&   ScenePath,
		const FString& SceneName = TXT(""));

private:
	//! Recursive scene graph loading helper
	static void RecurseLoad(
		FScene3DNode&			   SceneData,
		const FString&			   SceneName,
		const FPath&			   SceneDir,
		const aiScene*			   AIScene,
		const aiNode*			   Node,
		const aiMatrix4x4&		   ParentTr = FAssimpHelper::Glm2Assimp(FMat4(1.0f)),
		const AssetLoadingContext& LoadingContext = AssetLoadingContext());
};
