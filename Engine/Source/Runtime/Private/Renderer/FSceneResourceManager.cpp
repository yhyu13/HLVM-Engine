// Copyright 2026 HLVM Engine
//
// MIT License

#include "Renderer/FSceneResourceManager.h"
#include "Renderer/Texture/AsyncTextureLoader.h"
#include "Core/Log.h"

DECLARE_LOG_CATEGORY(LogResourceManager)

bool FSceneResourceManager::Initialize(nvrhi::IDevice* InDevice, const FPath& ScenePath)
{
    if (bIsInitialized)
    {
        Shutdown();
    }

    Device = InDevice;

    if (!MeshCache)
    {
        MeshCache = std::make_unique<FMeshCache>();
        MeshCache->SetMemoryBudget(&MemoryBudget);
    }

    TextureCache.SetMemoryBudget(&MemoryBudget);
    FAsyncTextureLoader::SetTextureCache(&TextureCache);

    SceneGPUData = std::make_unique<FSceneGPUData>();
    if (!SceneGPUData->Initialize(Device, ScenePath, MeshCache.get()))
    {
        HLVM_LOG(LogResourceManager, err, TXT("FSceneResourceManager: Failed to initialize scene GPU data"));
        SceneGPUData.reset();
        Device = nullptr;
        return false;
    }

    bIsInitialized = true;
    HLVM_LOG(LogResourceManager, info,
        TXT("FSceneResourceManager: Initialized successfully (mesh cache: {} entries, texture cache: {} entries, memory used: {}/{} bytes, {:.1f}% util)"),
        MeshCache->GetNumEntries(),
        TextureCache.GetNumEntries(),
        MemoryBudget.GetUsedBytes(),
        MemoryBudget.GetBudgetBytes(),
        MemoryBudget.GetUtilization() * 100.0f);
    return true;
}

void FSceneResourceManager::Shutdown()
{
    if (!bIsInitialized)
    {
        return;
    }

    HLVM_LOG(LogResourceManager, info, TXT("FSceneResourceManager::Shutdown"));

    // Release scene GPU data first (meshes, materials, placeholder textures)
    if (SceneGPUData)
    {
        SceneGPUData->Shutdown();
        SceneGPUData.reset();
    }

    // MeshCache persists across scene reloads — log current stats
    if (MeshCache)
    {
        HLVM_LOG(LogResourceManager, info,
            TXT("FMeshCache stats: {} entries, {} vertex bytes, {} index bytes"),
            MeshCache->GetNumEntries(),
            MeshCache->GetTotalVertexBytes(),
            MeshCache->GetTotalIndexBytes());
    }

    HLVM_LOG(LogResourceManager, info,
        TXT("FTextureCache stats: {} entries, {} bytes, total memory util: {:.1f}%"),
        TextureCache.GetNumEntries(),
        TextureCache.GetTotalMemoryBytes(),
        MemoryBudget.GetUtilization() * 100.0f);

    // TextureCache persists across scene reloads — only release active pointer
    FAsyncTextureLoader::SetTextureCache(nullptr);

    Device = nullptr;
    bIsInitialized = false;
}

uint32_t FSceneResourceManager::PollAsyncLoads()
{
    if (Device)
    {
        return FAsyncTextureLoader::Poll(Device);
    }
    return 0;
}

bool FSceneResourceManager::HasPendingLoads() const
{
    return FAsyncTextureLoader::HasPendingLoads();
}

FSceneGPUData::FDrawData FSceneResourceManager::BuildDrawData()
{
    if (SceneGPUData)
    {
        return SceneGPUData->BuildDrawData();
    }

    return FSceneGPUData::FDrawData();
}

glm::vec3 FSceneResourceManager::GetSceneCenter() const
{
    if (SceneGPUData)
    {
        auto DrawData = SceneGPUData->BuildDrawData();
        return DrawData.SceneCenter;
    }
    return glm::vec3(0.f);
}

glm::vec3 FSceneResourceManager::GetSceneBBoxMin() const
{
    if (SceneGPUData)
    {
        auto DrawData = SceneGPUData->BuildDrawData();
        return DrawData.BBoxMin;
    }
    return glm::vec3(0.f);
}

glm::vec3 FSceneResourceManager::GetSceneBBoxMax() const
{
    if (SceneGPUData)
    {
        auto DrawData = SceneGPUData->BuildDrawData();
        return DrawData.BBoxMax;
    }
    return glm::vec3(0.f);
}

float FSceneResourceManager::GetSceneRadius() const
{
    if (SceneGPUData)
    {
        auto DrawData = SceneGPUData->BuildDrawData();
        return DrawData.SceneRadius;
    }
    return 0.f;
}
