// FGIPass.cpp - Few-bounce / path-tracing GI ray-tracing pass implementation.
//
// Task 1.4 of the ReSTIR/GI separation sprint-1 plan: wire FRayTracingPipeline +
// FBindingLayoutBuilder + GI constant buffer + per-frame DispatchRays binding set.

#include "Renderer/GI/FGIPass.h"

#include "Core/Assert.h"
#include "Core/Log.h"
#include "Core/String.h"
#include "Platform/FileSystem/Path.h"
#include "Renderer/Common/FBindingLayoutBuilder.h"
#include "Renderer/ShaderMake/ShaderBlob.h"
#include "Utility/CVar/CVarMacros.h"

#include <fstream>
#include <vector>

#include <nvrhi/nvrhi.h>

DECLARE_LOG_CATEGORY(LogGI)

namespace GI
{
    // CVars - read at DispatchRays time so runtime tuning works without re-init.
    AUTO_CVAR_INT  (r_GI_MaxBounces,      4,    "Maximum number of GI bounces (0 = direct only)", EConsoleVariableFlag::Saved)
    AUTO_CVAR_INT  (r_GI_SamplesPerPixel, 8,    "Samples per pixel for indirect lighting",      EConsoleVariableFlag::Saved)
    AUTO_CVAR_FLOAT(r_GI_MinRayLength,    0.001f, "GI bounce ray TMin (avoid self-intersection)", EConsoleVariableFlag::Saved)
    AUTO_CVAR_BOOL (r_GI_EnableRR,        true, "Enable Russian Roulette path termination",     EConsoleVariableFlag::Saved)
    AUTO_CVAR_FLOAT(r_GI_RussianRoulette, 0.95f, "Russian Roulette survival threshold",          EConsoleVariableFlag::Saved)
    AUTO_CVAR_BOOL (r_GI_DebugBounceStats, false, "Write per-frame bounce stats to u1 UAV (gates DebugStatsTexture binding)", EConsoleVariableFlag::Console)

    // GI constants cbuffer layout (must match GIPathTracing.hlsl GIConstants).
    // 5 * float4 = 80 bytes.
    struct FGIConstantsData
    {
        float LightDir[4];
        float AmbientColor[4];
        float CameraPos[4];
        float Params[4];   // x=MaxBounces, y=SPP, z=ShadowTMin, w=ShadowTMax
        float Params2[4];  // x=AmbientScale, y=RayTMin, z=RayTMax, w=ShadowEnable
    };
    static_assert(sizeof(FGIConstantsData) == 80, "FGIConstantsData must be 80 bytes (5x float4)");

    namespace
    {
        std::vector<char> ReadBinaryFile(const std::string& Filename)
        {
            std::ifstream file(Filename, std::ios::ate | std::ios::binary);
            if (!file.is_open())
            {
                return {};
            }
            size_t fileSize = static_cast<size_t>(file.tellg());
            std::vector<char> buffer(fileSize);
            file.seekg(0);
            file.read(buffer.data(), static_cast<std::streamsize>(fileSize));
            file.close();
            return buffer;
        }
    } // namespace

    bool FGIPass::Initialize(nvrhi::IDevice* InDevice,
                             const FString& InShaderDataDir,
                             const FScene* InScene)
    {
        if (bIsInitialized)
            return true;

        if (!InDevice)
            return false;

        Device        = InDevice;
        ShaderDataDir = InShaderDataDir;
        Scene         = InScene;

        if (!LoadShaders())
            return false;
        if (!CreatePipeline())
            return false;
        if (!CreateBindingLayout())
            return false;
        if (!CreateConstantBuffer())
            return false;

        bIsInitialized = true;
        HLVM_LOG(LogGI, info, TXT("FGIPass initialized (shader dir: %s)"), *InShaderDataDir);
        return true;
    }

    void FGIPass::Shutdown()
    {
        // FRayTracingPipeline's copy/move ops are deleted, so we can't reassign.
        // Shutdown() is a public method that clears internal state.
        ShaderLibrary  = nullptr;
        RTPipeline.Shutdown();
        BindingLayout  = nullptr;
        ConstantBuffer = nullptr;
        OutputTexture  = nullptr;
        Device         = nullptr;
        Scene          = nullptr;
        bIsInitialized  = false;
    }

