/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#pragma once

#include "AssimpHelper.h"
#include "Core/Assert.h"
#include "Core/Log.h"
#include "Platform/FileSystem/Path.h"
#include <memory>

// DECLARE_LOG_CATEGORY(LogScene3D) - declared in AssimpHelper.h

/**
 * @brief Assimp scene object wrapper
 * 
 * Loads and holds an Assimp scene from file.
 * Manages the aiScene lifecycle with proper cleanup.
 */
class FAssimpSceneObject
{
public:
	/**
	 * @brief Load scene from file
	 * @param Path File path to load
	 * @return Shared pointer to loaded scene object
	 */
	inline static std::shared_ptr<FAssimpSceneObject> LoadFromFile(const FPath& Path)
	{
		return std::make_shared<FAssimpSceneObject>(Path);
	}

	/**
	 * @brief Default constructor (deleted - must provide path)
	 */
	FAssimpSceneObject() = delete;

	/**
	 * @brief Load scene from file path
	 * @param Path File path to load
	 */
	explicit FAssimpSceneObject(const FPath& Path);

	/**
	 * @brief Destructor - releases Assimp scene
	 */
	~FAssimpSceneObject();

	/**
	 * @brief Get the loaded Assimp scene
	 * @return Pointer to aiScene (nullptr if not loaded)
	 */
	const aiScene* GetScene() const noexcept
	{
		return m_Scene;
	}

	/**
	 * @brief Get root node of the scene
	 * @return Pointer to root aiNode (nullptr if not loaded)
	 */
	const aiNode* GetRoot() const noexcept
	{
		HLVM_ENSURE_F(m_Scene != nullptr, TXT("Scene is not loaded"));
		return m_Scene->mRootNode;
	}

private:
	//! Loaded Assimp scene
	const aiScene* m_Scene{ nullptr };
};
