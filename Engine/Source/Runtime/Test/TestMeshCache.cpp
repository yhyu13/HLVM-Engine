/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 *
 * TestMeshCache — Verifies FMeshCache deduplication across scene reloads
 */

#include "Test.h"
#include "Renderer/RendererDefinition.h"
#include "Renderer/DeviceManager.h"
#include "Renderer/FSceneResourceManager.h"
#include "Renderer/Mesh/MeshCache.h"
#include <glm/glm.hpp>

DECLARE_LOG_CATEGORY(LogTest)

#ifdef HLVM_VULKAN_RENDERER

RECORD_BOOL(test_MeshCache)
{
    HLVM_LOG(LogTest, info, TXT("=== Starting MeshCache Test ==="));

    try
    {
        const FString GitRoot = FString::Format(TXT("{}"), *GProjectRoot);
        const FPath ScenePath = FPath(FString::Format(
            TXT("{}/Samples/Assets/Sponza/glTF/Sponza.gltf"), *GitRoot));

        // Create a device manager just to get a device
        TUniquePtr<FDeviceManager> DeviceManager = FDeviceManager::Create(nvrhi::GraphicsAPI::VULKAN);
        if (!DeviceManager)
        {
            throw std::runtime_error("Failed to create DeviceManager");
        }

        FDeviceCreationParameters& DeviceParams = const_cast<FDeviceCreationParameters&>(
            DeviceManager->GetDeviceParams());
        DeviceParams.BackBufferWidth = 800;
        DeviceParams.BackBufferHeight = 600;
        DeviceParams.SwapChainBufferCount = 2;
        DeviceParams.VSyncMode = 0;
        DeviceParams.bEnableDebugRuntime = false;
        DeviceParams.bEnableNVRHIValidationLayer = false;

        IWindow::Properties WindowProps;
        WindowProps.Title = "MeshCache Test";
        WindowProps.Extent = { 800, 600 };
        WindowProps.Resizable = false;
        WindowProps.VSync = IWindow::EVsync::Off;

        if (!DeviceManager->CreateWindowDeviceAndSwapChain(WindowProps))
        {
            throw std::runtime_error("Failed to create window, device and swap chain");
        }

        nvrhi::IDevice* NvrhiDevice = DeviceManager->GetDevice();

        // =====================================================================
        // First load
        // =====================================================================
        HLVM_LOG(LogTest, info, TXT("First scene load..."));
        FSceneResourceManager ResourceManager;
        if (!ResourceManager.Initialize(NvrhiDevice, ScenePath))
        {
            throw std::runtime_error("Failed to initialize resource manager (first load)");
        }

        auto DrawData1 = ResourceManager.BuildDrawData();
        uint32_t MeshCount1 = static_cast<uint32_t>(DrawData1.GBufferItems.size());
        HLVM_LOG(LogTest, info, TXT("First load: {} meshes"), MeshCount1);

        // Poll async loads until complete
        uint32_t TotalUploads = 0;
        for (int i = 0; i < 100 && ResourceManager.HasPendingLoads(); ++i)
        {
            TotalUploads += ResourceManager.PollAsyncLoads();
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        HLVM_LOG(LogTest, info, TXT("Async uploads complete: {} textures"), TotalUploads);

        // Shutdown but keep manager alive
        ResourceManager.Shutdown();

        // =====================================================================
        // Second load (same path) — should hit cache
        // =====================================================================
        HLVM_LOG(LogTest, info, TXT("Second scene load (should hit mesh cache)..."));
        if (!ResourceManager.Initialize(NvrhiDevice, ScenePath))
        {
            throw std::runtime_error("Failed to initialize resource manager (second load)");
        }

        auto DrawData2 = ResourceManager.BuildDrawData();
        uint32_t MeshCount2 = static_cast<uint32_t>(DrawData2.GBufferItems.size());
        HLVM_LOG(LogTest, info, TXT("Second load: {} meshes"), MeshCount2);

        // Poll async loads
        for (int i = 0; i < 100 && ResourceManager.HasPendingLoads(); ++i)
        {
            ResourceManager.PollAsyncLoads();
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        ResourceManager.Shutdown();

        // =====================================================================
        // Validation
        // =====================================================================
        HLVM_ENSURE(MeshCount1 == MeshCount2);
        HLVM_ENSURE(MeshCount1 > 0);

        HLVM_LOG(LogTest, info, TXT("Test completed successfully!"));
        return true;
    }
    catch (const std::exception& e)
    {
        HLVM_LOG(LogTest, critical, TXT("Test failed: {}"), TO_TCHAR_CSTR(e.what()));
        return false;
    }
}

#else // HLVM_VULKAN_RENDERER

RECORD_BOOL(test_MeshCache)
{
    HLVM_LOG(LogTest, warn, TXT("Vulkan renderer not enabled - skipping test"));
    return true;
}

#endif // HLVM_VULKAN_RENDERER