    bool FGIPass::LoadShaders()
    {
        const std::string SblobPath = FPath::Combine(ShaderDataDir, TXT("GIPathTracing.sblob")).string();
        auto Blob = ReadBinaryFile(SblobPath);
        if (Blob.empty())
        {
            HLVM_LOG(LogGI, err, TXT("Failed to read GIPathTracing.sblob at %s"), *FString(SblobPath.c_str()));
            return false;
        }

        const void* ShaderBinary    = nullptr;
        size_t      ShaderBinarySize = 0;
        if (!ShaderMake::FindPermutationInBlob(Blob.data(), Blob.size(), nullptr, 0,
                                                &ShaderBinary, &ShaderBinarySize))
        {
            HLVM_LOG(LogGI, err, TXT("Failed to extract shader library from GIPathTracing.sblob"));
            return false;
        }

        ShaderLibrary = Device->createShaderLibrary(ShaderBinary, ShaderBinarySize);
        if (!ShaderLibrary)
        {
            HLVM_LOG(LogGI, err, TXT("Failed to create ShaderLibrary from GIPathTracing.sblob"));
            return false;
        }

        return true;
    }

    bool FGIPass::CreatePipeline()
    {
        if (!ShaderLibrary)
            return false;

        if (!RTPipeline.InitializeFromLibrary(Device, ShaderLibrary,
                                                TXT("RayGen"), TXT("ClosestHit"), TXT("Miss"),
                                                TXT("ShadowMiss")))
        {
            HLVM_LOG(LogGI, err, TXT("FRayTracingPipeline init from library failed"));
            return false;
        }

        // MaxPayloadSize = 64 (GIPayload: throughput+radiance+origin+direction+hitDist+bounceCount+flags+seed)
        // MaxAttributeSize = 8 (barycentric float2 attributes - standard)
        if (!RTPipeline.FinalizePipeline(/*MaxPayloadSize*/ 64, /*MaxAttributeSize*/ 8))
        {
            HLVM_LOG(LogGI, err, TXT("FRayTracingPipeline finalize failed"));
            return false;
        }

        if (!RTPipeline.BuildShaderTable())
        {
            HLVM_LOG(LogGI, err, TXT("FRayTracingPipeline shader table build failed"));
            return false;
        }

        return true;
    }

    bool FGIPass::CreateBindingLayout()
    {
        // Build the RT pipeline's binding layout via its embedded builder.
        // Resource layout matches GIPathTracing.hlsl register declarations:
        //   b0 = GIConstants (own constant buffer)
        //   b1 = ViewConstants (test-provided)
        //   t0 = SceneBVH (RayTracingAccelStruct)
        //   t1 = GBufferWorldPos (Texture2D<float4>)
        //   t2 = GBufferNormal  (Texture2D<float4>)
        //   t3 = GBufferMaterial (Texture2D<float4>)
        //   t5 = RTVertices  (StructuredBuffer<FRTVertex>)
        //   t6 = RTIndices   (StructuredBuffer<uint>)
        //   t7 = RTInstanceInfo (StructuredBuffer<FInstanceInfo>)
        //   s2 = LinearSampler
        //   u0 = OutputTexture (RWTexture2D<float4>)
        //   u1 = DebugStatsTexture (RWTexture2D<float4>, optional - added at DispatchRays time)
        auto& Builder = RTPipeline.CreateBindingLayout();
        Builder.SetVisibility(nvrhi::ShaderType::All)
               .AddConstantBuffer(0)             // b0 - GIConstants
               .AddConstantBuffer(1)             // b1 - ViewConstants
               .AddRayTracingAccelStruct(0)      // t0 - SceneBVH
               .AddTextureSRV(1)                 // t1 - GBufferWorldPos
               .AddTextureSRV(2)                 // t2 - GBufferNormal
               .AddTextureSRV(3)                 // t3 - GBufferMaterial
               .AddStructuredBufferSRV(5)        // t5 - RTVertices
               .AddStructuredBufferSRV(6)        // t6 - RTIndices
               .AddStructuredBufferSRV(7)        // t7 - RTInstanceInfo
               .AddSampler(2)                    // s2 - LinearSampler
               .AddTextureUAV(0);                // u0 - OutputTexture

        BindingLayout = RTPipeline.GetBindingLayout();
        return static_cast<bool>(BindingLayout);
    }

    bool FGIPass::CreateConstantBuffer()
    {
        nvrhi::BufferDesc Desc;
        Desc.byteSize = sizeof(FGIConstantsData);
        Desc.isConstantBuffer = true;
        Desc.initialState = nvrhi::ResourceStates::ConstantBuffer;
        Desc.keepInitialState = true;
        Desc.debugName = "FGIPass.GIConstants";
        ConstantBuffer = Device->createBuffer(Desc);
        return static_cast<bool>(ConstantBuffer);
    }

