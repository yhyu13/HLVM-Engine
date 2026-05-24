// Copyright 2026 HLVM Engine
//
// MIT License

#pragma once

#include "Core/String.h"
#include "Renderer/Scene3D/FSceneGPUData.h"
#include "Renderer/Mesh/MeshCache.h"
#include "Renderer/Texture/TextureCache.h"
#include "Renderer/Utility/FMemoryBudget.h"
#include <nvrhi/nvrhi.h>

/**
 * @brief Minimal resource manager that unifies scene loading and asset cleanup
 *
 * FResourceManager owns the lifetime of scene GPU data and guarantees
 * that all cached GPU resources are released before the device is destroyed.
 *
 * Current scope:
 * - Owns FSceneGPUData (meshes, materials, textures)
 * - Calls FTextureCache::Clear() on Shutdown() to prevent validation errors
 * - Polls async texture loads
 *
 * Future scope:
 * - Own FTextureCache directly (convert from singleton)
 * - Own FMeshCache for vertex/index buffer deduplication
 * - Memory budget enforcement
 */
class FSceneResourceManager
{
public:
    bool Initialize(nvrhi::IDevice* InDevice, const FPath& ScenePath);
    void Shutdown();

    // Poll async texture uploads (call once per frame)
    // Returns number of textures uploaded this call
    uint32_t PollAsyncLoads();

    // Check if any texture loads are still pending
    bool HasPendingLoads() const;

    // Build per-frame draw items (resolves current material bindings)
    FSceneGPUData::FDrawData BuildDrawData();

    // Scene bounds accessors (no need to BuildDrawData just for bounds)
    glm::vec3 GetSceneCenter() const;
    glm::vec3 GetSceneBBoxMin() const;
    glm::vec3 GetSceneBBoxMax() const;
    float GetSceneRadius() const;

    // Accessors
    bool IsInitialized() const
    {
        return bIsInitialized;
    }

    nvrhi::IDevice* GetDevice() const
    {
        return Device;
    }

    FSceneGPUData* GetSceneGPUData()
    {
        return SceneGPUData.get();
    }

    FSceneResourceManager() = default;
    ~FSceneResourceManager()
    {
        Shutdown();
        if (MeshCache)
        {
            MeshCache->Clear();
            MeshCache.reset();
        }
    }
    FSceneResourceManager(const FSceneResourceManager&) = delete;
    FSceneResourceManager& operator=(const FSceneResourceManager&) = delete;

private:
    nvrhi::IDevice* Device = nullptr;
    TUniquePtr<FSceneGPUData> SceneGPUData;
    TUniquePtr<FMeshCache> MeshCache;
    FTextureCache TextureCache;
    FMemoryBudget MemoryBudget;
    bool bIsInitialized = false;
};
