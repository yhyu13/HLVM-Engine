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
#include "Renderer/Common/FLight.h"
#include "Renderer/GI/GICVars.h"
#include "Renderer/ShaderMake/ShaderBlob.h"
#include "Utility/CVar/CVarMacros.h"

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <vector>

#include <nvrhi/nvrhi.h>

DECLARE_LOG_CATEGORY(LogGI)

namespace GI
{
    // GI constants cbuffer layout (must match GIPathTracing.hlsl GIConstants).
    // 7 * float4 = 112 bytes.
    struct FGIConstantsData
    {
        float LightDir[4];
        float AmbientColor[4];
        float CameraPos[4];
        float Params[4];   // x=MaxBounces, y=SPP, z=ShadowTMin, w=ShadowTMax
        float Params2[4];  // x=AmbientScale, y=RayTMin, z=RayTMax, w=ShadowEnable
        float Params3[4];  // x=EnableRR, y=RussianRouletteMinSurvival, z=DebugStatsEnabled, w=LightCount
        float Params4[4];  // x=EnableNEE, y=MISPower, z=SingleLightNEE, w=BSDFDirectMIS
        float Params5[4];  // x=DebugMode, yzw=unused
    };
    static_assert(sizeof(FGIConstantsData) == 128, "FGIConstantsData must be 128 bytes (8x float4)");

    namespace
    {
        // Default directional light direction (toward the light) and intensity.
        // Keep in sync with WriteConstants() LightDir.
        constexpr float DefaultLightDir[3] = { 0.577f, 0.577f, 0.577f };
        constexpr float DefaultLightIntensity = 1.0f;

        Renderer::FLight BuildDefaultDirectionalLight(const float* Dir, float Intensity)
        {
            Renderer::FLight Light{};
            Light.position[0] = 0.0f;
            Light.position[1] = 0.0f;
            Light.position[2] = 0.0f;
            Light.range = 1e20f;

            Light.direction[0] = Dir[0];
            Light.direction[1] = Dir[1];
            Light.direction[2] = Dir[2];
            Light.intensity = Intensity;

            Light.color[0] = 1.0f;
            Light.color[1] = 1.0f;
            Light.color[2] = 1.0f;
            Light.type = static_cast<uint32_t>(Renderer::ELightType::Directional);

            Light.innerConeAngle = 0.0f;
            Light.outerConeAngle = 0.0f;
            Light.areaWidth = 0.0f;
            Light.areaHeight = 0.0f;

            Light.flags = Renderer::kLightFlag_CastShadow;
            Light.shadowMapIndex = Renderer::kNoShadowMap;
            return Light;
        }

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
        if (!CreateBindingLayout())
            return false;
        if (!CreatePipeline())
            return false;
        if (!CreateConstantBuffer())
            return false;
        if (!UploadLights())
            return false;

        bIsInitialized = true;
        HLVM_LOG(LogGI, info, TXT("FGIPass initialized (shader dir: %s)"), *InShaderDataDir);
        return true;
    }

