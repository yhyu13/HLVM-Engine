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
#include <iostream>
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
        // Material texture slots in GIPathTracing.hlsl (t9..t40). Keep in sync
        // with MAX_MATERIAL_TEXTURES in that shader.
        constexpr uint32_t MaxMaterialTextures = 32;

        // Default point-light positions for the Sponza test scene at
        // 0.01 scale. The Sponza is a closed building, so a single
        // directional sun (DefaultLightDir) is fully occluded from
        // every interior surface and NEE direct lighting produces zero
        // contribution. Adding point lights INSIDE the building gives
        // NEE a source that's unoccluded from the surrounding
        // surfaces, producing a properly lit image. Positions are
        // placed at mid-hall height (~6 m at 0.01 scale = 0.06),
        // well inside the main hall (the camera is at z=0.08 looking
        // at z=0, so the hall extends roughly z=-0.5 to z=+0.5).
        // Lights are placed at moderate distance from the camera
        // and from any wall so each light illuminates a broad area
        // without blowing out the surface right in front of it.
        constexpr float DefaultPointPositions[3][3] = {
            { 0.0f,  0.08f, -0.20f }, // center-back, well inside the hall
            { 0.20f, 0.08f,  0.20f }, // right-front, well inside the hall
            {-0.20f, 0.08f,  0.20f }, // left-front, well inside the hall
        };
        constexpr float DefaultPointColor[3] = { 1.0f, 0.9f, 0.75f }; // warm white
        constexpr float DefaultPointIntensity = 4.0f;
        constexpr float DefaultPointRange = 0.6f; // 60 cm at 0.01 scale = 6 m unscaled

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

        Renderer::FLight BuildDefaultPointLight(const float* Pos, const float* Color, float Intensity, float Range)
        {
            Renderer::FLight Light{};
            Light.position[0] = Pos[0];
            Light.position[1] = Pos[1];
            Light.position[2] = Pos[2];
            Light.range = Range;

            Light.direction[0] = 0.0f;
            Light.direction[1] = -1.0f; // unused for point lights but conventionally -Y
            Light.direction[2] = 0.0f;
            Light.intensity = Intensity;

            Light.color[0] = Color[0];
            Light.color[1] = Color[1];
            Light.color[2] = Color[2];
            Light.type = static_cast<uint32_t>(Renderer::ELightType::Point);

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
        // v214: MaterialPlaceholderTexture (Phase 3b per-texel bounce albedo
        // array, t9..t40, filled with white pixels so out-of-range or empty
        // slots return 1,1,1 instead of sampling garbage). Was previously
        // created+uploaded lazily on first DispatchRays, which forced a
        // per-frame `Device->waitForIdle()` at line 671 and an open/close
        // command list inside the dispatch hot path. The `if (!Material*)`
        // guard made it one-shot per process, so the lifecycle move is
        // steady-state byte-identical.
        {
            nvrhi::TextureDesc PlaceholderDesc;
            PlaceholderDesc.dimension  = nvrhi::TextureDimension::Texture2D;
            PlaceholderDesc.width       = 1;
            PlaceholderDesc.height      = 1;
            PlaceholderDesc.format      = nvrhi::Format::RGBA8_UNORM;
            PlaceholderDesc.initialState = nvrhi::ResourceStates::ShaderResource;
            PlaceholderDesc.keepInitialState = true;
            PlaceholderDesc.debugName   = "FGIPass.MaterialPlaceholder";
            MaterialPlaceholderTexture = Device->createTexture(PlaceholderDesc);
            nvrhi::CommandListHandle WriteCmd = Device->createCommandList();
            WriteCmd->open();
            const uint32_t WhitePixel = 0xFFFFFFFFu;
            WriteCmd->writeTexture(MaterialPlaceholderTexture, 0, 0, &WhitePixel, 4);
            WriteCmd->close();
            Device->executeCommandList(WriteCmd);
            Device->waitForIdle();
        }

        bIsInitialized = true;
        HLVM_LOG(LogGI, info, TXT("FGIPass initialized (shader dir: {})"), *InShaderDataDir);
        return true;
    }

    void FGIPass::Shutdown()
    {
        // FRayTracingPipeline's copy/move ops are deleted, so we can't reassign.
        // Shutdown() is a public method that clears internal state.
        ShaderLibrary          = nullptr;
        RTPipeline.Shutdown();
        BindingLayout          = nullptr;
        // 2026-08-16 (six-role-pipeline v2): UAVBindingLayout removed.
        ConstantBuffer         = nullptr;
        LightsBuffer           = nullptr;
        LightsCount            = 0;
        OutputTexture          = nullptr;
        DummyDebugStatsTexture = nullptr;
        DummySampleInfoTexture = nullptr;
        DummyDirectTexture     = nullptr;
        MaterialPlaceholderTexture = nullptr;
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
            HLVM_LOG(LogGI, err, TXT("Failed to read GIPathTracing.sblob at {}"), TO_TCHAR_CSTR(SblobPath.c_str()));
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

        // MaxPayloadSize = 80 (v210, 2026-08-21): the ReSTIR GI candidate
        // sample now carries the second-path-vertex state back from ClosestHit
        // (x2 normal + packed x2 ID), growing GIPayload from 64 to 80 bytes.
        // TestPathTraceGI's 64-byte payload is still compatible (maxPayloadSize
        // only needs to be >= the shaders' actual payload).
        // MaxAttributeSize = 8 (barycentric float2 attributes - standard)
        if (!RTPipeline.FinalizePipeline(/*MaxPayloadSize*/ 80, /*MaxAttributeSize*/ 8))
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
        // 2026-08-16 (six-role-pipeline v2): REVERT the v22 split. The
        // separate SRV-only + UAV-only binding layouts caused GBuffer SRV
        // reads to return zero (mode 20/21/22 dumped solid black despite
        // valid per-frame texture handles). The proven-control
        // TestCornellBoxGI uses a single binding set containing both SRV
        // and UAV resources, and its GI dispatch works correctly.
        //
        // The v22 split's justification (silence a Vulkan validation
        // warning) was weaker than the data-corruption it caused. With the
        // validation layer STUBBED at DeviceManagerVk4_LifeCycle.cpp
        // anyway, the warning is invisible; reverting gains correctness
        // at zero cost.
        //
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
        //   t9 = MaterialTextures[] (Texture2D<float4> array, MaxMaterialTextures slots)
        //   s2 = LinearSampler
        //   u0 = OutputTexture (RWTexture2D<float4>)  -- merged back into primary layout
        //   u1 = DebugStatsTexture (RWTexture2D<float4>, bound always; dummy when unused)
        //   u2 = OutputDirection (RWTexture2D<float4>)  -- ReSTIR primary ray direction
        //   u3 = SampleInfo (RWTexture2D<float4>)  -- ReSTIR x2 normal + sample pdf (v210)
        //   u4 = DirectTexture (RWTexture2D<float4>)  -- primary direct+ambient+sky (v210)
        auto& Builder = RTPipeline.CreateBindingLayout();
        // All offsets zero — the builder's AddXxx methods already apply
        // the SPIR-V register shifts (TRegShift=0, SRegShift=128, BRegShift=256,
        // URegShift=384) and we want nvrhi to NOT add a second shift on top.
        Builder.SetBindingOffsets(0, 0, 0, 0)
               .SetVisibility(nvrhi::ShaderType::All)
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
               .AddTextureUAV(0)                 // u0 - OutputTexture (radiance)
               .AddTextureUAV(1)                 // u1 - DebugStatsTexture (bound always; dummy when unused)
               .AddTextureUAV(2)                 // u2 - OutputDirection (primary ray dir for ReSTIR)
               .AddTextureUAV(3)                 // u3 - SampleInfo (x2 normal + pdf, v210)
               .AddTextureUAV(4);                // u4 - DirectTexture (primary direct+ambient, v210)
        // t9 - per-texel bounce albedo texture ARRAY (Phase 3b). One binding
        // with descriptorCount = MaxMaterialTextures; the shader indexes it by
        // RTInstanceInfo.AlbedoTextureIndex. Slots without a caller texture are
        // filled with a white placeholder at dispatch time.
        {
            nvrhi::BindingLayoutItem MatTexArray = nvrhi::BindingLayoutItem::Texture_SRV(9);
            MatTexArray.size = MaxMaterialTextures;
            Builder.AddItem(MatTexArray);
        }

        // The actual binding layout handle is created inside
        // FRayTracingPipeline::FinalizePipeline(); we only need to make sure
        // the builder was populated here.
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

            // Also add a few default point lights INSIDE the Sponza so
            // NEE direct lighting has unoccluded sources. The Sponza is
            // a closed building; the sun alone is fully occluded from
            // every interior surface and produces zero contribution. The
            // point lights at the default positions are inside the
            // main hall (close to the camera) and will reach the
            // surrounding walls without occlusion, producing a
            // properly lit image.
            for (int i = 0; i < 3; ++i)
            {
                LightsToUpload.push_back(BuildDefaultPointLight(
                    DefaultPointPositions[i], DefaultPointColor,
                    DefaultPointIntensity, DefaultPointRange));
            }
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
        // v140: AmbientColor now sourced from Desc.AmbientColor (default in FGIPassDesc
        // preserves the old hardcoded value for backward-compat with TestPathTraceGI).
        const float* AmbientColorPtr = Desc.AmbientColor;
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
        std::memcpy(Data.AmbientColor, AmbientColorPtr, sizeof(Data.AmbientColor));
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

        // DIAG (2026-08-11): DebugMode flow diagnostic — the env override
        // (HLVM_PT_DEBUG_MODE) vs CVar (r_GI_DebugMode) vs the cbuffer value.
        // Gated behind HLVM_RGI_DIAG=1 (was a default-ON std::cerr, violating
        // the logging-macros rule).
        const char* DebugModeEnvForLog = std::getenv("HLVM_PT_DEBUG_MODE");
        if (std::getenv("HLVM_RGI_DIAG"))
        {
            const FString EnvForLog = DebugModeEnvForLog
                ? FString(DebugModeEnvForLog) : FString(TXT("<null>"));
            HLVM_LOG(LogGI, info,
                TXT("FGIPass::WriteConstants: DebugMode effective={} cvar={} env_var={} Params5[0]={}"),
                DebugMode, CVar_r_GI_DebugMode.GetValue(),
                *EnvForLog,
                Data.Params5[0]);
        }

        CmdList->writeBuffer(ConstantBuffer, &Data, sizeof(Data));
    }

    void FGIPass::DispatchRays(nvrhi::ICommandList* CmdList, const FGIPassDesc& Desc)
    {
        // DIAG (2026-08-11): dispatch-entry probe (was a default-ON
        // std::cerr). Gated behind HLVM_RGI_DIAG=1.
        if (std::getenv("HLVM_RGI_DIAG"))
        {
            HLVM_LOG(LogGI, info,
                TXT("FGIPass::DispatchRays entry: bIsInitialized={} RTPipeline.Initialized={} SceneTLAS=0x{:x} OutputTex=0x{:x} Frame={}"),
                bIsInitialized ? 1 : 0, RTPipeline.IsInitialized() ? 1 : 0,
                reinterpret_cast<uintptr_t>(Desc.SceneTLAS.Get()),
                reinterpret_cast<uintptr_t>(Desc.OutputTexture.Get()),
                Desc.FrameIndex);
        }
        // DIAGNOSTIC (v3 — six-role-pipeline): early-return log so we can see
        // whether the dispatch body is even reached when gi_raw dumps return 0,0,0.
        if (!bIsInitialized || !RTPipeline.IsInitialized())
        {
            HLVM_LOG(LogGI, warn, TXT("FGIPass::DispatchRays: EARLY-RETURN bIsInitialized={} RTPipeline.Initialized={}"),
                bIsInitialized, RTPipeline.IsInitialized());
            return;
        }

        if (!Desc.SceneTLAS || !Desc.OutputTexture || !Desc.ViewConstants)
        {
            HLVM_LOG(LogGI, err, TXT("FGIPass::DispatchRays: missing required handles"));
            return;
        }

        // DIAGNOSTIC (v3): log entry state. Helps correlate dump frame index
        // with the dispatch and confirms the OutputTexture handle is non-null.
        HLVM_LOG(LogGI, info, TXT("FGIPass::DispatchRays ENTER: OutputTex=0x{:x} OutputW={} OutputH={} Frame={} CmdList=0x{:x}"),
            reinterpret_cast<uintptr_t>(Desc.OutputTexture.Get()),
            Desc.OutputWidth, Desc.OutputHeight, Desc.FrameIndex,
            reinterpret_cast<uintptr_t>(CmdList));

        // v128 (six-role-pipeline, tick 113, 2026-07-30): handle-identity probe.
        // Compare with TestReSTIR_GI_Temporal.cpp:1531 (RenderGBuffer) log
        // line. Matching handles = binding layer is wrong at descriptor level.
        // Differing handles = texture handle identity issue (recreation).
        if (Desc.FrameIndex < 4u)
        {
            HLVM_LOG(LogGI, info, TXT("[handle-id] FGIPass::DispatchRays: GBufferMaterial={:#x} WorldPos={:#x} Normal={:#x}"),
                reinterpret_cast<uintptr_t>(Desc.GBufferMaterial.Get()), reinterpret_cast<uintptr_t>(Desc.GBufferWorldPos.Get()), reinterpret_cast<uintptr_t>(Desc.GBufferNormal.Get()));
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

        // v22 split (six-role-pipeline): separate SRV-only + UAV-only binding layouts to
        // avoid the nvrhi-deferred-barrier-ordering pattern documented in
        // gpu-rendering-bisect-debug/references/nvrhi-deferred-barrier-ordering.md.
        // The single-binding-set approach (v1-v21) caused nvrhi to bind descriptor sets
        // before the implicit UAV->GENERAL barrier landed, triggering the
        // "A command list should be executed before it is reopened" pattern 7x/run.
        //
        // 2026-08-16 (six-role-pipeline v2): v22 split reverted. SRV + UAV go
        // into the SAME binding set, matching the proven-control
        // TestCornellBoxGI pattern. The single-set validation warning is
        // acceptable (validation is stubbed at DeviceManagerVk4_LifeCycle.cpp
        // anyway).
        FBindingSetBuilder SRVBuilder;
        SRVBuilder.SetConstantBuffer(0, ConstantBuffer)
                  .SetConstantBuffer(1, Desc.ViewConstants)
                  .SetRayTracingAccelStruct(0, Desc.SceneTLAS)
                  .SetTextureSRV(1, Desc.GBufferWorldPos)
                  .SetTextureSRV(2, Desc.GBufferNormal)
                  .SetTextureSRV(3, Desc.GBufferMaterial);

        if (Desc.RTVertices)
            SRVBuilder.SetStructuredBufferSRV(5, Desc.RTVertices);
        if (Desc.RTIndices)
            SRVBuilder.SetStructuredBufferSRV(6, Desc.RTIndices);

        nvrhi::BufferHandle ActiveLightsBuffer = Desc.LightsBuffer ? Desc.LightsBuffer : LightsBuffer;
        if (ActiveLightsBuffer)
            SRVBuilder.SetStructuredBufferSRV(7, ActiveLightsBuffer);

        if (Desc.RTInstanceInfo)
            SRVBuilder.SetStructuredBufferSRV(8, Desc.RTInstanceInfo);

        if (Desc.LinearSampler)
            SRVBuilder.SetSampler(2, Desc.LinearSampler);

        // u0 — primary output radiance. Bound unconditionally.
        SRVBuilder.SetTextureUAV(0, Desc.OutputTexture);

        // u1 — debug bounce stats (optional, bound always; dummy when unused).
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
        SRVBuilder.SetTextureUAV(1, DebugStatsUAV);

        // u2 — primary sample ray direction (ReSTIR GI). The shader writes this
        // UNCONDITIONALLY at the raw dispatch coord (GIPathTracing.hlsl, the
        // `OutputDirection[pixel]` store), so the bound resource MUST be at
        // least the dispatch extent. A 1x1 dummy was used here previously,
        // which made every thread but one an out-of-bounds UAV store on any
        // consumer that does not supply the texture. Fall back to the mandatory
        // output UAV instead: it is the one resource guaranteed present, both
        // consumers size it to their own dispatch extent, and the shader
        // overwrites it later in the same invocation, so the stray write cannot
        // affect the result. Same idiom as FReSTIRPass::DispatchGeneration's
        // optional-t4 ternary. Contrast u1 above, which is guarded by
        // Params3.z instead and therefore may safely keep its dummy.
        nvrhi::TextureHandle DirectionUAV = Desc.OutputDirection
            ? Desc.OutputDirection
            : Desc.OutputTexture;
        CmdList->setTextureState(DirectionUAV, nvrhi::AllSubresources,
                                 nvrhi::ResourceStates::UnorderedAccess);
        SRVBuilder.SetTextureUAV(2, DirectionUAV);

        // v210 (2026-08-21, ZetaRay ground-truth port, Phase 1): u3 SampleInfo
        // (x2 normal + primary-sample pdf) and u4 DirectTexture (primary
        // direct + ambient + sky-on-primary-miss). Unlike u2, these are ONLY
        // written by the ReSTIR GI consumer's shader (which declares them and
        // always supplies real dispatch-sized textures), so absent callers
        // (TestPathTraceGI / TestCornellBoxGI, whose shaders do not declare
        // u3/u4) get 1x1 dummies purely as descriptor-set placeholders.
        nvrhi::TextureHandle SampleInfoUAV = Desc.SampleInfoTexture;
        if (!SampleInfoUAV)
        {
            if (!DummySampleInfoTexture)
            {
                nvrhi::TextureDesc DummyDesc;
                DummyDesc.dimension = nvrhi::TextureDimension::Texture2D;
                DummyDesc.width = 1;
                DummyDesc.height = 1;
                DummyDesc.format = nvrhi::Format::RGBA32_FLOAT;
                DummyDesc.isUAV = true;
                DummyDesc.initialState = nvrhi::ResourceStates::UnorderedAccess;
                DummyDesc.keepInitialState = true;
                DummyDesc.debugName = "FGIPass.DummySampleInfo";
                DummySampleInfoTexture = Device->createTexture(DummyDesc);
            }
            SampleInfoUAV = DummySampleInfoTexture;
        }
        CmdList->setTextureState(SampleInfoUAV, nvrhi::AllSubresources,
                                 nvrhi::ResourceStates::UnorderedAccess);
        SRVBuilder.SetTextureUAV(3, SampleInfoUAV);

        nvrhi::TextureHandle DirectUAV = Desc.DirectTexture;
        if (!DirectUAV)
        {
            if (!DummyDirectTexture)
            {
                nvrhi::TextureDesc DummyDesc;
                DummyDesc.dimension = nvrhi::TextureDimension::Texture2D;
                DummyDesc.width = 1;
                DummyDesc.height = 1;
                DummyDesc.format = nvrhi::Format::RGBA32_FLOAT;
                DummyDesc.isUAV = true;
                DummyDesc.initialState = nvrhi::ResourceStates::UnorderedAccess;
                DummyDesc.keepInitialState = true;
                DummyDesc.debugName = "FGIPass.DummyDirect";
                DummyDirectTexture = Device->createTexture(DummyDesc);
            }
            DirectUAV = DummyDirectTexture;
        }
        CmdList->setTextureState(DirectUAV, nvrhi::AllSubresources,
                                 nvrhi::ResourceStates::UnorderedAccess);
        SRVBuilder.SetTextureUAV(4, DirectUAV);

        // Build SRV+UAV binding set as one (v22 split reverted).
        nvrhi::BindingSetDesc SRVSetDesc = SRVBuilder.Build();
        // Phase 3b: fill the t9 descriptor array (arrayElement 0..31); the
        // shader's dynamic AlbedoTextureIndex must always hit a valid slot.
        for (uint32_t i = 0; i < MaxMaterialTextures; ++i)
        {
            nvrhi::TextureHandle MatTex = (i < Desc.MaterialTextures.size())
                ? Desc.MaterialTextures[i] : MaterialPlaceholderTexture;
            nvrhi::BindingSetItem ArrayItem = nvrhi::BindingSetItem::Texture_SRV(9, MatTex);
            ArrayItem.arrayElement = i;
            SRVSetDesc.bindings.push_back(ArrayItem);
        }
        // DIAG (2026-08-11): binding-layout/set dump + invariant check.
        // The per-frame dump was always-on noise (gated behind HLVM_RGI_DIAG);
        // the layout/set match itself remains a fatal invariant.
        if (const nvrhi::BindingLayoutDesc* LayoutDesc = BindingLayout->getDesc())
        {
            if (std::getenv("HLVM_RGI_DIAG"))
            {
                HLVM_LOG(LogGI, info, TXT("[v23-diag] binding layout item count={}"), LayoutDesc->bindings.size());
                for (size_t i = 0; i < LayoutDesc->bindings.size(); ++i)
                {
                    const auto& It = LayoutDesc->bindings[i];
                    HLVM_LOG(LogGI, info, TXT("[v23-diag]   layout[{}] slot={} type={} size={}"),
                        i, It.slot, static_cast<int>(It.type), It.size);
                }
                HLVM_LOG(LogGI, info, TXT("[v23-diag] binding set item count={}"), SRVSetDesc.bindings.size());
                for (size_t i = 0; i < SRVSetDesc.bindings.size(); ++i)
                {
                    const auto& It = SRVSetDesc.bindings[i];
                    HLVM_LOG(LogGI, info, TXT("[v23-diag]   set[{}] slot={} type={} resHandle=0x{:x}"),
                        i, It.slot, static_cast<int>(It.type),
                        reinterpret_cast<uintptr_t>(It.resourceHandle));
                }
            }
            // Same policy as FReBLURPass: a layout/set mismatch is a hard
            // invariant violation (fatal), not a logged warning.
            HLVM_ENSURE(FBindingSetBuilder::ValidateAgainstLayout(SRVSetDesc, *LayoutDesc));
        }
        nvrhi::BindingSetHandle SRVBindingSet = Device->createBindingSet(
            SRVSetDesc, BindingLayout);
        if (!SRVBindingSet)
        {
            HLVM_LOG(LogGI, err, TXT("FGIPass: failed to create per-frame binding set (v22-revert)"));
            return;
        }
        if (std::getenv("HLVM_RGI_DIAG"))
        {
            HLVM_LOG(LogGI, info, TXT("FGIPass: per-frame binding set created OK (handle=0x{:x})"),
                reinterpret_cast<uintptr_t>(SRVBindingSet.Get()));
        }

        // v22 split reverted: dispatch with ONE binding set containing SRV + UAV.
        // The deferred-barrier-ordering warning may re-emerge (validation layer
        // stubbed in this build, so we won't see it). If GBuffer SRV reads now
        // return non-zero (mode 20/21/22 dump non-black), the revert succeeded.
        RTPipeline.DispatchRays(CmdList, Desc.OutputWidth, Desc.OutputHeight, 1, SRVBindingSet);

        // DIAGNOSTIC (v3): log dispatch return. If this line doesn't appear,
        // the dispatch call hangs/fails fatally; if it does appear, the
        // dispatch returned normally and the issue is downstream
        // (layout tracking, UAV write dropping, etc.).
        if (std::getenv("HLVM_RGI_DIAG"))
        {
            HLVM_LOG(LogGI, info, TXT("FGIPass::DispatchRays EXIT: dispatch returned, OutputTex=0x{:x}"),
                reinterpret_cast<uintptr_t>(Desc.OutputTexture.Get()));
        }

        OutputTexture = Desc.OutputTexture;
    }

} // namespace GI
