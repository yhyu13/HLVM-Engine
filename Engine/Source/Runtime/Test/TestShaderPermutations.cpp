/**
 * Copyright 2026 HLVM Engine
 *
 * MIT License
 *
 * TestShaderPermutations - Integration tests for FShaderLibrary permutation support
 *
 * Validates that multi-permutation .sblob files can be loaded with different
 * macro combinations, and that FDeferredLightingPass creates all 4 pipelines.
 */

#include "Test.h"
#include "Renderer/DeviceManager.h"
#include "Renderer/Window/WindowDefinition.h"
#include "Renderer/Shader/ShaderLibrary.h"
#include "Renderer/Deferred/FDeferredLightingPass.h"

DECLARE_LOG_CATEGORY(LogTest)

#if HLVM_VULKAN_RENDERER

static const char* WINDOW_TITLE = "TestShaderPermutations";
static const uint32_t WINDOW_WIDTH = 100;
static const uint32_t WINDOW_HEIGHT = 100;

RECORD(TestShaderPermutations_LoadAll, true)
{
    // =====================================================================
    // Create a minimal device
    // =====================================================================
    TUniquePtr<FDeviceManager> DeviceManager = FDeviceManager::Create(nvrhi::GraphicsAPI::VULKAN);
    HLVM_ENSURE(DeviceManager != nullptr);

    IWindow::Properties WindowProps;
    WindowProps.Title = WINDOW_TITLE;
    WindowProps.Extent = { WINDOW_WIDTH, WINDOW_HEIGHT };
    WindowProps.Resizable = false;
    WindowProps.VSync = IWindow::EVsync::Off;

    FDeviceCreationParameters& DeviceParams = const_cast<FDeviceCreationParameters&>(DeviceManager->GetDeviceParams());
    DeviceParams.bEnableNVRHIValidationLayer = false;
    DeviceParams.bEnableDebugRuntime = false;

    HLVM_ENSURE(DeviceManager->CreateWindowDeviceAndSwapChain(WindowProps));
    nvrhi::IDevice* Device = DeviceManager->GetDevice();
    HLVM_ENSURE(Device != nullptr);

    const FString ShaderDataDir = FString::Format(
        TXT("{}/Engine/Source/Runtime/Test/TestSponzaDeferred_Data"),
        *GProjectRoot);

    // =====================================================================
    // Test 1: Load all 4 permutations directly via FShaderLibrary
    // =====================================================================
    FShaderLibrary::Get().Clear();

    const TVector<FShaderMacro> Permutations[4] = {
        { FShaderMacro(TXT("ENABLE_SSAO"), TXT("0")), FShaderMacro(TXT("ENABLE_CONTACT_SHADOWS"), TXT("0")) },
        { FShaderMacro(TXT("ENABLE_SSAO"), TXT("1")), FShaderMacro(TXT("ENABLE_CONTACT_SHADOWS"), TXT("0")) },
        { FShaderMacro(TXT("ENABLE_SSAO"), TXT("0")), FShaderMacro(TXT("ENABLE_CONTACT_SHADOWS"), TXT("1")) },
        { FShaderMacro(TXT("ENABLE_SSAO"), TXT("1")), FShaderMacro(TXT("ENABLE_CONTACT_SHADOWS"), TXT("1")) },
    };

    for (int i = 0; i < 4; ++i)
    {
        nvrhi::ShaderHandle Shader = FShaderLibrary::Get().LoadShader(
            Device, ShaderDataDir, TXT("SponzaDeferredLighting_cs.sblob"),
            nvrhi::ShaderType::Compute, Permutations[i]);
        HLVM_ENSURE(Shader != nullptr);
        HLVM_LOG(LogTest, info, TXT("Permutation {} loaded successfully"), i);
    }

    // Blob should be cached once (shared across all 4 permutations)
    HLVM_ENSURE(FShaderLibrary::Get().GetCacheSize() == 1);

    // =====================================================================
    // Test 2: Macro order independence — different order, same result
    // =====================================================================
    {
        TVector<FShaderMacro> Order1 = {
            FShaderMacro(TXT("ENABLE_SSAO"), TXT("1")),
            FShaderMacro(TXT("ENABLE_CONTACT_SHADOWS"), TXT("1"))
        };
        TVector<FShaderMacro> Order2 = {
            FShaderMacro(TXT("ENABLE_CONTACT_SHADOWS"), TXT("1")),
            FShaderMacro(TXT("ENABLE_SSAO"), TXT("1"))
        };

        nvrhi::ShaderHandle Shader1 = FShaderLibrary::Get().LoadShader(
            Device, ShaderDataDir, TXT("SponzaDeferredLighting_cs.sblob"),
            nvrhi::ShaderType::Compute, Order1);
        nvrhi::ShaderHandle Shader2 = FShaderLibrary::Get().LoadShader(
            Device, ShaderDataDir, TXT("SponzaDeferredLighting_cs.sblob"),
            nvrhi::ShaderType::Compute, Order2);

        HLVM_ENSURE(Shader1 != nullptr);
        HLVM_ENSURE(Shader2 != nullptr);
        // Cache should still be size 1 (same blob)
        HLVM_ENSURE(FShaderLibrary::Get().GetCacheSize() == 1);
    }

    // =====================================================================
    // Test 3: FDeferredLightingPass initializes all 4 pipelines
    // =====================================================================
    {
        FDeferredLightingPass Pass;
        bool bInit = Pass.Initialize(Device, ShaderDataDir);
        HLVM_ENSURE(bInit);
        Pass.Shutdown();
        HLVM_LOG(LogTest, info, TXT("FDeferredLightingPass initialized all permutations"));
    }

    // =====================================================================
    // Test 4: Invalidate clears blob, reload works
    // =====================================================================
    {
        FShaderLibrary::Get().Invalidate(
            FPath::Combine(ShaderDataDir, TXT("SponzaDeferredLighting_cs.sblob")));
        // After invalidate, cache size should still be 1 (entry exists but blob data cleared)
        HLVM_ENSURE(FShaderLibrary::Get().GetCacheSize() == 1);

        nvrhi::ShaderHandle Shader = FShaderLibrary::Get().LoadShader(
            Device, ShaderDataDir, TXT("SponzaDeferredLighting_cs.sblob"),
            nvrhi::ShaderType::Compute, Permutations[3]);
        HLVM_ENSURE(Shader != nullptr);
    }

    FShaderLibrary::Get().Clear();

    // =====================================================================
    // Cleanup
    // =====================================================================
    DeviceManager->Shutdown();

    HLVM_LOG(LogTest, info, TXT("TestShaderPermutations_LoadAll passed"));
}

#endif // HLVM_VULKAN_RENDERER
