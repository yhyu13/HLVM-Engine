/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 *
 * Simple Scene Graph Test Suite for HLVM-Engine
 *
 * Minimal test cases for the scene graph system.
 */

#include "Test.h"

#include "Renderer/Scene3D/Scene3DLoader.h"
#include "Renderer/Scene3D/Scene3DNode.h"
#include "Renderer/Mesh/StaticMesh.h"
#include "Renderer/Material/PBRMaterial.h"
#include "Renderer/Mesh/IMesh.h"
#include "Renderer/Material/IMaterial.h"
#include "Platform/FileSystem/Path.h"
#include "Core/Log.h"

DECLARE_LOG_CATEGORY(LogTest)

/**
 * @brief Test basic scene loading from glTF file
 */
RECORD(scene_load_basic)
{
    HLVM_LOG(LogTest, info, TXT("Testing basic scene loading"));

	const FString GitRoot = FString::Format(TXT("{}/../../../../.."),*GExecutablePath);
    const FPath scenePath = FPath(FString::Format(TXT("{}{}"), *GitRoot, TXT("/Samples/Assets/sponza/Sponza01.gltf")));
    
    // Load scene using Scene3DLoader
    std::shared_ptr<FScene3DNode> scene = FScene3DLoader::LoadFromFile(scenePath);
    
    // Basic validation
    HLVM_ENSURE(scene != nullptr);
    HLVM_ENSURE(!scene->GetName().empty());
    HLVM_ENSURE(scene->GetAllMesh().size() > 0);
    HLVM_ENSURE(scene->GetAllMaterial().size() > 0);
    
    HLVM_LOG(LogTest, info, TXT("Loaded scene: {} meshes, {} materials"), 
             scene->GetAllMesh().size(), scene->GetAllMaterial().size());
}

/**
 * @brief Test mesh data extraction and format validation
 */
RECORD(mesh_data_validation)
{
    HLVM_LOG(LogTest, info, TXT("Testing mesh data validation"));
    
	const FString GitRoot = FString::Format(TXT("{}/../../../../.."),*GExecutablePath);
	const FPath scenePath = FPath(FString::Format(TXT("{}{}"), *GitRoot, TXT("/Samples/Assets/sponza/Sponza01.gltf")));
    std::shared_ptr<FScene3DNode> scene = FScene3DLoader::LoadFromFile(scenePath);
    
    auto meshes = scene->GetAllMesh();
    HLVM_ENSURE(!meshes.empty());
    
    // Validate first mesh
    auto firstMesh = meshes[0];
    HLVM_ENSURE(firstMesh != nullptr);
    
    // Check mesh interface methods
    //HLVM_ENSURE(!firstMesh->GetName().empty());
    HLVM_ENSURE(firstMesh->NumVertices() > 0);
    HLVM_ENSURE(firstMesh->NumIndices() > 0);
    
    HLVM_LOG(LogTest, info, TXT("Mesh validation passed: {} vertices, {} indices"),
             firstMesh->NumVertices(), firstMesh->NumIndices());
}

/**
 * @brief Test PBR material extraction and properties
 */
RECORD(material_system_validation)
{
    HLVM_LOG(LogTest, info, TXT("Testing material system"));
    
	const FString GitRoot = FString::Format(TXT("{}/../../../../.."),*GExecutablePath);
	const FPath scenePath = FPath(FString::Format(TXT("{}{}"), *GitRoot, TXT("/Samples/Assets/sponza/Sponza01.gltf")));
    std::shared_ptr<FScene3DNode> scene = FScene3DLoader::LoadFromFile(scenePath);
    
    auto materials = scene->GetAllMaterial();
    HLVM_ENSURE(!materials.empty());
    
    // Validate first material
    auto firstMaterial = materials[0];
    HLVM_ENSURE(firstMaterial != nullptr);
    
    // Check material interface methods
    HLVM_ENSURE(!firstMaterial->GetName().empty());
    
    HLVM_LOG(LogTest, info, TXT("Material validation passed: {} materials loaded"),
             materials.size());
}

/**
 * @brief Test memory management and cleanup
 */
RECORD(memory_management)
{
	HLVM_LOG(LogTest, info, TXT("Testing memory management"));

	const FString GitRoot = FString::Format(TXT("{}/../../../../.."),*GExecutablePath);
	const FPath scenePath = FPath(FString::Format(TXT("{}{}"), *GitRoot, TXT("/Samples/Assets/sponza/Sponza01.gltf")));

	// Load and unload multiple times
	constexpr int NUM_ITERATIONS = 3;
	for (int i = 0; i < NUM_ITERATIONS; ++i) {
		std::shared_ptr<FScene3DNode> scene = FScene3DLoader::LoadFromFile(scenePath);
		HLVM_ENSURE(scene != nullptr);
	}

	HLVM_LOG(LogTest, info, TXT("Memory management test passed - {} load/unload cycles"),
		NUM_ITERATIONS);
}
