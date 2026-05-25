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
 * - Placeholder → real texture transition for valid files
 */

#include "Test.h"
#include "Renderer/Texture/AsyncTextureLoader.h"
#include "Renderer/Texture/TextureCache.h"
#include "Renderer/Material/PBRMaterial.h"

#if HLVM_VULKAN_RENDERER
#include "Renderer/DeviceManager.h"
#include "Renderer/Window/WindowDefinition.h"
#endif

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

#if HLVM_VULKAN_RENDERER

RECORD(TestTextureStreaming_RealTextureTransition, true)
{
    // =====================================================================
    // Create a minimal device
    // =====================================================================
    TUniquePtr<FDeviceManager> DeviceManager = FDeviceManager::Create(nvrhi::GraphicsAPI::VULKAN);
    HLVM_ENSURE(DeviceManager != nullptr);

    IWindow::Properties WindowProps;
    WindowProps.Title = TXT("TestTextureStreaming");
    WindowProps.Extent = { 100, 100 };
    WindowProps.Resizable = false;
    WindowProps.VSync = IWindow::EVsync::Off;

    FDeviceCreationParameters& DeviceParams = const_cast<FDeviceCreationParameters&>(DeviceManager->GetDeviceParams());
    DeviceParams.bEnableNVRHIValidationLayer = false;
    DeviceParams.bEnableDebugRuntime = false;

    HLVM_ENSURE(DeviceManager->CreateWindowDeviceAndSwapChain(WindowProps));
    nvrhi::IDevice* Device = DeviceManager->GetDevice();
    HLVM_ENSURE(Device != nullptr);

    // =====================================================================
    // Set up a texture cache so uploads are deduplicated
    // =====================================================================
    FTextureCache TextureCache;
    FAsyncTextureLoader::SetTextureCache(&TextureCache);

    // =====================================================================
    // Create a material with a known-valid texture
    // =====================================================================
    const FPath TexturePath = FPath::Combine(GProjectRoot, TXT("Samples/Assets/Sponza/glTF/white.png"));
    HLVM_ENSURE(FPath::Exists(TexturePath));

    auto Material = std::make_shared<FPBRMaterial>();
    Material->SetTexture(TXT("white"), TexturePath, IMaterial::ETextureType::Albedo);

    TVector<std::shared_ptr<FPBRMaterial>> Materials = { Material };
    TVector<IMaterial::ETextureType> TypesToLoad = { IMaterial::ETextureType::Albedo };

    // Before load: material should NOT have a GPU texture
    HLVM_ENSURE(!Material->HasGPUTexture(IMaterial::ETextureType::Albedo));

    // =====================================================================
    // Begin async load (non-blocking)
    // =====================================================================
    FAsyncTextureLoader::BeginAsyncLoad(Device, Materials, TypesToLoad);
    HLVM_ENSURE(FAsyncTextureLoader::HasPendingLoads());

    // =====================================================================
    // Poll until completion
    // =====================================================================
    uint32_t TotalUploaded = 0;
    uint32_t Attempts = 0;
    while (FAsyncTextureLoader::HasPendingLoads() && Attempts < 1000)
    {
        TotalUploaded += FAsyncTextureLoader::Poll(Device);
        ++Attempts;
        if (FAsyncTextureLoader::HasPendingLoads())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    // Should have uploaded exactly 1 texture
    HLVM_ENSURE(TotalUploaded == 1);
    HLVM_ENSURE(!FAsyncTextureLoader::HasPendingLoads());

    // After load: material MUST have a valid GPU texture
    HLVM_ENSURE(Material->HasGPUTexture(IMaterial::ETextureType::Albedo));
    HLVM_ENSURE(Material->GetGPUTexture(IMaterial::ETextureType::Albedo).GetTextureHandle() != nullptr);

    // Texture cache should contain the path
    HLVM_ENSURE(TextureCache.GetNumEntries() == 1);

    HLVM_LOG(LogTest, info, TXT("TestTextureStreaming_RealTextureTransition passed (uploaded {} texture in {} polls)"), TotalUploaded, Attempts);

    // =====================================================================
    // Cleanup — release GPU resources BEFORE destroying the device
    // =====================================================================
    Material.reset();
    Materials.clear();
    FAsyncTextureLoader::SetTextureCache(nullptr);
    TextureCache.Clear();
    DeviceManager->Shutdown();
}

#endif // HLVM_VULKAN_RENDERER
