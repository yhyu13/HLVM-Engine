/**
 * Copyright 2026 HLVM Engine
 *
 * MIT License
 *
 * TestShaderLibrary - Unit tests for FShaderLibrary and FShaderHotReloader
 */

#include "Test.h"
#include "Renderer/Shader/ShaderLibrary.h"
#include "Renderer/Shader/ShaderHotReloader.h"
#include "Platform/FileSystem/Path.h"

#include <boost/filesystem.hpp>

DECLARE_LOG_CATEGORY(LogTest)

RECORD(TestShaderLibrary_EmptyCache, true)
{
    FShaderLibrary::Get().Clear();
    HLVM_ENSURE(FShaderLibrary::Get().GetCacheSize() == 0);
    HLVM_LOG(LogTest, info, TXT("TestShaderLibrary_EmptyCache passed"));
}

RECORD(TestShaderLibrary_NullDevice, true)
{
    FShaderLibrary::Get().Clear();

    nvrhi::ShaderHandle Shader = FShaderLibrary::Get().LoadShader(
        nullptr,
        FPath(TXT("/fake/path.sblob")),
        nvrhi::ShaderType::Compute);

    HLVM_ENSURE(Shader == nullptr);
    HLVM_ENSURE(FShaderLibrary::Get().GetCacheSize() == 0);
    HLVM_LOG(LogTest, info, TXT("TestShaderLibrary_NullDevice passed"));
}

RECORD(TestShaderLibrary_InvalidPath, true)
{
    FShaderLibrary::Get().Clear();

    // Create a fake device pointer (not used for blob read, but required by API)
    // Since we pass a non-existent path, LoadShader should fail before device is used
    nvrhi::ShaderHandle Shader = FShaderLibrary::Get().LoadShader(
        reinterpret_cast<nvrhi::IDevice*>(0x1),
        FPath(TXT("/nonexistent/shader.sblob")),
        nvrhi::ShaderType::Compute);

    HLVM_ENSURE(Shader == nullptr);
    HLVM_LOG(LogTest, info, TXT("TestShaderLibrary_InvalidPath passed"));
}

RECORD(TestShaderLibrary_PollNoChange, true)
{
    FShaderLibrary::Get().Clear();

    // Poll on non-cached file should return false
    bool Changed = FShaderLibrary::Get().PollFile(FPath(TXT("/fake/path.sblob")));
    HLVM_ENSURE(!Changed);

    HLVM_LOG(LogTest, info, TXT("TestShaderLibrary_PollNoChange passed"));
}

RECORD(TestShaderLibrary_InvalidateNoCrash, true)
{
    FShaderLibrary::Get().Clear();

    // Invalidate non-cached path should not crash
    FShaderLibrary::Get().Invalidate(FPath(TXT("/fake/path.sblob")));

    HLVM_LOG(LogTest, info, TXT("TestShaderLibrary_InvalidateNoCrash passed"));
}

RECORD(TestShaderLibrary_HotReloaderRegister, true)
{
    // Test registration/unregistration doesn't crash
    struct FFakeReloadable : public IShaderReloadable
    {
        bool bReloaded = false;
        virtual void ReloadShaders() override { bReloaded = true; }
    };

    FFakeReloadable Fake;
    FShaderHotReloader::Get().Register(&Fake);
    FShaderHotReloader::Get().Unregister(&Fake);

    HLVM_LOG(LogTest, info, TXT("TestShaderLibrary_HotReloaderRegister passed"));
}

#ifdef HLVM_VULKAN_RENDERER

RECORD(TestShaderLibrary_RealBlobLoad, true)
{
    FShaderLibrary::Get().Clear();

    // Use a real blob file that always exists
    FPath BlobPath = FPath::Combine(
        FString::Format(TXT("{}/Engine/Source/Runtime/Shader"), *GProjectRoot),
        TXT("BlitVS.sblob"));

    // Even without a device, we can verify the file exists
    HLVM_ENSURE(FPath::Exists(BlobPath));

    // With nullptr device, LoadShader returns nullptr but should NOT cache the blob
    // because it bails early
    nvrhi::ShaderHandle Shader = FShaderLibrary::Get().LoadShader(
        nullptr, BlobPath, nvrhi::ShaderType::Vertex);
    HLVM_ENSURE(Shader == nullptr);

    HLVM_LOG(LogTest, info, TXT("TestShaderLibrary_RealBlobLoad passed"));
}

#endif // HLVM_VULKAN_RENDERER
