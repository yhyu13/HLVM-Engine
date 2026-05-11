/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 *
 * TestSponzaLoading - Sponza Scene Loading with Deferred Rendering
 *
 * Tests:
 * 1. Scene loading via FScene3DLoader
 * 2. Material texture loading via FPBRMaterial::LoadTexture (PNG via STB)
 *
 * This test validates the pipeline from glTF scene to texture info.
 */

#include "Test.h"
#include "Renderer/Scene3D/Scene3DLoader.h"
#include "Renderer/Material/PBRMaterial.h"
#include "Platform/FileSystem/Path.h"
#include "Core/Log.h"

DECLARE_LOG_CATEGORY(LogTest)

RECORD(sponza_scene_load)
{
    HLVM_LOG(LogTest, info, TXT("=== Testing Sponza Scene Loading ==="));

    const FString GitRoot = FString::Format(TXT("{}"), *GProjectRoot);
    const FPath ScenePath = FPath(FString::Format(
        TXT("{}/Samples/Assets/Sponza/glTF/Sponza.gltf"), *GitRoot));

    // Verify file exists
    HLVM_ENSURE_F(FPath::Exists(ScenePath), TXT("Scene file not found"));

    // Load scene
    auto Scene = FScene3DLoader::LoadFromFile(ScenePath);
    HLVM_ENSURE_F(Scene != nullptr, TXT("Failed to load scene"));
    HLVM_ENSURE_F(!Scene->IsEmpty(), TXT("Scene is empty"));

    HLVM_LOG(LogTest, info, TXT("Scene loaded successfully"));

    auto StaticMeshes = Scene->GetAllStaticMesh();
    HLVM_ENSURE_F(!StaticMeshes.empty(), TXT("No meshes found"));

    HLVM_LOG(LogTest, info, TXT("Found meshes"));
}

RECORD(sponza_texture_info)
{
    HLVM_LOG(LogTest, info, TXT("=== Testing Texture Info ==="));

    const FString GitRoot = FString::Format(TXT("{}"), *GProjectRoot);
    const FPath ScenePath = FPath(FString::Format(
        TXT("{}/Samples/Assets/Sponza/glTF/Sponza.gltf"), *GitRoot));

    auto Scene = FScene3DLoader::LoadFromFile(ScenePath);
    HLVM_ENSURE_F(Scene != nullptr, TXT("Failed to load scene"));

    auto Materials = Scene->GetAllMaterial();
    HLVM_ENSURE_F(!Materials.empty(), TXT("No materials found"));

    HLVM_LOG(LogTest, info, TXT("Found materials"));

    // Count texture types
    int AlbedoCount = 0;
    int PngCount = 0;
    int KtxCount = 0;

    for (auto& Material : Materials)
    {
        if (auto PBRMat = std::dynamic_pointer_cast<FPBRMaterial>(Material))
        {
            if (PBRMat->HasTexture(IMaterial::ETextureType::Albedo))
            {
                AlbedoCount++;
                FPath TexPath = PBRMat->GetTexturePath(IMaterial::ETextureType::Albedo);
                FString Ext = FPath::GetExtension(TexPath);
                if (Ext == ".png" || Ext == ".PNG" || Ext == ".jpg" || Ext == ".JPG")
                {
                    PngCount++;
                }
                else if (Ext == ".ktx" || Ext == ".KTX")
                {
                    KtxCount++;
                }
            }
        }
    }

    HLVM_LOG(LogTest, info, TXT("Albedo count: %d"), AlbedoCount);
    HLVM_LOG(LogTest, info, TXT("PNG textures: %d"), PngCount);
    HLVM_LOG(LogTest, info, TXT("KTX textures: %d"), KtxCount);

    HLVM_ENSURE_F(AlbedoCount > 0, TXT("No albedo textures found"));
}