    void FGIPass::Shutdown()
    {
        // FRayTracingPipeline's copy/move ops are deleted, so we can't reassign.
        // Shutdown() is a public method that clears internal state.
        ShaderLibrary          = nullptr;
        RTPipeline.Shutdown();
        BindingLayout          = nullptr;
        ConstantBuffer         = nullptr;
        LightsBuffer           = nullptr;
        LightsCount            = 0;
        OutputTexture          = nullptr;
        DummyDebugStatsTexture = nullptr;
        Device                 = nullptr;
        Scene                  = nullptr;
        bIsInitialized          = false;
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

        // MaxPayloadSize = 64 (GIPayload is a compact fully-used 64-byte struct in
        // GIPathTracing.hlsl - see its comment about slangc per-entry dead-stripping)
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

        BindingLayout = RTPipeline.GetBindingLayout();
        if (!BindingLayout)
        {
            HLVM_LOG(LogGI, err, TXT("FRayTracingPipeline did not produce a binding layout"));
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
        //   t7 = Lights      (StructuredBuffer<FLight>)
        //   t8 = RTInstanceInfo (StructuredBuffer<FInstanceInfo>)
        //   s2 = LinearSampler
        //   u0 = OutputTexture (RWTexture2D<float4>)
        //   u1 = DebugStatsTexture (RWTexture2D<float4>, bound always; dummy when unused)
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
               .AddStructuredBufferSRV(7)        // t7 - Lights
               .AddStructuredBufferSRV(8)        // t8 - RTInstanceInfo
               .AddSampler(2)                    // s2 - LinearSampler
               .AddTextureUAV(0)                 // u0 - OutputTexture
               .AddTextureUAV(1);                // u1 - DebugStatsTexture

        // The actual binding layout handle is created inside FRayTracingPipeline::FinalizePipeline();
        // we only need to make sure the builder was populated here.
        return true;
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

    bool FGIPass::UploadLights()
    {
        if (LightsBuffer)
            return true;

        std::vector<Renderer::FLight> LightsToUpload;

        if (Scene && !Scene->Lights.empty())
        {
            LightsToUpload.assign(Scene->Lights.begin(), Scene->Lights.end());
        }
        else
        {
            // Use the current CVar directional sun direction. Falls back to the
            // canonical sun direction if the CVar vector is degenerate.
            float DirX = CVar_r_GI_LightDirX.GetValue();
            float DirY = CVar_r_GI_LightDirY.GetValue();
            float DirZ = CVar_r_GI_LightDirZ.GetValue();
            float Len  = std::sqrt(DirX * DirX + DirY * DirY + DirZ * DirZ);
            if (Len < 1e-6f)
            {
                DirX = DefaultLightDir[0];
                DirY = DefaultLightDir[1];
                DirZ = DefaultLightDir[2];
                Len  = 1.0f;
            }
            const float Dir[3] = { DirX / Len, DirY / Len, DirZ / Len };
            LightsToUpload.push_back(BuildDefaultDirectionalLight(Dir, DefaultLightIntensity));
        }

        if (LightsToUpload.empty())
        {
            HLVM_LOG(LogGI, err, TXT("FGIPass::UploadLights: no lights to upload"));
            return false;
        }

        const size_t BufferSize = LightsToUpload.size() * sizeof(Renderer::FLight);

        nvrhi::BufferDesc Desc;
        Desc.byteSize = BufferSize;
        Desc.structStride = sizeof(Renderer::FLight);
        Desc.initialState = nvrhi::ResourceStates::ShaderResource;
        Desc.keepInitialState = true;
        Desc.debugName = "FGIPass.Lights";
        LightsBuffer = Device->createBuffer(Desc);
        if (!LightsBuffer)
        {
            HLVM_LOG(LogGI, err, TXT("FGIPass::UploadLights: failed to create lights buffer"));
            return false;
        }

        nvrhi::CommandListHandle Cmd = Device->createCommandList();
        Cmd->open();
        Cmd->writeBuffer(LightsBuffer, LightsToUpload.data(), BufferSize);
        Cmd->close();
        Device->executeCommandList(Cmd);
        Device->waitForIdle();

        LightsCount = static_cast<uint32_t>(LightsToUpload.size());
        HLVM_LOG(LogGI, info, TXT("FGIPass::UploadLights: uploaded {} light(s)"), LightsCount);
        return true;
    }

    void FGIPass::WriteConstants(nvrhi::ICommandList* CmdList, const FGIPassDesc& Desc)
    {
        // Per-frame constants: Desc overrides CVar defaults for bounce/SPP so tests
        // can set them without mutating global CVars; everything else is CVar-driven.
        const uint32_t maxBounces   = (Desc.MaxBounces > 0)
            ? Desc.MaxBounces
            : static_cast<uint32_t>(CVar_r_GI_MaxBounces.GetValue());
        const uint32_t spp          = (Desc.SamplesPerPixel > 0)
            ? Desc.SamplesPerPixel
            : static_cast<uint32_t>(CVar_r_GI_SPP.GetValue());

        const float LightDir[4]     = {
            CVar_r_GI_LightDirX.GetValue(),
            CVar_r_GI_LightDirY.GetValue(),
            CVar_r_GI_LightDirZ.GetValue(),
            DefaultLightIntensity };
        const float AmbientColor[4] = { 0.6f, 0.6f, 0.65f, 0.0f };
        const float CameraPos[4]    = { 0.0f, 0.0f, 0.0f, 1.0f };

        // Keep the internal directional light in sync with the per-frame LightDir constant,
        // but only when we own the fallback buffer (no scene lights, no caller-provided buffer).
        if (!Desc.LightsBuffer && !Scene && LightsBuffer)
        {
            const float Dir[3] = { LightDir[0], LightDir[1], LightDir[2] };
            Renderer::FLight Light = BuildDefaultDirectionalLight(Dir, DefaultLightIntensity);
            CmdList->writeBuffer(LightsBuffer, &Light, sizeof(Light));
        }

        FGIConstantsData Data;
        std::memcpy(Data.LightDir,     LightDir,     sizeof(LightDir));
        std::memcpy(Data.AmbientColor, AmbientColor, sizeof(AmbientColor));
        std::memcpy(Data.CameraPos,    CameraPos,    sizeof(CameraPos));
        Data.Params[0]  = static_cast<float>(maxBounces);
        Data.Params[1]  = static_cast<float>(spp);
        Data.Params[2]  = CVar_r_GI_ShadowTMin.GetValue();
        Data.Params[3]  = CVar_r_GI_ShadowTMax.GetValue();
        Data.Params2[0] = (Desc.AmbientScale >= 0.0f)
            ? Desc.AmbientScale
            : CVar_r_GI_AmbientScale.GetValue();
        Data.Params2[1] = CVar_r_GI_RayTMin.GetValue();
        Data.Params2[2] = CVar_r_GI_RayTMax.GetValue();
        Data.Params2[3] = CVar_r_GI_ShadowRays.GetValue() ? 1.0f : 0.0f;
        Data.Params3[0] = Desc.EnableRR ? 1.0f : 0.0f;
        Data.Params3[1] = (Desc.RussianRoulette > 0.0f)
            ? Desc.RussianRoulette
            : CVar_r_GI_RussianRoulette.GetValue();
        Data.Params3[2] = (Desc.DebugStatsTexture && Desc.DebugBounceStats) ? 1.0f : 0.0f;

        // Explicit light-count path: caller-provided buffer wins; otherwise use internal.
        uint32_t ActiveLightCount = Desc.LightsBuffer ? Desc.LightCount : LightsCount;
        Data.Params3[3] = static_cast<float>(ActiveLightCount);

        Data.Params4[0] = CVar_r_GI_EnableNEE.GetValue() ? 1.0f : 0.0f;
        Data.Params4[1] = CVar_r_GI_MISPower.GetValue();
        Data.Params4[2] = CVar_r_GI_SingleLightNEE.GetValue() ? 1.0f : 0.0f;
        Data.Params4[3] = CVar_r_GI_BSDFDirectMIS.GetValue() ? 1.0f : 0.0f;

        int DebugMode = CVar_r_GI_DebugMode.GetValue();
        if (const char* DebugModeEnv = std::getenv("HLVM_PT_DEBUG_MODE"))
        {
            DebugMode = std::atoi(DebugModeEnv);
        }
        Data.Params5[0] = static_cast<float>(DebugMode);

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

        nvrhi::BufferHandle ActiveLightsBuffer = Desc.LightsBuffer ? Desc.LightsBuffer : LightsBuffer;
        if (ActiveLightsBuffer)
            SetBuilder.SetStructuredBufferSRV(7, ActiveLightsBuffer);

        if (Desc.RTInstanceInfo)
            SetBuilder.SetStructuredBufferSRV(8, Desc.RTInstanceInfo);

        if (Desc.LinearSampler)
            SetBuilder.SetSampler(2, Desc.LinearSampler);

        SetBuilder.SetTextureUAV(0, Desc.OutputTexture);

        nvrhi::TextureHandle DebugStatsUAV = Desc.DebugStatsTexture;
        if (DebugStatsUAV && Desc.DebugBounceStats)
        {
            CmdList->setTextureState(DebugStatsUAV, nvrhi::AllSubresources,
                                     nvrhi::ResourceStates::UnorderedAccess);
        }
        else
        {
            if (!DummyDebugStatsTexture)
            {
                nvrhi::TextureDesc DummyDesc;
                DummyDesc.dimension = nvrhi::TextureDimension::Texture2D;
                DummyDesc.width = 1;
                DummyDesc.height = 1;
                DummyDesc.format = nvrhi::Format::RGBA32_FLOAT;
                DummyDesc.isUAV = true;
                DummyDesc.initialState = nvrhi::ResourceStates::UnorderedAccess;
                DummyDesc.keepInitialState = true;
                DummyDesc.debugName = "FGIPass.DummyDebugStats";
                DummyDebugStatsTexture = Device->createTexture(DummyDesc);
            }
            DebugStatsUAV = DummyDebugStatsTexture;
            CmdList->setTextureState(DebugStatsUAV, nvrhi::AllSubresources,
                                     nvrhi::ResourceStates::UnorderedAccess);
        }
        SetBuilder.SetTextureUAV(1, DebugStatsUAV);

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
