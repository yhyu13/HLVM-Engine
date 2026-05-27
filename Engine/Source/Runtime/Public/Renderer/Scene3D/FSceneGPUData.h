#pragma once

#include "Core/String.h"
#include "Renderer/Deferred/FDeferredFrameRenderer.h"
#include "Renderer/Shadow/FShadowMapPass.h"
#include <nvrhi/nvrhi.h>
#include <glm/glm.hpp>

class FScene3DNode;
class FStaticMesh;
class FMeshCache;

class FSceneGPUData
{
public:
    struct FDrawData
    {
        TVector<FShadowMapPass::FMeshDrawItem> ShadowItems;
        TVector<FDeferredFrameRenderer::FGBufferMeshItem> GBufferItems;
        glm::vec3 SceneCenter;
        glm::vec3 BBoxMin;
        glm::vec3 BBoxMax;
        float SceneRadius = 0.0f;
    };

    bool Initialize(nvrhi::IDevice* InDevice, const FPath& ScenePath, FMeshCache* InMeshCache = nullptr);
    void Shutdown();

    // Build per-frame draw items (resolves current material bindings)
    FDrawData BuildDrawData();

    // Accessors
    nvrhi::TextureHandle GetPlaceholderTexture() const
    {
        return PlaceholderTexture;
    }
    nvrhi::TextureHandle GetNormalPlaceholderTexture() const
    {
        return NormalPlaceholderTexture;
    }

    // O(1) cached scene bounds accessors — do NOT trigger BuildDrawData()
    glm::vec3 GetSceneCenter() const { return CachedSceneCenter; }
    glm::vec3 GetSceneBBoxMin() const { return CachedBBoxMin; }
    glm::vec3 GetSceneBBoxMax() const { return CachedBBoxMax; }
    float GetSceneRadius() const { return CachedSceneRadius; }

    FSceneGPUData() = default;
    ~FSceneGPUData()
    {
        Shutdown();
    }
    FSceneGPUData(const FSceneGPUData&) = delete;
    FSceneGPUData& operator=(const FSceneGPUData&) = delete;

private:
    struct FMeshGPUData
    {
        nvrhi::BufferHandle VertexBuffer;
        nvrhi::BufferHandle IndexBuffer;
        uint32_t IndexCount;
        std::shared_ptr<FStaticMesh> Mesh;
        glm::mat4 WorldTransform{1.0f};  // World transform from scene graph
    };

    nvrhi::IDevice* Device = nullptr;
    std::shared_ptr<FScene3DNode> Scene;
    TVector<FMeshGPUData> MeshGPUData;
    FMeshCache* MeshCache = nullptr;
    FPath CachedScenePath;

    nvrhi::TextureHandle PlaceholderTexture;
    nvrhi::TextureHandle NormalPlaceholderTexture;

    glm::vec3 CachedSceneCenter = glm::vec3(0.f);
    glm::vec3 CachedBBoxMin = glm::vec3(0.f);
    glm::vec3 CachedBBoxMax = glm::vec3(0.f);
    float CachedSceneRadius = 0.0f;
    bool bIsInitialized = false;
};