    void FGIPass::WriteConstants(nvrhi::ICommandList* CmdList, const FGIPassDesc& /*Desc*/)
    {
        // Read CVars at dispatch time (per spec). AUTO_CVAR_* macros create
        // CVar_<name> global instances.
        const uint32_t maxBounces   = static_cast<uint32_t>(CVar_r_GI_MaxBounces.GetValue());
        const uint32_t spp          = static_cast<uint32_t>(CVar_r_GI_SamplesPerPixel.GetValue());
        const float    minRayLength = CVar_r_GI_MinRayLength.GetValue();

        // Defaults - mirror TestFewBounceGI's baked constants. Future: read from Desc.
        const float LightDir[4]    = { 0.577f, 0.577f, 0.577f, 1.0f };
        const float AmbientColor[4] = { 0.6f,  0.6f,  0.65f,  0.0f };
        const float CameraPos[4]   = { 0.0f,  0.0f,  0.0f,  1.0f };

        FGIConstantsData Data;
        std::memcpy(Data.LightDir,    LightDir,    sizeof(LightDir));
        std::memcpy(Data.AmbientColor, AmbientColor, sizeof(AmbientColor));
        std::memcpy(Data.CameraPos,   CameraPos,   sizeof(CameraPos));
        Data.Params[0]  = static_cast<float>(maxBounces);
        Data.Params[1]  = static_cast<float>(spp);
        Data.Params[2]  = 0.001f;  // ShadowTMin
        Data.Params[3]  = 1000.0f; // ShadowTMax
        Data.Params2[0] = 0.3f;    // AmbientScale
        Data.Params2[1] = minRayLength;
        Data.Params2[2] = 1000.0f; // RayTMax
        Data.Params2[3] = 1.0f;    // ShadowEnable

        CmdList->writeBuffer(ConstantBuffer, &Data, sizeof(Data));
    }

    void FGIPass::DispatchRays(nvrhi::ICommandList* CmdList, const FGIPassDesc& Desc)
    {
        if (!bIsInitialized || !RTPipeline.IsInitialized())
            return;

        if (!Desc.SceneTLAS || !Desc.OutputTexture || !Desc.ViewConstants)
        {
            HLVM_LOG(LogGI, err, TXT("FGIPass::DispatchRays: missing required handles"));
            return;
        }

        WriteConstants(CmdList, Desc);

        CmdList->setTextureState(Desc.OutputTexture, nvrhi::AllSubresources,
                                 nvrhi::ResourceStates::UnorderedAccess);
        if (Desc.GBufferWorldPos)
            CmdList->setTextureState(Desc.GBufferWorldPos, nvrhi::AllSubresources,
                                     nvrhi::ResourceStates::ShaderResource);
        if (Desc.GBufferNormal)
            CmdList->setTextureState(Desc.GBufferNormal, nvrhi::AllSubresources,
                                     nvrhi::ResourceStates::ShaderResource);
        if (Desc.GBufferMaterial)
            CmdList->setTextureState(Desc.GBufferMaterial, nvrhi::AllSubresources,
                                     nvrhi::ResourceStates::ShaderResource);

        FBindingSetBuilder SetBuilder;
        SetBuilder.SetConstantBuffer(0, ConstantBuffer)
                  .SetConstantBuffer(1, Desc.ViewConstants)
                  .SetRayTracingAccelStruct(0, Desc.SceneTLAS)
                  .SetTextureSRV(1, Desc.GBufferWorldPos)
                  .SetTextureSRV(2, Desc.GBufferNormal)
                  .SetTextureSRV(3, Desc.GBufferMaterial);

        if (Desc.RTVertices)
            SetBuilder.SetStructuredBufferSRV(5, Desc.RTVertices);
        if (Desc.RTIndices)
            SetBuilder.SetStructuredBufferSRV(6, Desc.RTIndices);
        if (Desc.RTInstanceInfo)
            SetBuilder.SetStructuredBufferSRV(7, Desc.RTInstanceInfo);

        if (Desc.LinearSampler)
            SetBuilder.SetSampler(2, Desc.LinearSampler);

        SetBuilder.SetTextureUAV(0, Desc.OutputTexture);

        if (Desc.DebugStatsTexture && Desc.DebugBounceStats)
            SetBuilder.SetTextureUAV(1, Desc.DebugStatsTexture);

        nvrhi::BindingSetHandle BindingSet = Device->createBindingSet(
            SetBuilder.Build(), BindingLayout);

        if (!BindingSet)
        {
            HLVM_LOG(LogGI, err, TXT("FGIPass: failed to create per-frame binding set"));
            return;
        }

        RTPipeline.DispatchRays(CmdList, Desc.OutputWidth, Desc.OutputHeight, 1, BindingSet);

        OutputTexture = Desc.OutputTexture;
    }

} // namespace GI
