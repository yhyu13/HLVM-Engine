/**
 * Copyright 2026 HLVM Engine
 *
 * MIT License
 *
 * TestTextureStreaming - Tests for non-blocking async texture loading
 *
 * Validates:
 * - BeginAsyncLoad enqueues decodes and returns immediately
 * - Poll processes completed decodes without blocking
 * - HasPendingLoads tracks remaining work
 * - Failed decodes are handled gracefully
 */

#include "Test.h"
#include "Renderer/Texture/AsyncTextureLoader.h"
#include "Renderer/Texture/TextureCache.h"
#include "Renderer/Material/PBRMaterial.h"

DECLARE_LOG_CATEGORY(LogTest)

RECORD(TestTextureStreaming_EmptyMaterials, true)
{
    // BeginAsyncLoad with empty materials should not crash
    FAsyncTextureLoader::BeginAsyncLoad(
        nullptr, {}, {});
    HLVM_ENSURE(!FAsyncTextureLoader::HasPendingLoads());
    HLVM_LOG(LogTest, info, TXT("TestTextureStreaming_EmptyMaterials passed"));
}

RECORD(TestTextureStreaming_InvalidPath, true)
{
    // Create a material with a non-existent texture
    auto Material = std::make_shared<FPBRMaterial>();
    Material->SetTexture(TXT("fake"), FPath(TXT("/nonexistent/texture.png")), IMaterial::ETextureType::Albedo);

    TVector<std::shared_ptr<FPBRMaterial>> Materials = { Material };
    TVector<IMaterial::ETextureType> TypesToLoad = { IMaterial::ETextureType::Albedo };

    // Begin async load
    FAsyncTextureLoader::BeginAsyncLoad(nullptr, Materials, TypesToLoad);
    HLVM_ENSURE(FAsyncTextureLoader::HasPendingLoads());

    // Poll should eventually process the failed decode and return 0
    // Worker thread may need time to complete, so poll in a loop
    uint32_t Uploaded = 0;
    uint32_t Attempts = 0;
    while (FAsyncTextureLoader::HasPendingLoads() && Attempts < 100)
    {
        Uploaded = FAsyncTextureLoader::Poll(nullptr);
        if (Uploaded == 0 && FAsyncTextureLoader::HasPendingLoads())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        ++Attempts;
    }
    HLVM_ENSURE(Uploaded == 0);
    HLVM_ENSURE(!FAsyncTextureLoader::HasPendingLoads());

    // Material should still not have a GPU texture
    HLVM_ENSURE(!Material->HasGPUTexture(IMaterial::ETextureType::Albedo));

    HLVM_LOG(LogTest, info, TXT("TestTextureStreaming_InvalidPath passed"));
}

RECORD(TestTextureStreaming_PollNoPending, true)
{
    // Poll with no pending loads should return 0 and not crash
    HLVM_ENSURE(!FAsyncTextureLoader::HasPendingLoads());
    uint32_t Uploaded = FAsyncTextureLoader::Poll(nullptr);
    HLVM_ENSURE(Uploaded == 0);
    HLVM_LOG(LogTest, info, TXT("TestTextureStreaming_PollNoPending passed"));
}
