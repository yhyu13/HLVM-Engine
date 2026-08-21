/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 *
 * TestReSTIR_GI_Temporal — End-to-end Sponza + ReSTIR GI pipeline.
 *
 * Pipeline (modes 0..5, 13, 14 of GIPathTracing.hlsl debug modes are honored):
 *
 *   Sponza GLTF (Samples/Assets/sponza/Sponza01.gltf)
 *      ↓
 *   BLAS / TLAS (one instance per static mesh, scaled 0.01 because Sponza is large)
 *      ↓
 *   GBuffer (GBufferSponzaVS/PS) -> WorldPos / Normal / Material textures
 *      ↓
 *   GI Ray Trace (FGIPass w/ GIPathTracing.hlsl — 64-byte compact payload)
 *      ↓ OutputTexture (HDR rgb + first-hit dist alpha)
 *   Bilateral Denoise (FBilateralDenoisePass) -> DenoisedHDR
 *      ↓
 *   ReSTIR Generate (FReSTIRPass::DispatchGeneration) -> Reservoir0/1
 *      ↓
 *   ReSTIR Temporal (FReSTIRPass::DispatchTemporal) -> Merged Reservoir0/1 + OutRadiance
 *      ↓
 *   ReSTIR Spatial (FReSTIRPass::DispatchSpatial, 3x3 + pairwise MIS) -> SpatialRadiance
 *      ↓
 *   GIAccumulate (ACES tonemap + sRGB gamma) -> Display
 *      ↓
 *   Blit to swapchain
 *
 * Debug/verification aids (all inherited from Vibe_Coding/51_PathTraceGI_Debug):
 *   - HLVM_DUMP_RGI=1        dump Output/Denoised/ReSTIR/Display PNGs on the last frame
 *   - HLVM_RGI_ACCUM=N       target frames for accumulation (default 8)
 *   - HLVM_RGI_EXPOSURE=F    pre-tonemap exposure (default 1.0)
 *   - HLVM_PT_DEBUG_MODE=N   shader-side debug mode (passed to GIPathTracing.hlsl)
 *
 * Why this test exists: the previous TestFewBounceGI was renamed to
 * TestCornellBoxGI (commit 2216e71) and the ReSTIR/ReBLUR compute pipelines
 * were left without an end-to-end driver. This test re-integrates them with
 * the proven 64-byte payload GIPathTracing.hlsl — see
 * Vibe_Coding/51_PathTraceGI_Debug/session-PathTraceGI_payload_debug.md
 * for the payload rules that prevent the slangc dead-strip class of bug.
 *
 * Validation: see TestReSTIR_GI_Temporal_Data/validate_restir_gi.py
 * (4 structural checks: black%, color variance, temporal stability, cell variance).
 */

#include "Test.h"

#include "Renderer/DeviceManager.h"
#include "Renderer/Common/FCommonRenderPasses.h"
#include "Renderer/Common/FBindingLayoutBuilder.h"
#include "Renderer/GI/FGIPass.h"
#include "Renderer/Common/FLight.h"
#include "Renderer/PostProcess/FBilateralDenoisePass.h"
#include "Renderer/PostProcess/FReBLURPass.h"
#include "Renderer/PostProcess/FReSTIRPass.h"
#include "Renderer/RayTracing/BLASBuilder.h"
#include "Renderer/GI/GICVars.h"   // v176: r_ReSTIR_MaxM CVar (default 30.0f, see GICVars.h:38)
#include "Renderer/RayTracing/TLASBuilder.h"
#include "Renderer/Scene3D/Scene3DLoader.h"
#include "Renderer/Scene3D/FCornellBoxScene.h"
#include "Renderer/Mesh/StaticMesh.h"
#include "Renderer/Material/PBRMaterial.h"
#include "Renderer/Texture/AsyncTextureLoader.h"
#include "Renderer/ShaderMake/ShaderBlob.h"
#include "Platform/FileSystem/Path.h"
#include "Image/FImageDump.h"
#include <chrono>
#include <numeric>
#include <parallel_hashmap/phmap.h>

#include <nvrhi/utils.h>
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <unordered_map>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <Utility/Timer.h>

// Convenience: timestamp prefix for dump files ("YYYYMMDD_HHMMSS").
static std::string MakeTimestampPrefix()
{
    time_t now = time(nullptr);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", localtime(&now));
    return std::string(buf);
}

DECLARE_LOG_CATEGORY(LogTest)

#if HLVM_VULKAN_RENDERER

// ============================================================================
// Configuration
// ============================================================================

static const char*    WINDOW_TITLE = "ReSTIR GI Temporal — Sponza";
static const uint32_t WIDTH = 800;
static const uint32_t HEIGHT = 600;
static const uint32_t DEFAULT_ACCUM_TARGET_FRAMES = 8;

// GPU-side layouts — must match GIPathTracing.hlsl.
struct FRTVertex
{
    float Position[3];
    float Padding0;
    float Normal[3];
    float Padding1;
    float UV[2];
    float Padding2[2];
};
static_assert(sizeof(FRTVertex) == 48, "FRTVertex must be 48 bytes");

struct FInstanceInfo
{
    uint32_t VertexOffset;
    uint32_t IndexOffset;
    uint32_t VertexCount;
    uint32_t IndexCount;
    float    AlbedoColor[3];
    uint32_t AlbedoTextureIndex;
    uint32_t MaterialFlags;
    float    Roughness;   // gltf roughnessFactor (2026-08-10 Phase 2)
    float    Metallic;    // gltf metallicFactor
    uint32_t Padding;
};
static_assert(sizeof(FInstanceInfo) == 48, "FInstanceInfo must be 48 bytes");

// ============================================================================
// Camera rig
// ============================================================================
// Sponza's coordinate origin is at floor center, with the structure extending
// roughly ±6m after the 0.01 scale we apply when building the TLAS. Place the
// camera outside the structure, looking inward and downward, with a wide FOV.
//
// Lesson inherited from Vibe_Coding/51_PathTraceGI_Debug:
//   - camera must be IN FRONT OF the visible scene, not at its center
//   - FOV must be wide enough (90°) to see the colored side walls
//   - view direction must be -Z, not +Z (left-handed, RH-conventions glitch)
// Sponza vertices live in the GLTF coord space (hundreds of units), but the
// TLAS scales them by 0.01 — so the path tracer sees Sponza at ±0.06.
// Apply the same scale to the raster ModelMatrix and place the camera at the
// matching scaled coordinates. Original-scale camera with the un-scaled
// model matrix produced no rasterized fragments (all verts clipped by far).
//
// v-framing (2026-08-01): measured gi_raw/worldpos ranges show the rasterized
// structure actually spans x[-15,15] y[-12,8] z[-14,0] (Sponza's original units
// are ~1500, scaled by the 0.01 model matrix). The old camera at (0,0.03,0.08)
// sat on the floor at the near edge and framed only ~6% of the frame (a thin
// floor band). Move the camera back (+z, outside z>0), up to mid-height, and
// look at the scene center so the full structure is framed. FOV 75°.
//
// 2026-08-10: horizon camera + rotating scene. The scene spins around Y
// (TLAS instances AND the raster ModelMatrix use the same per-frame angle
// `SceneRotationDeg`), while the camera stays fixed in WORLD space looking
// level at the horizon — so the sun (also world-fixed) lights different
// facades as Sponza turns.
static glm::vec3 EnvVec3(const char* Name, const glm::vec3& Fallback)
{
    const char* S = std::getenv(Name);
    if (S && *S)
    {
        float X = 0.0f, Y = 0.0f, Z = 0.0f;
        if (std::sscanf(S, "%f %f %f", &X, &Y, &Z) == 3)
            return glm::vec3(X, Y, Z);
    }
    return Fallback;
}
// Horizon framing: eye height ~2.5 m, perfectly level (target y == eye y).
// Override with HLVM_RGI_CAM_POS / HLVM_RGI_CAM_TARGET (world coords).
static glm::vec3 GetCameraPos()   { return EnvVec3("HLVM_RGI_CAM_POS",    glm::vec3( 0.0f, 2.5f, 18.0f)); }
static glm::vec3 GetCameraTarget(){ return EnvVec3("HLVM_RGI_CAM_TARGET", glm::vec3( 0.0f, 2.5f, -10.0f)); }
static glm::vec3 GetCameraUp()    { return glm::vec3( 0.0f, 1.0f,  0.0f); }
static float     GetCameraFovDeg(){ return 65.0f; }

// Per-mesh albedo for the multimodal structural judge. Sponza's real colors
// live in textures which this test does not load (flat white base color), so
// the picture would be uniformly gray and the CHROMATIC modality of the
// multimodal validator could never be judged. Assign a deterministic palette
// color keyed by mesh name when the loaded material carries no chromatic
// content; keep the material color when it does. Both the RT instance buffer
// and the GBuffer pass use this, so the GBuffer material texture stays the
// per-pixel ground truth for the validator.
static FVec3 GetMeshAlbedo(const FString& MeshName, const FVec3& MaterialAlbedo)
{
    // 2026-08-10 (material rework Phase 4): the palette-hash hack is gone.
    // For untextured meshes the gltf baseColorFactor is used as-is; a neutral
    // white factor (Sponza's textures carry the color, so the factor is 1,1,1)
    // falls back to a neutral 0.7 gray instead of an arbitrary per-name color.
    (void)MeshName;
    const float MinC = std::min(MaterialAlbedo.x, std::min(MaterialAlbedo.y, MaterialAlbedo.z));
    const float MaxC = std::max(MaterialAlbedo.x, std::max(MaterialAlbedo.y, MaterialAlbedo.z));
    if (MaxC - MinC > 0.05f)
        return MaterialAlbedo;
    return FVec3(0.7f, 0.7f, 0.7f);
}

// ============================================================================
// Helpers
// ============================================================================

static nvrhi::TextureHandle CreateTexture2D(
    nvrhi::IDevice* Device,
    uint32_t W, uint32_t H,
    nvrhi::Format Format,
    nvrhi::ResourceStates InitialState,
    const char* DebugName)
{
    nvrhi::TextureDesc Desc;
    Desc.dimension = nvrhi::TextureDimension::Texture2D;
    Desc.width = W;
    Desc.height = H;
    Desc.format = Format;
    Desc.initialState = InitialState;
    Desc.keepInitialState = true;
    Desc.debugName = DebugName;
    if (InitialState == nvrhi::ResourceStates::UnorderedAccess)
        Desc.isUAV = true;
    return Device->createTexture(Desc);
}

static std::vector<char> ReadBinaryFile(const std::string& Filename)
{
    std::ifstream file(Filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) return {};
    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(fileSize));
    return buffer;
}

static FString MakeShaderDataDir()
{
    // ShaderMake writes this test's blobs into the source test data directory.
    // Anchor the path to GProjectRoot so it is independent of the executable
    // location and the process working directory.
    return FString::Format(TXT("{}/Engine/Source/Runtime/Test/{}_Data"),
        *FString(GProjectRoot.string().c_str()), *GExecutableName);
}

// ============================================================================
// Pass
// ============================================================================

class FReSTIRGITemporalPass : public IRenderPass
{
public:
    using IRenderPass::IRenderPass;

    bool Initialize(nvrhi::IDevice* Device, nvrhi::IFramebuffer* Framebuffer, const FString& InWindowTitle)
    {
        NvrhiDevice = Device;
        BindingCache.SetDevice(NvrhiDevice);
        FBInfo = Framebuffer->getFramebufferInfo();
        WindowTitle = InWindowTitle;

        const auto DataDir = MakeShaderDataDir();
        HLVM_LOG(LogTest, info, TXT("TestReSTIR_GI_Temporal data dir: {}"), *DataDir);

        // Scene turntable setup (2026-08-10):
        //   HLVM_RGI_SCENE_YAW="deg"      -> fixed angle, no animation
        //   HLVM_RGI_SCENE_RPS="r/s"      -> rotation speed (default 0.03)
        //   HLVM_RGI_SUN_DIR="x y z"      -> world-space sun direction
        if (const char* Yaw = std::getenv("HLVM_RGI_SCENE_YAW"))
        {
            SceneRotationDeg = static_cast<float>(std::atof(Yaw));
            SceneRotationDegSpeed = 0.0f;
        }
        else
        {
        const float RPS = std::getenv("HLVM_RGI_SCENE_RPS")
            ? static_cast<float>(std::atof(std::getenv("HLVM_RGI_SCENE_RPS")))
            : 0.03f;
            SceneRotationDegSpeed = RPS * 360.0f;
        }
        // 2026-08-11: the render window is visible by default; HLVM_HIDE_WINDOW
        // opts out (headless). The keep-focus/raise behavior follows.
        bShowWindow = (std::getenv("HLVM_HIDE_WINDOW") == nullptr);
        HLVM_LOG(LogTest, info, TXT("Scene turntable: yaw={:.1f} speed={:.1f} deg/s"),
            SceneRotationDeg, SceneRotationDegSpeed);

        // Sampler
        {
            nvrhi::SamplerDesc Desc;
            Desc.setAddressU(nvrhi::SamplerAddressMode::Clamp)
                .setAddressV(nvrhi::SamplerAddressMode::Clamp)
                .setAddressW(nvrhi::SamplerAddressMode::Clamp)
                .setMinFilter(true).setMagFilter(true).setMipFilter(false);
            LinearSampler = NvrhiDevice->createSampler(Desc);
        }

        // 1x1 white placeholder albedo texture (2026-08-10 Phase 1): meshes
        // without a loaded albedo texture multiply the white placeholder by
        // their per-instance AlbedoColor (the old palette fallback).
        {
            nvrhi::TextureDesc Desc;
            Desc.dimension = nvrhi::TextureDimension::Texture2D;
            Desc.width = 1;
            Desc.height = 1;
            Desc.format = nvrhi::Format::RGBA8_UNORM;
            Desc.isRenderTarget = false;
            Desc.isUAV = false;
            Desc.initialState = nvrhi::ResourceStates::ShaderResource;
            Desc.keepInitialState = true;
            Desc.debugName = "GBufferPlaceholderTexture";
            PlaceholderTexture = NvrhiDevice->createTexture(Desc);
            nvrhi::CommandListHandle TexCmdList = NvrhiDevice->createCommandList();
            TexCmdList->open();
            const uint32_t WhitePixel = 0xFFFFFFFFu;
            TexCmdList->writeTexture(PlaceholderTexture, 0, 0, &WhitePixel, 4);
            TexCmdList->close();
            NvrhiDevice->executeCommandList(TexCmdList);
        }

        // Sponza GLTF scene (Samples/Assets/sponza/Sponza01.gltf)
        if (!LoadSponza())
        {
            HLVM_LOG(LogTest, err, TXT("Failed to load Sponza scene"));
            return false;
        }

        // =====================================================================
        // Phase 0 diagnostic (2026-08-10, material-system rework): per-mesh
        // material inventory — gltf baseColor/roughness/metallic factors,
        // albedo texture path, GPU texture presence, and the CURRENT palette
        // color the renderer substitutes (the "colored pillars" bug). Also
        // probes whether the Sponza .ktx albedo textures actually decode
        // through FAsyncTextureLoader.
        // =====================================================================
        {
            TVector<std::shared_ptr<FPBRMaterial>> Materials;
            HLVM_LOG(LogTest, info, TXT("Phase-0 material inventory:"));
            for (auto& Entry : Scene->MeshTree)
            {
                auto It = Scene->MeshMultiMaterialMap.find(Entry.second);
                if (It == Scene->MeshMultiMaterialMap.end() || It->second.empty())
                {
                    HLVM_LOG(LogTest, info, TXT("  mesh '{}': NO MATERIAL"), Entry.second->GetName());
                    continue;
                }
                auto M = std::dynamic_pointer_cast<FPBRMaterial>(It->second[0]);
                if (!M)
                {
                    HLVM_LOG(LogTest, info, TXT("  mesh '{}': material is not FPBRMaterial"), Entry.second->GetName());
                    continue;
                }

                const FVec3 Factor  = M->GetAlbedoColor();
                const FVec3 Palette = GetMeshAlbedo(Entry.second->GetName(), Factor);
                const FString TexPath = M->HasTexture(IMaterial::ETextureType::Albedo)
                    ? FString(M->GetTexturePath(IMaterial::ETextureType::Albedo).string().c_str())
                    : FString(TXT("(none)"));
                HLVM_LOG(LogTest, info,
                    TXT("  mesh '{}' mat '{}': baseColor=({:.2f},{:.2f},{:.2f}) rough={:.3f} metal={:.3f} albedoTex='{}' gpuTex={} | fallback=({:.2f},{:.2f},{:.2f})"),
                    Entry.second->GetName(), M->GetName(),
                    Factor.x, Factor.y, Factor.z,
                    M->GetRoughness(), M->GetMetallic(),
                    *TexPath, M->HasGPUTexture(IMaterial::ETextureType::Albedo) ? 1 : 0,
                    Palette.x, Palette.y, Palette.z);
                Materials.push_back(M);
            }

            // Phase-0 probe: can the Sponza .ktx albedo textures actually load?
            if (!Materials.empty())
            {
                const uint32_t Enqueued = FAsyncTextureLoader::LoadMaterialTexturesAsync(
                    NvrhiDevice, Materials, {IMaterial::ETextureType::Albedo});
                int Tries = 0;
                while (FAsyncTextureLoader::HasPendingLoads() && Tries++ < 300)
                {
                    FAsyncTextureLoader::Poll(NvrhiDevice);
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
                NvrhiDevice->waitForIdle();

                uint32_t Loaded = 0;
                for (auto& M : Materials)
                {
                    if (M->HasGPUTexture(IMaterial::ETextureType::Albedo))
                        ++Loaded;
                }
                HLVM_LOG(LogTest, info,
                    TXT("Phase-0 albedo load probe: enqueued={} loaded={}/{} (pending={})"),
                    Enqueued, Loaded, static_cast<uint32_t>(Materials.size()),
                    FAsyncTextureLoader::HasPendingLoads() ? 1 : 0);
            }

            // Phase 3 (2026-08-10): per-mesh AVERAGE albedo from the real
            // texture (last KTX2 mip, linearized) → RTInstanceInfo.AlbedoColor,
            // so closest-hit bounce shading uses the material's true color
            // (gray pillars bounce neutral, red bricks bounce warm) instead of
            // the palette hash. The GBuffer keeps full per-texel detail via the
            // texture; this average only feeds the ray-trace bounce side.
            {
                size_t InstanceIdx = 0;
                size_t Patched = 0;
                std::unordered_map<nvrhi::TextureHandle, uint32_t> TexToIndex;
                TVector<nvrhi::TextureHandle> UniqueTextures;
                for (auto& Entry : Scene->MeshTree)
                {
                    auto StaticMesh = std::dynamic_pointer_cast<FStaticMesh>(Entry.second);
                    if (!StaticMesh) continue;
                    if (StaticMesh->GetVertices().empty() || StaticMesh->GetIndices().empty())
                        continue;
                    if (InstanceIdx >= AllInstanceInfos.size())
                        break;

                    auto It = Scene->MeshMultiMaterialMap.find(Entry.second);
                    if (It != Scene->MeshMultiMaterialMap.end() && !It->second.empty())
                    {
                        auto PBRMat = std::dynamic_pointer_cast<FPBRMaterial>(It->second[0]);
                        if (PBRMat && PBRMat->HasTexture(IMaterial::ETextureType::Albedo))
                        {
                            const FPath TexPath = PBRMat->GetTexturePath(IMaterial::ETextureType::Albedo);
                            const FPath KTX2Path = TexPath.parent_path() / "ktx2" /
                                (TexPath.stem().string() + ".ktx2");
                            const FPath DecodePath = std::filesystem::exists(KTX2Path.string())
                                ? KTX2Path : TexPath;
                            const auto Decoded = FAsyncTextureLoader::DecodeKTXTexture(DecodePath);
                            if (Decoded.bIsValid && !Decoded.Mips.empty())
                            {
                                const uint32_t MipLevel = static_cast<uint32_t>(Decoded.Mips.size()) - 1;
                                const uint32_t MipW = std::max(1u, Decoded.Width >> MipLevel);
                                const uint32_t MipH = std::max(1u, Decoded.Height >> MipLevel);
                                const auto& LastMip = Decoded.Mips.back();
                                const uint8_t* Src = Decoded.PixelData.data() + LastMip.Offset;
                                double R = 0.0, G = 0.0, B = 0.0;
                                size_t N = 0;
                                for (uint32_t y = 0; y < MipH; ++y)
                                {
                                    for (uint32_t x = 0; x < MipW; ++x)
                                    {
                                        const uint8_t* P = Src + static_cast<size_t>(y) * LastMip.RowPitch + static_cast<size_t>(x) * 4;
                                        R += P[0] / 255.0; G += P[1] / 255.0; B += P[2] / 255.0;
                                        ++N;
                                    }
                                }
                                if (N > 0)
                                {
                                    R /= static_cast<double>(N); G /= static_cast<double>(N); B /= static_cast<double>(N);
                                    if (Decoded.Format == nvrhi::Format::SRGBA8_UNORM)
                                    {
                                        R = std::pow(R, 2.2); G = std::pow(G, 2.2); B = std::pow(B, 2.2);
                                    }
                                    AllInstanceInfos[InstanceIdx].AlbedoColor[0] = static_cast<float>(R);
                                    AllInstanceInfos[InstanceIdx].AlbedoColor[1] = static_cast<float>(G);
                                    AllInstanceInfos[InstanceIdx].AlbedoColor[2] = static_cast<float>(B);
                                    const FVec3 Palette = GetMeshAlbedo(Entry.second->GetName(), PBRMat->GetAlbedoColor());
                                    HLVM_LOG(LogTest, info,
                                        TXT("  avg-albedo '{}': linear=({:.3f},{:.3f},{:.3f}) (fallback=({:.2f},{:.2f},{:.2f}))"),
                                        Entry.second->GetName(),
                                        static_cast<float>(R), static_cast<float>(G), static_cast<float>(B),
                                        Palette.x, Palette.y, Palette.z);
                                    ++Patched;
                                }
                            }
                        }
                        // Phase 3b: assign the per-texel bounce texture index.
                        if (PBRMat && PBRMat->HasGPUTexture(IMaterial::ETextureType::Albedo))
                        {
                            const nvrhi::TextureHandle H = PBRMat->GetGPUTexture(IMaterial::ETextureType::Albedo).GetTextureHandle();
                            auto TexIt = TexToIndex.find(H);
                            if (TexIt == TexToIndex.end())
                            {
                                TexIt = TexToIndex.emplace(H, static_cast<uint32_t>(UniqueTextures.size())).first;
                                UniqueTextures.push_back(H);
                            }
                            AllInstanceInfos[InstanceIdx].AlbedoTextureIndex = TexIt->second;
                        }
                    }
                    ++InstanceIdx;
                }
                MaterialTextures = UniqueTextures;

                // Re-upload the patched instance info for the RT side.
                nvrhi::CommandListHandle Cmd = NvrhiDevice->createCommandList();
                Cmd->open();
                Cmd->writeBuffer(InstanceInfoBuffer, AllInstanceInfos.data(),
                    static_cast<uint32_t>(AllInstanceInfos.size() * sizeof(FInstanceInfo)));
                Cmd->close();
                NvrhiDevice->executeCommandList(Cmd);
                NvrhiDevice->waitForIdle();
                HLVM_LOG(LogTest, info,
                    TXT("Phase-3 average-albedo patch: {}/{} instances use real texture averages"),
                    static_cast<uint32_t>(Patched), static_cast<uint32_t>(AllInstanceInfos.size()));
                HLVM_LOG(LogTest, info,
                    TXT("Phase-3b per-texel bounce textures: {} unique textures bound (t9..t{})"),
                    static_cast<uint32_t>(MaterialTextures.size()),
                    9 + static_cast<uint32_t>(MaterialTextures.size()) - 1);
            }
        }

        // View constants buffer (b1)
        {
            nvrhi::BufferDesc Desc;
            Desc.byteSize = sizeof(glm::mat4) * 3 + sizeof(float) * 4;
            Desc.isConstantBuffer = true;
            Desc.initialState = nvrhi::ResourceStates::ConstantBuffer;
            Desc.keepInitialState = true;
            Desc.debugName = "ReSTIRGIViewConstants";
            ViewConstantsBuffer = NvrhiDevice->createBuffer(Desc);
        }

        // GBuffer textures
        if (!CreateGBufferTextures())
        {
            HLVM_LOG(LogTest, err, TXT("Failed to create GBuffer textures"));
            return false;
        }
        // Real Sponza GBuffer pass — replaces the previous hardcoded fill.
        // Uses GBufferPT_VS / GBufferPT_PS (3 MRTs: worldPos, normal, material)
        // and per-instance FInstanceInfo as a constant buffer to seed the
        // material color.
        if (!CreateGBufferPipeline(DataDir))
        {
            HLVM_LOG(LogTest, err, TXT("Failed to create GBuffer pipeline"));
            return false;
        }

        // GI pass
        if (!GIPass.Initialize(NvrhiDevice, DataDir, nullptr))
        {
            HLVM_LOG(LogTest, err, TXT("Failed to initialize FGIPass (dataDir={})"),
                *DataDir);
            return false;
        }
        // Bilateral denoise, ReSTIR, ReBLUR, GIAccumulate passes
        if (!BilateralDenoisePass.Initialize(NvrhiDevice, DataDir))
        {
            HLVM_LOG(LogTest, err, TXT("Failed to initialize BilateralDenoisePass"));
            return false;
        }
        if (!ReSTIRPass.Initialize(NvrhiDevice, DataDir))
        {
            HLVM_LOG(LogTest, err, TXT("Failed to initialize FReSTIRPass"));
            return false;
        }

        // ReBLUR denoiser (after the ReSTIR resolve). Disable with
        // HLVM_RGI_REBLUR=0. Pattern from TestCornellBoxGI.
        bReBLURInitialized = false;
        if (std::getenv("HLVM_RGI_REBLUR") == nullptr || std::string(std::getenv("HLVM_RGI_REBLUR")) != "0")
        {
            if (!ReBLURPass.Initialize(NvrhiDevice, DataDir))
            {
                HLVM_LOG(LogTest, warn, TXT("Failed to initialize ReBLURPass; continuing without denoiser"));
            }
            else
            {
                for (int i = 0; i < 2; ++i)
                {
                    nvrhi::TextureDesc Desc;
                    Desc.dimension = nvrhi::TextureDimension::Texture2D;
                    Desc.width = WIDTH;
                    Desc.height = HEIGHT;
                    Desc.format = nvrhi::Format::RGBA32_FLOAT;
                    Desc.isUAV = true;
                    Desc.initialState = nvrhi::ResourceStates::UnorderedAccess;
                    Desc.keepInitialState = true;
                    Desc.debugName = i == 0 ? "ReBLURHistory0" : "ReBLURHistory1";
                    ReBLURHistoryTexture[i] = NvrhiDevice->createTexture(Desc);
                }
                nvrhi::CommandListHandle ClearCmd = NvrhiDevice->createCommandList();
                ClearCmd->open();
                nvrhi::Color ClearColor(0.0f, 0.0f, 0.0f, 0.0f);
                ClearCmd->clearTextureFloat(ReBLURHistoryTexture[0], nvrhi::AllSubresources, ClearColor);
                ClearCmd->clearTextureFloat(ReBLURHistoryTexture[1], nvrhi::AllSubresources, ClearColor);
                ClearCmd->close();
                NvrhiDevice->executeCommandList(ClearCmd);
                bReBLURInitialized = true;
                HLVM_LOG(LogTest, info, TXT("ReBLUR denoiser initialized"));
            }
        }

        if (!CreateAccumulationPipeline(DataDir))
        {
            HLVM_LOG(LogTest, err, TXT("Failed to create accumulation pipeline"));
            return false;
        }
        if (!CreateResolvePipeline(DataDir))
        {
            HLVM_LOG(LogTest, err, TXT("Failed to create resolve pipeline"));
            return false;
        }

        // Frame count / exposure / debug controls
        AccumTargetFrames = DEFAULT_ACCUM_TARGET_FRAMES;
        if (const char* E = std::getenv("HLVM_RGI_ACCUM"))
        {
            try
            {
                int N = std::stoi(E); if (N > 0) AccumTargetFrames = static_cast<uint32_t>(N);
            } catch (...) {}
        }

        Exposure = 1.0f;
        if (const char* E = std::getenv("HLVM_RGI_EXPOSURE"))
        {
            try { float v = std::stof(E); if (v > 0.0f) Exposure = v; } catch (...) {}
        }

        bDumpRequested = (std::getenv("HLVM_DUMP_RGI") != nullptr);
        if (bDumpRequested)
            HLVM_LOG(LogTest, info, TXT("HLVM_DUMP_RGI=1: enabling frame dumps"));

        // Pipeline mode (2026-08-03): the real ReSTIR pipeline (Generate/
        // Temporal/Spatial, RealEngine-modeled reservoirs) is the DEFAULT.
        // HLVM_RGI_BYPASS=1 forces the raw gi_raw display path for A/B
        // debugging of the ReSTIR stages.
        bBypass = (std::getenv("HLVM_RGI_BYPASS") != nullptr);
        if (bBypass)
            HLVM_LOG(LogTest, info, TXT("HLVM_RGI_BYPASS=1: displaying gi_raw directly (ReSTIR skipped)"));
        else
            HLVM_LOG(LogTest, info, TXT("ReSTIR pipeline enabled (default)"));

        // v176: HLVM_RGI_MAXM env-var hook — override r_ReSTIR_MaxM at startup
        // (no rebuild needed). Inline shape (no new class member).
        if (const char* E = std::getenv("HLVM_RGI_MAXM"))
        {
            try
            {
                float v = std::stof(E);
                if (v > 0.0f)
                {
                    CVar_r_ReSTIR_MaxM.SetValue(v);
                    HLVM_LOG(LogTest, info, TXT("HLVM_RGI_MAXM override: r_ReSTIR_MaxM = {:.2f}"), v);
                }
            } catch (...) {}
        }

        // (HLVM-bypass: non-immediate pattern, like TestRTShadowsGBuffer.
        // The validation-layer's immediate-CL state machine silently
        // dropped setGraphicsState/drawIndexed GPU commands across frames.
        // Non-immediate CLs get explicit per-frame open/close/execute.)
        nvrhi::CommandListParameters CmdListParams;
        CmdListParams.enableImmediateExecution = false;
        CommandList = NvrhiDevice->createCommandList(CmdListParams);
        HLVM_LOG(LogTest, info, TXT("FReSTIRGITemporalPass initialized successfully"));
        return true;
    }

    void Shutdown()
    {
        BindingCache.Clear();
        GIPass.Shutdown();
        BilateralDenoisePass.Shutdown();
        ReSTIRPass.Shutdown();
        if (bReBLURInitialized)
        {
            ReBLURPass.Shutdown();
            bReBLURInitialized = false;
        }
        ReBLURHistoryTexture[0] = nullptr;
        ReBLURHistoryTexture[1] = nullptr;

        LinearSampler           = nullptr;
        ViewConstantsBuffer     = nullptr;
        GBufferWorldPos         = nullptr;
        GBufferNormal           = nullptr;
        GBufferMaterial         = nullptr;
        GBufferDepth            = nullptr;
        PrevGBufferWorldPos     = nullptr;
        PrevGBufferNormal       = nullptr;
        PrevGBufferMaterial     = nullptr;
        PrevLinearDepth         = nullptr;
        SunLightBuffer          = nullptr;
        PlaceholderTexture      = nullptr;
        AllInstanceInfos.clear();
        MaterialTextures.clear();
        LinearDepthTexture      = nullptr;
        VertexBuffer            = nullptr;
        IndexBuffer             = nullptr;
        InstanceInfoBuffer      = nullptr;
        SceneTLAS               = nullptr;
        SceneBLASes.clear();

        OutputTexture           = nullptr;
        DirectionTexture        = nullptr;
        SampleInfoTexture       = nullptr;
        DirectTexture           = nullptr;
        DenoisedTexture         = nullptr;        ReservoirTex0           = nullptr;
        ReservoirTex1           = nullptr;
        ReservoirTex2           = nullptr;
        TemporalReservoir0      = nullptr;
        TemporalReservoir1      = nullptr;
        TemporalReservoir2      = nullptr;
        TemporalReservoir3      = nullptr;
        TemporalReservoir4      = nullptr;
        TemporalReservoir5      = nullptr;
        SpatialRadiance         = nullptr;
        AccumTexture            = nullptr;
        DisplayTexture          = nullptr;

        AccumulatePipeline      = nullptr;
        AccumulateBindingLayout = nullptr;
        AccumulateCS            = nullptr;
        AccumulateConstants     = nullptr;
        CommandList             = nullptr;
        // v212: release per-mesh GBuffer binding sets BEFORE device teardown
        // (a static cache here tripped VUID-vkDestroyDevice-device-05137).
        GBufferDrawCache.clear();

        // GBuffer PT pass resources
        GBufferVS               = nullptr;
        GBufferPS               = nullptr;
        GBufferInputLayout      = nullptr;
        GBufferBindingLayout    = nullptr;
        GBufferPipeline         = nullptr;
        GBufferFrameBuffer      = nullptr;
        GBufferVertexBuffer     = nullptr;
        GBufferIndexBuffer      = nullptr;
        GBufferPerInstanceCB    = nullptr;
        Scene.reset();
    }

    virtual void Animate(float dt) override
    {
        ++FrameCount;
        // Scene turntable: advance the Y-rotation (degrees per second).
        PrevSceneRotationDeg = SceneRotationDeg;
        SceneRotationDeg += SceneRotationDegSpeed * dt;
        // Visible-window mode: periodically re-focus/raise the window so the
        // compositor keeps it above other windows (e.g. the IDE) — the swapchain
        // presents correctly, but a covered window looks like "nothing shows".
        if (bShowWindow && (FrameCount % 60 == 0))
        {
            if (auto* DM = GetDeviceManager())
            {
                if (GLFWwindow* W = static_cast<GLFWwindow*>(DM->GetGLFWWindow()))
                {
                    glfwRestoreWindow(W);
                    glfwFocusWindow(W);
                }
            }
        }
        FPSUpdateTimer += dt;
        if (FPSUpdateTimer >= 1.0f)
        {
            float fps = float(FrameCount) / FPSUpdateTimer;
            WindowTitle = FString::Format(
                TXT("ReSTIR GI Temporal - {:.1f} FPS | Accum {}/{}"),
                fps, AccumFrameCount, AccumTargetFrames);
            if (auto* DM = GetDeviceManager()) DM->SetWindowTitle(WindowTitle);
            // v211: frame-time gate (HLVM_RGI_LOG_FRAMETIME=1).
            if (std::getenv("HLVM_RGI_LOG_FRAMETIME"))
            {
                HLVM_LOG(LogTest, info, TXT("frame time: {:.2f} ms/frame ({:.1f} fps)"),
                    fps > 0.0f ? 1000.0f / fps : 0.0f, fps);
            }
            FPSUpdateTimer = 0.0f;
            FrameCount = 0;
        }
    }

    virtual void Render(nvrhi::IFramebuffer* Framebuffer) override
    {
        if (!NvrhiDevice || !Framebuffer) return;
        // v212: per-phase CPU timing (HLVM_RGI_LOG_FRAMETIME=1) for the
        // real-time pass.
        const bool bProfile = (std::getenv("HLVM_RGI_LOG_FRAMETIME") != nullptr);
        auto T0 = std::chrono::steady_clock::now();
        // v213 (Phase 5b): poll LAST frame's GPU timers (results land after the
        // previous submission executes).
        if (bGpuTimers && bTimersPending)
        {
            bool AllReady = true;
            double Times[6] = {0, 0, 0, 0, 0, 0};
            for (int i = 0; i < 6; ++i)
            {
                if (!GpuTimers[i]) { AllReady = false; break; }
                if (NvrhiDevice->pollTimerQuery(GpuTimers[i]))
                    Times[i] = static_cast<double>(NvrhiDevice->getTimerQueryTime(GpuTimers[i])) * 1000.0;
                else
                    AllReady = false;
            }
            if (AllReady)
            {
                bTimersPending = false;
                for (int i = 0; i < 6; ++i)
                    GpuTimerSums[i].push_back(Times[i]);
                const size_t N = GpuTimerSums[0].size();
                if (N % 8 == 0)
                {
                    auto Avg8 = [](const std::vector<double>& V) {
                        return std::accumulate(V.end() - 8, V.end(), 0.0) / 8.0;
                    };
                    HLVM_LOG(LogTest, info,
                        TXT("gpu ms (avg 8): GBuffer={:.2f} RayTrace={:.2f} ReSTIR={:.2f} Resolve={:.2f} BlitAccum={:.2f} total={:.2f}"),
                        Avg8(GpuTimerSums[0]), Avg8(GpuTimerSums[1]), Avg8(GpuTimerSums[2]),
                        Avg8(GpuTimerSums[3]), Avg8(GpuTimerSums[4]), Avg8(GpuTimerSums[5]));
                }
            }
        }
        const auto& FB = Framebuffer->getFramebufferInfo();
        // Remember the swapchain back-buffer for the readback diagnostic.
        CurrentBackBufferTexture = (Framebuffer->getDesc().colorAttachments.size() > 0)
            ? Framebuffer->getDesc().colorAttachments[0].texture
            : nullptr;
        if (FB.width != LastWidth || FB.height != LastHeight)
        {
            LastWidth = FB.width;
            LastHeight = FB.height;
            BindingCache.Clear();
        }

        // UpdateViewConstants needs the command list open, so open CommandList first.
        CommandList->open();
        if (bGpuTimers && GpuTimers[5]) CommandList->beginTimerQuery(GpuTimers[5]);

        // v195: WIDTH/HEIGHT, NOT FB.width/FB.height. These two arguments reach
        // THREE consumers, only one of which is the camera aspect ratio:
        //   (a) aspect  — glm::perspective(fov, float(W)/float(H), ...)
        //   (b) VC.Size — marshalled into ViewConstantsBuffer, read by the RT
        //                 shader as g_View.RenderTargetSize
        //   (c) the raster viewport, via LastWidth/LastHeight
        // (b) is load-bearing. GIPathTracing.hlsl forms the Phase-D scale as
        // RenderTargetSize / DispatchRaysDimensions(); the denominator is
        // HalfResWidth, assigned W/2 in CreateGBufferTextures where W == WIDTH,
        // so it is FIXED at 400 regardless of window size. A swapchain-derived
        // numerator therefore corrupts gbScale for every ray: widening drives
        // gbPixel past the fixed GBuffer, every Load returns 0, and those pixels
        // fall into the no-geometry early-out and are shaded as SKY — silently,
        // and plausibly enough to survive a glance. Narrowing samples only the
        // left/top sub-rect. The GBuffer MRTs are never recreated on resize
        // (BackBufferResizing only clears the binding cache), so WIDTH/HEIGHT is
        // the true extent. On (a): the blit presents with an unconditional
        // stretch — BlitVS emits a fixed fullscreen NDC quad, BlitPS samples it
        // with no letterbox term — so the correct upstream aspect is the render
        // target's; taking it from the window would apply that aspect twice.
        UpdateViewConstants(WIDTH, HEIGHT);

        // (0) GBuffer raster pass — renders Sponza into GBufferWorldPos /
        //     GBufferNormal / GBufferMaterial MRTs. Must run BEFORE FGIPass
        //     so the RT shader sees populated GBuffer inputs.
        // RenderGBuffer leaves the CommandList OPEN (no mid-frame submit;
        // the v1-introduced HLVM-bypass close+execute+waitForIdle+open
        // block was the regression — see NOTE comment near line 1531).
        // The whole frame submits at end of Render via line 691.
        // v197: no arguments — the raster extent is a property of the target,
        // not of the caller. See the note on RenderGBuffer's definition.
        if (bGpuTimers && GpuTimers[0]) CommandList->beginTimerQuery(GpuTimers[0]);
        RenderGBuffer();
        if (bGpuTimers && GpuTimers[0]) CommandList->endTimerQuery(GpuTimers[0]);
        auto T1 = std::chrono::steady_clock::now();

        // Turntable: rebuild the TLAS with the current scene Y-rotation so the
        // ray tracer and the raster pass always agree (2026-08-10).
        if (bGpuTimers && GpuTimers[1]) CommandList->beginTimerQuery(GpuTimers[1]);
        BuildTLAS(CommandList);

        // (1) GI ray trace — produces OutputTexture (HDR rgb + hitDist alpha)
        {
            GI::FGIPassDesc Desc{};
            Desc.GBufferWorldPos   = GBufferWorldPos;
            Desc.GBufferNormal     = GBufferNormal;
            Desc.GBufferMaterial   = GBufferMaterial;
            Desc.LinearSampler     = LinearSampler;
            Desc.ViewConstants     = ViewConstantsBuffer;
            Desc.SceneTLAS         = SceneTLAS;
            Desc.OutputTexture     = OutputTexture;
            Desc.OutputDirection   = DirectionTexture;
            // v210 (ZetaRay ground-truth port): candidate sample state.
            Desc.SampleInfoTexture = SampleInfoTexture;
            Desc.DirectTexture     = DirectTexture;
            Desc.RTVertices        = VertexBuffer;
            Desc.RTIndices         = IndexBuffer;
            Desc.RTInstanceInfo    = InstanceInfoBuffer;
            Desc.OutputWidth       = HalfResWidth;    // Phase D: half-res trace
            Desc.OutputHeight      = HalfResHeight;
            // ReSTIR-GI sampling: 1 primary ray/pixel. Bounce count tunable
            // (HLVM_RGI_BOUNCES); 1 = the RealEngine sampling model, 4 = more
            // indirect depth. Temporal/spatial reuse + ReBLUR recover quality.
            Desc.MaxBounces        = static_cast<uint32_t>(std::max(
                1, std::getenv("HLVM_RGI_BOUNCES") ? std::atoi(std::getenv("HLVM_RGI_BOUNCES")) : 4));
            // Phase A (PLAN_REALTIME_RESTIR_GAP): ReSTIR-native sampling —
            // exactly ONE primary GI ray per pixel; the reservoir IS that
            // sample (Generate copies it with M=1/W=1 + the direction is
            // written to u2). Reuse (temporal/spatial) + ReBLUR replace the
            // old multi-sample loop.
            Desc.SamplesPerPixel   = 1;
            Desc.MinRayLength      = 0.001f;
            Desc.EnableRR          = true;
            // v212: NEE toggle (HLVM_RGI_NEE=0) — isolates the shadow-ray cost
            // in the tracer for the real-time budget pass.
            if (const char* Nee = std::getenv("HLVM_RGI_NEE"))
                CVar_r_GI_EnableNEE.SetValue(std::atoi(Nee) != 0);
            Desc.FrameIndex        = AccumFrameCount;
            // Lighting setup (2026-08-10): sunlight interior. Pass the sun-only
            // light buffer (intensity 8.0 directional) so NEE is sunlight, and
            // keep the flat ambient low (~0.35, sky-tinted) so the sun creates
            // visible shadows while sky GI (path-traced miss) fills the dark
            // interior. The earlier v142 setup used AmbientScale=1.5 + interior
            // point lights, which washed out the scene.
            Desc.LightsBuffer      = SunLightBuffer;
            Desc.LightCount        = 1;
            Desc.MaterialTextures  = MaterialTextures;   // Phase 3b per-texel bounce albedo
            Desc.AmbientScale      = 0.35f;
            Desc.AmbientColor[0]   = 0.75f;
            Desc.AmbientColor[1]   = 0.8f;
            Desc.AmbientColor[2]   = 1.0f;
            Desc.AmbientColor[3]   = 0.0f;

            // DIAGNOSTIC (v3 — six-role-pipeline): pre/post GIPass logs to
            // confirm the dispatch call is reached and returns.
            HLVM_LOG(LogTest, info, TXT("Pre-GIPass: CommandList=0x{:x} OutputTex=0x{:x} Frame={}"),
                reinterpret_cast<uintptr_t>(CommandList.Get()),
                reinterpret_cast<uintptr_t>(Desc.OutputTexture.Get()),
                AccumFrameCount);

            GIPass.DispatchRays(CommandList, Desc);

            HLVM_LOG(LogTest, info, TXT("Post-GIPass: returned Frame={}"),
                AccumFrameCount);
        }
        if (bGpuTimers && GpuTimers[1]) CommandList->endTimerQuery(GpuTimers[1]);
        auto T2 = std::chrono::steady_clock::now();

        // (2) Bilateral denoise — VESTIGIAL. Output dead; retained on a false
        // premise. The old comment claimed this dispatch is what forces nvrhi
        // to flush pending layout transitions before the ReSTIR binding sets
        // are created (VUID-VkDescriptorImageInfo-imageLayout-00344). Wrong:
        // in vulkan-compute.cpp, CommandList::setComputeState ends by calling
        // commitBarriers(), while CommandList::dispatch only does
        // updateComputeVolatileBuffers() + cmdBuf.dispatch(). The flush is
        // setComputeState's, and every consuming pass issues its own —
        // FReSTIRPass::DispatchGeneration flushes before DispatchTemporal
        // builds its sets. Nothing here is load-bearing for that ordering.
        // (The explicit commitBarriers() before ReBLURPass::Dispatch guards a
        // different hazard — descriptors bound before barriers land *within*
        // one setComputeState — and is not a precedent for this block.)
        // Kept only because deleting it is gated on absence-evidence (no VUID
        // in a real run), which source cannot supply. See PENDING_PLAN_v190.
        // Also incoherent: half-res input, full-res guides and OutputTexture,
        // so it filters one quadrant. Harmless solely because ReBLUR overwrites
        // DenoisedTexture in 5.5 and no gate reads the "denoised" dump.
        // Do not cite this pass as correct.
        if (!bBypass)
        {
            FBilateralDenoisePass::FDesc Bd{};
            Bd.InputTexture    = OutputTexture;
            Bd.DepthTexture    = LinearDepthTexture;
            Bd.NormalTexture   = GBufferNormal;
            Bd.OutputTexture   = DenoisedTexture;
            // v189: match the HALF-res InputTexture above. This was FB.width/
            // height, but FBilateralDenoisePass derives BOTH its dispatch grid
            // and TexelSize from these, and the shader recovers its bounds by
            // inverting TexelSize — so ~3/4 of threads cleared the early-out
            // and then Loaded the half-res input out of bounds (returning 0).
            // Grid-independence of the barrier flush: see the block comment
            // above. Scope limits of this fix: likewise above.
            Bd.OutputWidth     = HalfResWidth;
            Bd.OutputHeight    = HalfResHeight;
            Bd.DepthSigma      = 0.05f;
            Bd.NormalSigma     = 0.5f;
            Bd.SpatialSigma    = 4.0f;
            BilateralDenoisePass.Dispatch(CommandList, Bd);
        }

        // (3) ReSTIR Generate — skipped in HLVM_RGI_BYPASS mode
        auto T3 = T2, T4 = T2, T5 = T2;
        if (bGpuTimers && GpuTimers[2]) CommandList->beginTimerQuery(GpuTimers[2]);
        if (!bBypass)
        {
            ReSTIR::FReSTIRPass::FGenerationDesc Gd{};
            Gd.RadianceTexture  = OutputTexture;      // gi_raw (radiance.rgb + hitT.a)
            Gd.DirectionTexture = DirectionTexture;   // Phase B: primary ray direction
            Gd.SampleInfoTexture = SampleInfoTexture; // v210: x2 normal + pdf
            Gd.MaterialTexture  = GBufferMaterial;    // v210: albedo for f
            Gd.WorldPosTexture  = GBufferWorldPos;
            Gd.NormalTexture   = GBufferNormal;
            Gd.DepthTexture    = LinearDepthTexture;
            Gd.OutReservoir0   = ReservoirTex0;
            Gd.OutReservoir1   = ReservoirTex1;
            Gd.OutReservoir2   = ReservoirTex2;
            Gd.OutputWidth     = HalfResWidth;    // Phase D
            Gd.OutputHeight    = HalfResHeight;

            ReSTIR::FReSTIRConstants C{};
            // v185: this pass dispatches at HALF res (Gd.OutputWidth above, and
            // FReSTIRPass.cpp:393 derives the grid from it), so OutputSize must
            // describe the half-res grid the shader is actually running on.
            // Matches the spatial block below. Inert today (the generation
            // shader only uses OutputSize for its early-out, which no half-res
            // thread can trip against a full-res bound) but a live trap for the
            // next field added — same latent shape v184 was bitten by.
            C.OutputSize[0]      = float(HalfResWidth);
            C.OutputSize[1]      = float(HalfResHeight);
            C.RcpOutputSize[0]   = 1.0f / float(HalfResWidth);
            C.RcpOutputSize[1]   = 1.0f / float(HalfResHeight);
            C.FrameIndex         = float(AccumFrameCount);
            C.NumCandidates      = 8.0f;
            C.DepthThreshold     = 0.05f;
            C.NormalThreshold    = 0.5f;
            C.DebugVis           = 0.0f;

            ReSTIRPass.DispatchGeneration(CommandList, Gd, C);
            T3 = std::chrono::steady_clock::now();
        }

        // (4) ReSTIR Temporal (skip first frame — no history) — skipped in bypass
        if (!bBypass)
        {
            // The temporal pass reads ReservoirTex0/1 as SRV (history merge)
            // and writes TemporalReservoir0/1 as UAV. ReSTIR Generate wrote
            // them in the previous step in UnorderedAccess state. Transition
            // them to ShaderResource here so the SRV reads inside the shader
            // get SHADER_READ_ONLY_OPTIMAL — otherwise the Vulkan validation
            // layer flags GENERAL vs SHADER_READ_ONLY_OPTIMAL mismatch.
            //
            // bug-075 (six-role-pipeline v1) is fixed by splitting the
            // temporal binding layout into SRV-only (set 0) + UAV-only (set 1)
            // and dispatching in two phases. The shader was edited to declare
            // its UAVs at register(u0/u1, space1) so SPIR-V places them in
            // set 1, matching the split layout. Each dispatch binds only ONE
            // set, so nvrhi's requireTextureState infers only one state per
            // dispatch and the descriptor-bind layout is unambiguous.
            CommandList->setTextureState(
                ReservoirTex0, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
            CommandList->setTextureState(
                ReservoirTex1, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
            CommandList->setTextureState(
                ReservoirTex2, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
            // On frame > 0 the prev-frame Temporal pass wrote the previous
            // output pair in UnorderedAccess. The temporal pass reads one
            // PAIR as SRV history and writes the OTHER PAIR as UAV output.
            // Transition ALL FOUR to ShaderResource; nvrhi auto-transitions
            // the UAV-written pair back to GENERAL inside the dispatch.
            CommandList->setTextureState(
                TemporalReservoir0, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
            CommandList->setTextureState(
                TemporalReservoir1, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
            CommandList->setTextureState(
                TemporalReservoir2, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
            CommandList->setTextureState(
                TemporalReservoir3, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
            CommandList->setTextureState(
                TemporalReservoir4, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
            CommandList->setTextureState(
                TemporalReservoir5, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
            // v210: true prev-frame GBuffer (written by CopyGBufferToPrev).
            CommandList->setTextureState(
                PrevGBufferWorldPos, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
            CommandList->setTextureState(
                PrevGBufferNormal, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
            CommandList->setTextureState(
                PrevGBufferMaterial, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
            CommandList->setTextureState(
                PrevLinearDepth, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
            // Also keep depth SRV in SHADER_READ_ONLY (they remain untouched
            // until temporal writes them).
            CommandList->setTextureState(
                LinearDepthTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);

            ReSTIR::FReSTIRPass::FTemporalDesc Td{};
            Td.CurrentReservoir0 = ReservoirTex0;
            Td.CurrentReservoir1 = ReservoirTex1;
            Td.CurrentReservoir2 = ReservoirTex2;
            // Ping-pong History and Output across TWO texture PAIRS
            // (0/1 <-> 2/3). The old two-texture ping-pong aliased a history
            // SRV with an output UAV on the same texture every frame — a
            // Vulkan layout conflict (SRV needs SHADER_READ_ONLY, UAV needs
            // GENERAL) that the validation layer reports as
            // VUID-VkDescriptorImageInfo-imageLayout-00344 at the temporal
            // dispatch. With four textures the history pair and output pair
            // never overlap. On even frames History = pair 0/1 and Output =
            // pair 2/3; on odd frames they swap.
            const bool bPing = (AccumFrameCount % 2) == 0;
            Td.HistoryReservoir0 = (AccumFrameCount == 0) ? ReservoirTex0 : (bPing ? TemporalReservoir0 : TemporalReservoir2);
            Td.HistoryReservoir1 = (AccumFrameCount == 0) ? ReservoirTex1 : (bPing ? TemporalReservoir1 : TemporalReservoir3);
            Td.HistoryReservoir2 = (AccumFrameCount == 0) ? ReservoirTex2 : (bPing ? TemporalReservoir4 : TemporalReservoir5);
            Td.OutReservoir0     =                                            (bPing ? TemporalReservoir2 : TemporalReservoir0);
            Td.OutReservoir1     =                                            (bPing ? TemporalReservoir3 : TemporalReservoir1);
            Td.OutReservoir2     =                                            (bPing ? TemporalReservoir5 : TemporalReservoir4);
            Td.CurrentRadiance   = OutputTexture;       // gi_raw (radiance source)
            Td.HistoryRadiance   = OutputTexture;       // no separate history radiance texture
            Td.DepthTexture      = LinearDepthTexture;
            Td.NormalTexture     = GBufferNormal;
            // v210: true previous-frame GBuffer (was same-frame aliases).
            Td.PrevDepthTexture  = PrevLinearDepth;
            Td.PrevNormalTexture = PrevGBufferNormal;
            Td.PrevWorldPosTexture = PrevGBufferWorldPos;
            Td.PrevMaterialTexture = PrevGBufferMaterial;
            // v211 (Phase 4): segment-visibility TLAS in reuse.
            Td.SceneTLAS = SceneTLAS;
            // v210: full-res primary surface for target/BSDF evaluation.
            Td.WorldPosTexture   = GBufferWorldPos;
            Td.MaterialTexture   = GBufferMaterial;
            Td.OutRadiance       = SpatialRadiance;       // output radiance directly
            Td.OutputWidth       = HalfResWidth;    // Phase D
            Td.OutputHeight      = HalfResHeight;

            ReSTIR::FReSTIRTemporalConstants TC{};
            // Real reprojection matrices: inverse current view-proj and the
            // previous frame's view-proj. GLM is column-major, matching the
            // HLSL mul(matrix, vector) convention in ReSTIR_Temporal_cs.hlsl.
            glm::mat4 InvCurr = glm::inverse(CurrViewProj);
            memcpy(TC.InverseCurrViewProj, glm::value_ptr(InvCurr), sizeof(TC.InverseCurrViewProj));
            memcpy(TC.PrevViewProj, glm::value_ptr(PrevViewProj), sizeof(TC.PrevViewProj));
            // v185: this pass dispatches at HALF res (Td.OutputWidth above),
            // but these were still filled from the FULL-res framebuffer. The
            // shader uses them for ALL its screen-space arithmetic, so at
            // 800x600/400x300 every derived quantity was off by 2x:
            //   :136 uv = (pixel + 0.5) * RcpOutputSize -> spans only [0,0.5],
            //        so reprojection was computed in the top-left quadrant of
            //        NDC instead of the full frame;
            //   :170 prevPixel = prevUV * OutputSize -> up to 800, while the
            //        history reservoirs are 400x300, and the :176 bounds test
            //        validates against this SAME wrong constant so it does not
            //        catch it. Out-of-range Loads return 0 -> prevDepth/normal
            //        zero -> history rejected. The likeliest cause of the
            //        long-standing `M mean=2.93` against MaxM=30.
            // The spatial block below was already correct; these two call sites
            // were simply missed when Phase D landed.
            TC.OutputSize[0]    = float(HalfResWidth);
            TC.OutputSize[1]    = float(HalfResHeight);
            TC.RcpOutputSize[0] = 1.0f / float(HalfResWidth);
            TC.RcpOutputSize[1] = 1.0f / float(HalfResHeight);
            TC.FrameIndex       = float(AccumFrameCount);
            TC.MaxM             = CVar_r_ReSTIR_MaxM.GetValue();   // v176: wire CVar (default 30.0f; tune via HLVM_RGI_MAXM)
            TC.DepthThreshold   = 0.05f;
            TC.NormalThreshold  = 0.5f;
            TC.DebugVis         = 0.0f;
            // Phase C: object-aware temporal reprojection (turntable).
            TC.SceneYaw         = SceneRotationDeg;
            TC.PrevSceneYaw     = PrevSceneRotationDeg;
            // Near/far planes (must match UpdateViewConstants perspective).
            // The temporal shader reconstructs the exact NDC z from the
            // linear view-space depth using these.
            TC.NearPlane        = 0.001f;
            TC.FarPlane         = 50.0f;
            // v183: this pass dispatches at half res but DepthTexture /
            // NormalTexture / PrevDepthTexture / PrevNormalTexture are full-res
            // GBuffer MRTs. The shader scales the dispatch pixel by this ratio
            // before sampling them; without it the depth/normal validation
            // compares the top-left quadrant and rejects nearly all history.
            //
            // v191: the numerator is WIDTH, NOT FB.width. INVARIANT: the GBuffer
            // MRTs are fixed-size and independent of the swapchain — they are
            // created once inside CreateGBufferTextures from `const uint32_t W =
            // WIDTH` and are never recreated on resize (BackBufferResizing clears
            // only the binding cache). HalfResWidth is W/2 off that same constant.
            // Both operands of this ratio are therefore fixed, and FB.width was a
            // variable third quantity that merely coincides with the numerator at
            // startup. On a resize the uint division truncates silently: a 600-wide
            // swapchain yields 1, which `max(int(s),1)` in the shader's GB() helper
            // turns into the identity map — undoing v183 exactly the way v184's
            // GBufferScale==0 did — and a 1200-wide one yields 3, indexing the
            // 800-wide GBuffer out of bounds so every Load returns 0 and history is
            // rejected. Both are silent: no VUID, no error, just a collapsed M.
            // If the GBuffer is ever made to follow the swapchain, this must change
            // with it.
            TC.GBufferScale     = static_cast<float>(WIDTH / std::max(HalfResWidth, 1u));

            ReSTIRPass.DispatchTemporal(CommandList, Td, TC);
            T4 = std::chrono::steady_clock::now();
        }

        // (5) ReSTIR Spatial (using merged reservoirs) — skipped in bypass
        if (!bBypass)
        {
            // Temporal wrote the output pair in UnorderedAccess state.
            // Transition them to ShaderResource so Spatial's SRV reads don't
            // hit the GENERAL vs SHADER_READ_ONLY_OPTIMAL validation error.
            CommandList->setTextureState(
                TemporalReservoir0, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
            CommandList->setTextureState(
                TemporalReservoir1, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
            CommandList->setTextureState(
                TemporalReservoir2, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
            CommandList->setTextureState(
                TemporalReservoir3, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
            CommandList->setTextureState(
                TemporalReservoir4, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
            CommandList->setTextureState(
                TemporalReservoir5, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
            CommandList->setTextureState(
                DenoisedTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);

            ReSTIR::FReSTIRPass::FSpatialDesc Sd{};
            Sd.RadianceTexture  = OutputTexture;        // gi_raw (unused in v1 reservoir math)
            // The temporal pass ping-pongs its reservoir output pair by frame
            // parity: on even AccumFrameCount the sample radiance lands in the
            // 2/3 pair (M/W in 3/2); on odd frames it lands in the 0/1 pair.
            // The spatial pass must read the pair with the SAME parity.
            const bool bSpatialPing = (AccumFrameCount % 2) == 0;
            Sd.Reservoir0       = bSpatialPing ? TemporalReservoir2 : TemporalReservoir0;
            Sd.Reservoir1       = bSpatialPing ? TemporalReservoir3 : TemporalReservoir1;
            Sd.Reservoir2       = bSpatialPing ? TemporalReservoir5 : TemporalReservoir4;
            // v210: full-res primary surface for target/BSDF evaluation.
            Sd.WorldPosTexture  = GBufferWorldPos;
            Sd.MaterialTexture  = GBufferMaterial;
            Sd.SceneTLAS        = SceneTLAS;
            Sd.NormalTexture    = GBufferNormal;
            Sd.DepthTexture     = LinearDepthTexture;
            Sd.OutRadiance      = SpatialRadiance;
            Sd.OutputWidth      = HalfResWidth;    // Phase D
            Sd.OutputHeight     = HalfResHeight;

            ReSTIR::FReSTIRSpatialConstants SC{};
            SC.OutputSize[0]    = float(HalfResWidth);   // Phase D
            SC.OutputSize[1]    = float(HalfResHeight);
            SC.RcpOutputSize[0] = 1.0f / float(HalfResWidth);
            SC.RcpOutputSize[1] = 1.0f / float(HalfResHeight);
            SC.NormalThreshold  = 0.9f;
            SC.DepthThreshold   = 0.05f;
            // v183: same half-res dispatch / full-res GBuffer ratio as the
            // temporal pass (NormalTexture and DepthTexture are full-res MRTs).
            // v191: WIDTH, not FB.width — same invariant and same silent-failure
            // analysis as the temporal site above (search: "v191: the numerator").
            SC.GBufferScale     = static_cast<float>(WIDTH / std::max(HalfResWidth, 1u));
            SC.MaxM             = CVar_r_ReSTIR_MaxM.GetValue();   // v176: wire CVar (default 30.0f; tune via HLVM_RGI_MAXM)
            SC.SpatialRadius    = 3.0f;
            SC.DebugVis         = 0.0f;

            ReSTIRPass.DispatchSpatial(CommandList, Sd, SC);
            T5 = std::chrono::steady_clock::now();
        }
        if (bGpuTimers && GpuTimers[2]) CommandList->endTimerQuery(GpuTimers[2]);

        // (5.4) Phase D: depth/normal-weighted half-res → full-res resolve.
        if (bGpuTimers && GpuTimers[3]) CommandList->beginTimerQuery(GpuTimers[3]);
        {
            struct FResolveC
            {
                float HalfW, HalfH;
                float RcpHalfW, RcpHalfH;
                float RcpFullW, RcpFullH;
                float DepthSigma, NormalSigma;
            };
            FResolveC RC{};
            RC.HalfW = static_cast<float>(HalfResWidth);
            RC.HalfH = static_cast<float>(HalfResHeight);
            RC.RcpHalfW = 1.0f / static_cast<float>(HalfResWidth);
            RC.RcpHalfH = 1.0f / static_cast<float>(HalfResHeight);
            // v192: the resolve pass is FIXED-EXTENT end to end. Its outputs
            // (FullResGIRaw / FullResSpatial), its guides (LinearDepthTexture /
            // GBufferNormal) and its input (OutputTexture) are all created inside
            // CreateGBufferTextures from WIDTH/HEIGHT and are never recreated on
            // resize. These were FB.width/FB.height — the one swapchain-derived
            // quantity in a pass where nothing else is swapchain-derived. Widening
            // the window launched threads past the end of an 800x600 output, and
            // the kernel has NO bounds test on tid.xy (its only early-out is the
            // depth check), so that was an out-of-bounds UAV store — undefined
            // behaviour, not the harmless zero an out-of-range Load returns.
            // Narrowing smeared the input's last column across the overhang (the
            // shader's own clamp) and left the remaining columns unwritten.
            // NOTE: Resolve_cs.hlsl's `int2 fp = hp * 2 + 1` is deliberately NOT
            // parameterised. It encodes the fixed half-res-to-full-res footprint
            // relationship between OutputTexture and the GBuffer MRTs, which have
            // a common origin in WIDTH/HEIGHT — it is not a swapchain ratio, and a
            // constant for it could only ever hold 2.
            RC.RcpFullW = 1.0f / static_cast<float>(WIDTH);
            RC.RcpFullH = 1.0f / static_cast<float>(HEIGHT);
            RC.DepthSigma = 8.0f;
            RC.NormalSigma = 32.0f;
            CommandList->writeBuffer(ResolveConstantsBuffer, &RC, sizeof(RC));

            auto DispatchResolve = [&](nvrhi::TextureHandle HalfTex, nvrhi::TextureHandle OutTex)
            {
                CommandList->setTextureState(HalfTex, nvrhi::AllSubresources,
                    nvrhi::ResourceStates::ShaderResource);
                CommandList->setTextureState(LinearDepthTexture, nvrhi::AllSubresources,
                    nvrhi::ResourceStates::ShaderResource);
                CommandList->setTextureState(GBufferNormal, nvrhi::AllSubresources,
                    nvrhi::ResourceStates::ShaderResource);
                CommandList->setTextureState(OutTex, nvrhi::AllSubresources,
                    nvrhi::ResourceStates::UnorderedAccess);

                FBindingSetBuilder SB;
                SB.SetConstantBuffer(0, ResolveConstantsBuffer)
                  .SetTextureSRV(0, HalfTex)
                  .SetTextureSRV(1, LinearDepthTexture)
                  .SetTextureSRV(2, GBufferNormal)
                  .SetTextureUAV(0, OutTex);
                nvrhi::BindingSetHandle BS = NvrhiDevice->createBindingSet(
                    SB.Build(), ResolveBindingLayout);
                nvrhi::ComputeState CS;
                CS.setPipeline(ResolvePipeline);
                CS.addBindingSet(BS);
                CommandList->setComputeState(CS);
                // v192: fixed extent, matching OutTex. See the note above.
                CommandList->dispatch((WIDTH + 7) / 8, (HEIGHT + 7) / 8, 1);
            };

            DispatchResolve(OutputTexture, FullResGIRaw);       // gi_raw dump
            DispatchResolve(SpatialRadiance, FullResSpatial);   // ReBLUR input
            // v210: upscale the primary direct+ambient so the display pass can
            // recombine it with the ReSTIR indirect estimate.
            DispatchResolve(DirectTexture, FullResDirect);
        }
        if (bGpuTimers && GpuTimers[3]) CommandList->endTimerQuery(GpuTimers[3]);
        auto T6 = std::chrono::steady_clock::now();

        // (5.5) ReBLUR denoise on the ReSTIR resolve output (RESTIR mode only)
        if (bGpuTimers && GpuTimers[4]) CommandList->beginTimerQuery(GpuTimers[4]);
        {
            nvrhi::TextureHandle AccumInput = bBypass ? FullResGIRaw : FullResSpatial;
            if (bReBLURInitialized && !bBypass)
            {
                CommandList->setTextureState(
                    FullResSpatial, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
                CommandList->setTextureState(
                    DenoisedTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);
                CommandList->setTextureState(
                    ReBLURHistoryTexture[0], nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
                CommandList->setTextureState(
                    ReBLURHistoryTexture[1], nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);

                ReBLUR::FReBLURConstants ReBLURConstants;
                std::memset(&ReBLURConstants, 0, sizeof(ReBLURConstants));
                glm::mat4 InvCurr = glm::inverse(CurrViewProj);
                memcpy(ReBLURConstants.InverseCurrViewProj, glm::value_ptr(InvCurr), 64);
                memcpy(ReBLURConstants.PrevViewProj, glm::value_ptr(PrevViewProj), 64);
                // v194: WIDTH/HEIGHT, not FB.width/FB.height. Every resource
                // this pass touches is fixed-size — output, input, history and
                // both GBuffer guides — and BackBufferResizing recreates none
                // of them, so the swapchain extent merely coincided at startup.
                //
                // These feed three consumers that must agree: the dispatch grid,
                // the blur-tap clamp, and pixelUv for IsHistoryValid. Widening
                // stores out of bounds (the kernel has no extent guard);
                // narrowing leaves the tail unwritten and rejects history across
                // the rounding pad, silently passing those texels through
                // undenoised. Neither raises a VUID.
                //
                // Set the extent explicitly rather than leaving OutputWidth zero
                // for the getDesc() fallback in FReBLURPass::Dispatch: that
                // fallback fixes only the dispatch grid and never touches these
                // constants, which are the ones the shader reads.
                ReBLURConstants.OutputSize[0] = static_cast<float>(WIDTH);
                ReBLURConstants.OutputSize[1] = static_cast<float>(HEIGHT);
                ReBLURConstants.RcpOutputSize[0] = 1.0f / static_cast<float>(WIDTH);
                ReBLURConstants.RcpOutputSize[1] = 1.0f / static_cast<float>(HEIGHT);
                ReBLURConstants.HitDistParams[0] = 3.0f;
                ReBLURConstants.HitDistParams[1] = 0.1f;
                ReBLURConstants.HitDistParams[2] = 20.0f;
                ReBLURConstants.HitDistParams[3] = -25.0f;
                ReBLURConstants.FrameIndex = static_cast<float>(AccumFrameCount);
                ReBLURConstants.HistoryFadeIn = 8.0f;
                ReBLURConstants.ConfidenceScale = 1.0f;
                ReBLURConstants.PassIndex = 0.0f;

                ReBLUR::FPooledBlurParams BlurParams = ReBLUR::FReBLURPass::GetDefaultBlurParams();

                ReBLUR::FReBLURPass::FDesc ReBLURDesc;
                ReBLURDesc.CurrentRadianceTexture = FullResSpatial;
                ReBLURDesc.HistoryTexture = ReBLURHistoryTexture[0];
                ReBLURDesc.DepthTexture = LinearDepthTexture;
                ReBLURDesc.NormalRoughnessTexture = GBufferNormal;
                ReBLURDesc.OutputTexture = DenoisedTexture;
                ReBLURDesc.OutputWidth = WIDTH;    // v194: see the note above
                ReBLURDesc.OutputHeight = HEIGHT;  // v194: see the note above
                // nvrhi binds descriptor sets BEFORE pending barriers land, so
                // the descriptors capture the stale image layout and reads
                // return garbage (VUID-VkDescriptorImageInfo-imageLayout-00344,
                // "A command list should be executed before it is reopened"
                // class). Flush the transitions above before the binding set is
                // created inside Dispatch.
                CommandList->commitBarriers();
                ReBLURPass.Dispatch(CommandList, ReBLURDesc, ReBLURConstants, BlurParams);

                // Copy denoised output to history for the next frame.
                CommandList->setTextureState(
                    DenoisedTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::CopySource);
                CommandList->setTextureState(
                    ReBLURHistoryTexture[0], nvrhi::AllSubresources, nvrhi::ResourceStates::CopyDest);
                CommandList->copyTexture(
                    ReBLURHistoryTexture[0], nvrhi::TextureSlice(), DenoisedTexture, nvrhi::TextureSlice());

                AccumInput = DenoisedTexture;
            }
            CommandList->setTextureState(
                FullResSpatial, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
            CommandList->setTextureState(
                AccumTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);
            CommandList->setTextureState(
                DisplayTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);
            if (bBypass)
                CommandList->setTextureState(
                    AccumInput, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
            // v210: full-res direct lighting for the display combine.
            CommandList->setTextureState(
                FullResDirect, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);

            ++AccumFrameCount;
            struct FAccumC { uint32_t FrameCount; uint32_t Width; uint32_t Height; float Exposure; };
            FAccumC AccC{};
            AccC.FrameCount = AccumFrameCount;
            // v193: WIDTH/HEIGHT, NOT FB.width/FB.height. Every resource this
            // pass touches is fixed-size: the UAVs AccumTexture and
            // DisplayTexture are created in CreateGBufferTextures from the
            // file-scope WIDTH/HEIGHT, and the SRV input (FullResGIRaw /
            // FullResSpatial / DenoisedTexture) is created there from the same
            // constants. The swapchain was the only swapchain-derived quantity
            // in the pass, and BackBufferResizing clears the binding cache
            // without recreating any texture.
            //
            // These two fields also parameterise the kernel's own bounds guard
            // (the early-out in GIAccumulate_cs.hlsl main, which compares
            // SV_DispatchThreadID against Width/Height). Sourced from the
            // swapchain, that guard clipped the dispatch against the dispatch —
            // a tautology, not a bound. Widening the window therefore stored out
            // of bounds through both UAVs; narrowing left part of DisplayTexture
            // unwritten, and since the dump sizes its readback from the texture
            // descriptor rather than the dispatch, that unwritten region reaches
            // validate_restir_gi.py's black-ratio check. Neither raises a VUID.
            AccC.Width      = WIDTH;
            AccC.Height     = HEIGHT;
            AccC.Exposure   = Exposure;
            CommandList->writeBuffer(AccumulateConstants, &AccC, sizeof(AccC));

            FBindingSetBuilder SetBuilder;
            SetBuilder.SetConstantBuffer(0, AccumulateConstants)
                      .SetTextureSRV(0, AccumInput)
                      .SetTextureSRV(1, FullResDirect)
                      .SetTextureUAV(0, AccumTexture)
                      .SetTextureUAV(1, DisplayTexture);
            nvrhi::BindingSetHandle AccumBS = NvrhiDevice->createBindingSet(
                SetBuilder.Build(), AccumulateBindingLayout);

            nvrhi::ComputeState CS;
            CS.setPipeline(AccumulatePipeline);
            CS.addBindingSet(AccumBS);
            CommandList->setComputeState(CS);
            // v193: grid covers the fixed UAV extent, matching the constants above.
            CommandList->dispatch((WIDTH + 7) / 8, (HEIGHT + 7) / 8, 1);
        }
        if (bGpuTimers && GpuTimers[4]) CommandList->endTimerQuery(GpuTimers[4]);
        auto T7 = std::chrono::steady_clock::now();

        // (7) Blit the accumulated display to the swapchain
        {
            CommandList->setTextureState(
                DisplayTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);

            nvrhi::Color clearColor(0.0f, 0.0f, 0.0f, 1.0f);
            nvrhi::utils::ClearColorAttachment(CommandList, Framebuffer, 0, clearColor);

            FCommonRenderPasses::BlitParameters BlitParams;
            FCommonRenderPasses::BlitTexture(
                CommandList, Framebuffer, DisplayTexture,
                &BindingCache, FB.width, FB.height, BlitParams);
        }

        // v210 (ZetaRay ground truth): snapshot THIS frame's rasterized GBuffer
        // into the true previous-frame chain at the END of the frame — the
        // temporal pass of the NEXT frame must read the surfaces that existed
        // LAST frame at the reprojected pixels. Under the turntable the current
        // frame's GBuffer at those pixels holds different geometry.
        CopyGBufferToPrev();

        if (bProfile)
        {
            static std::vector<double> PG, PR, PI, PD, PB, PA;
            PG.push_back(std::chrono::duration<double, std::milli>(T1 - T0).count());
            PR.push_back(std::chrono::duration<double, std::milli>(T2 - T1).count());
            PI.push_back(std::chrono::duration<double, std::milli>(T5 - T2).count());
            PD.push_back(std::chrono::duration<double, std::milli>(T6 - T5).count());
            PB.push_back(std::chrono::duration<double, std::milli>(T7 - T6).count());
            PA.push_back(std::chrono::duration<double, std::milli>(T7 - T0).count());
            if (PG.size() % 8 == 0)
            {
                auto Avg8 = [](const std::vector<double>& V) {
                    return std::accumulate(V.end() - 8, V.end(), 0.0) / 8.0;
                };
                HLVM_LOG(LogTest, info,
                    TXT("phase ms (avg 8): GBuffer={:.2f} RayTrace={:.2f} ReSTIR={:.2f} Resolve={:.2f} BlitAccum={:.2f} total={:.2f}"),
                    Avg8(PG), Avg8(PR), Avg8(PI), Avg8(PD), Avg8(PB), Avg8(PA));
            }
        }

        // The blit above recorded into the same open CommandList that
        // RenderGBuffer + FGIPass + denoise + ReSTIR + accumulate all wrote
        // into. The whole frame submits at end of Render via line 691.

        // bug-088 (six-role-pipeline v1, root-caused 2026-07-27): the per-frame
        // CommandList was closed at end of Render() but never submitted. The
        // next frame's open() discarded the recorded GPU work (GIPass, denoise,
        // ReSTIR, accumulate, blit). Symptoms in the log: "A command list
        // should be executed before it is reopened" every frame (informational)
        // and vkQueueSubmit validation errors reading DisplayTexture in layout
        // VK_IMAGE_LAYOUT_UNDEFINED (because the accumulate UAV write was
        // never submitted).
        //
        // Note: v5 (six-role-pipeline, 2026-07-27) removed the v1-introduced
        // HLVM-bypass `close+execute+waitForIdle+open` block at the end of
        // RenderGBuffer. That bypass was the regression (gi_raw became 0,0,0
        // after it landed). The v5 NOTE comment near line 1531 documents the
        // rationale.
        //
        // Fix: explicitly execute the per-frame CL after closing it. This
        // submits the post-raster work and lets DeviceManager::EndFrame() open
        // its own immediate CL without colliding with this one. The next
        // frame's Render() then opens a fresh CL (lines 388/399) and no work
        // is lost. (Previously the plan described this as an "open vs close"
        // collision with DeviceManager::EndFrame(); that was wrong — the real
        // bug was the missing executeCommandList, which dropped ~90% of the
        // pipeline's recorded GPU work every frame.)
        const bool bLastFrame = (AccumFrameCount >= AccumTargetFrames);
        if (CommandList)
        {
            if (bGpuTimers && GpuTimers[5]) CommandList->endTimerQuery(GpuTimers[5]);
            CommandList->close();
            NvrhiDevice->executeCommandList(CommandList);
            if (bGpuTimers)
                bTimersPending = true;
        }

        // Dump AFTER the per-frame CL executes. DumpCurrentFrame creates its
        // own CL and submits via executeCommandList — Vulkan's FIFO queue
        // order guarantees the dump sees the post-accumulate state.
        if (bDumpRequested && bLastFrame)
        {
            DumpCurrentFrame();
        }

        // End-of-run numerical summary — always logged on the last frame so
        // the pipeline claims (reservoir M/W, grayscale error, display range)
        // are verifiable from a plain run's log without HLVM_DUMP_RGI.
        if (bLastFrame)
        {
            LogFinalFrameStats();
        }

        if (bLastFrame)
        {
            if (auto* DM = GetDeviceManager()) DM->StopMessageLoop();
        }
    }

    virtual void BackBufferResizing() override
    {
        BindingCache.Clear();
    }

private:
    // ---- Scene load: Sponza GLTF --------------------------------------------
    bool LoadSponza()
    {
        // GExecutablePath is the executable's directory; Sponza is at
        //   ${GProjectRoot}/Samples/Assets/sponza/Sponza01.gltf
        const FPath ScenePath = FPath(FString::Format(
            TXT("{}/Samples/Assets/sponza/Sponza01.gltf"), *GProjectRoot));
        if (!FPath::Exists(ScenePath))
        {
            HLVM_LOG(LogTest, err, TXT("Sponza scene not found at {}"), *FString(ScenePath.string()));
            return false;
        }
        Scene = FScene3DLoader::LoadFromFile(ScenePath);
        if (!Scene || Scene->IsEmpty())
        {
            HLVM_LOG(LogTest, err, TXT("FScene3DLoader returned empty scene"));
            return false;
        }
        HLVM_LOG(LogTest, info, TXT("Sponza loaded ({} mesh groups)"), static_cast<uint32_t>(Scene->MeshTree.size()));

        // Build a unified vertex/index buffer + per-instance info for the RT shaders.
        std::vector<FRTVertex> AllVertices;
        std::vector<uint32_t> AllIndices;
        std::vector<FInstanceInfo> InstanceInfos;

        uint32_t VertexOffset = 0, IndexOffset = 0;
        for (auto& Entry : Scene->MeshTree)
        {
            auto StaticMesh = std::dynamic_pointer_cast<FStaticMesh>(Entry.second);
            if (!StaticMesh) continue;
            const auto& Verts   = StaticMesh->GetVertices();
            const auto& Indices = StaticMesh->GetIndices();
            if (Verts.empty() || Indices.empty()) continue;

            FInstanceInfo Info{};
            Info.VertexOffset = VertexOffset;
            Info.IndexOffset  = IndexOffset;
            Info.VertexCount  = static_cast<uint32_t>(Verts.size());
            Info.IndexCount   = static_cast<uint32_t>(Indices.size());
            Info.AlbedoTextureIndex = 0;
            Info.Roughness = 0.9f;
            Info.Metallic  = 0.0f;

            // Pull material albedo for this mesh if available
            auto MatIt = Scene->MeshMultiMaterialMap.find(Entry.second);
            if (MatIt != Scene->MeshMultiMaterialMap.end() && !MatIt->second.empty())
            {
                const auto& M = MatIt->second[0];
                const FVec3 A = GetMeshAlbedo(Entry.second->GetName(), M->AlbedoColor);
                Info.AlbedoColor[0] = A.x;
                Info.AlbedoColor[1] = A.y;
                Info.AlbedoColor[2] = A.z;
                // 2026-08-10 Phase 1: flag meshes with a real albedo texture so
                // GBufferPT_PS samples it instead of the palette color.
                if (auto PBRMat = std::dynamic_pointer_cast<FPBRMaterial>(M))
                {
                    if (PBRMat->HasTexture(IMaterial::ETextureType::Albedo))
                        Info.MaterialFlags |= 1u;
                    Info.Roughness = PBRMat->GetRoughness();
                    Info.Metallic  = PBRMat->GetMetallic();
                }
            }
            else
            {
                const FVec3 A = GetMeshAlbedo(Entry.second->GetName(), FVec3(0.7f, 0.7f, 0.7f));
                Info.AlbedoColor[0] = A.x;
                Info.AlbedoColor[1] = A.y;
                Info.AlbedoColor[2] = A.z;
            }

            InstanceInfos.push_back(Info);
            for (const auto& V : Verts)
            {
                FRTVertex RTV;
                RTV.Position[0] = V.Position.x;
                RTV.Position[1] = V.Position.y;
                RTV.Position[2] = V.Position.z;
                RTV.Normal[0]   = V.Normal.x;
                RTV.Normal[1]   = V.Normal.y;
                RTV.Normal[2]   = V.Normal.z;
                RTV.UV[0]       = V.UV.x;
                RTV.UV[1]       = V.UV.y;
                AllVertices.push_back(RTV);
            }
            for (uint32_t Idx : Indices) AllIndices.push_back(Idx);

            VertexOffset += Info.VertexCount;
            IndexOffset  += Info.IndexCount;
        }

        if (AllVertices.empty() || AllIndices.empty())
        {
            HLVM_LOG(LogTest, err, TXT("Sponza scene produced no geometry"));
            return false;
        }
        // Phase 3: keep the instance array so the init-time material pass can
        // patch per-mesh average albedo (RT bounce shading) and re-upload.
        AllInstanceInfos = InstanceInfos;

        nvrhi::CommandListHandle InitCmd = NvrhiDevice->createCommandList();
        InitCmd->open();

        // Global vertex buffer
        {
            nvrhi::BufferDesc Desc;
            Desc.byteSize = static_cast<uint32_t>(AllVertices.size() * sizeof(FRTVertex));
            Desc.structStride = sizeof(FRTVertex);
            Desc.initialState = nvrhi::ResourceStates::ShaderResource;
            Desc.keepInitialState = true;
            Desc.isAccelStructBuildInput = true;       // RT buffers need this flag for BLAS build
            Desc.debugName = "ReSTIRGIVertices";
            VertexBuffer = NvrhiDevice->createBuffer(Desc);
            InitCmd->writeBuffer(VertexBuffer, AllVertices.data(), Desc.byteSize);
        }
        // Global index buffer
        {
            nvrhi::BufferDesc Desc;
            Desc.byteSize = static_cast<uint32_t>(AllIndices.size() * sizeof(uint32_t));
            Desc.structStride = sizeof(uint32_t);
            Desc.initialState = nvrhi::ResourceStates::ShaderResource;
            Desc.keepInitialState = true;
            Desc.isAccelStructBuildInput = true;       // RT buffers need this flag for BLAS build
            Desc.debugName = "ReSTIRGIIndices";
            IndexBuffer = NvrhiDevice->createBuffer(Desc);
            InitCmd->writeBuffer(IndexBuffer, AllIndices.data(), Desc.byteSize);
        }
        // Instance info buffer
        {
            nvrhi::BufferDesc Desc;
            Desc.byteSize = static_cast<uint32_t>(InstanceInfos.size() * sizeof(FInstanceInfo));
            Desc.structStride = sizeof(FInstanceInfo);
            Desc.initialState = nvrhi::ResourceStates::ShaderResource;
            Desc.keepInitialState = true;
            Desc.debugName = "ReSTIRGIInstanceInfo";
            InstanceInfoBuffer = NvrhiDevice->createBuffer(Desc);
            InitCmd->writeBuffer(InstanceInfoBuffer, InstanceInfos.data(), Desc.byteSize);
        }

        // One BLAS per instance, all sharing the global vertex/index buffers.
        // Lesson inherited from Vibe_Coding/51_PathTraceGI_Debug:
        //   nvrhi Vulkan RT backend uses BYTE offsets, not vertex/index counts.
        SceneBLASes.clear();
        for (const FInstanceInfo& Info : InstanceInfos)
        {
            nvrhi::rt::GeometryDesc Geom{};
            Geom.geometryType = nvrhi::rt::GeometryType::Triangles;
            Geom.flags = nvrhi::rt::GeometryFlags::Opaque;
            auto& T = Geom.geometryData.triangles;
            T.indexBuffer   = IndexBuffer;
            T.vertexBuffer  = VertexBuffer;
            T.indexFormat   = nvrhi::Format::R32_UINT;
            T.indexOffset   = static_cast<uint64_t>(Info.IndexOffset) * sizeof(uint32_t);
            T.indexCount    = Info.IndexCount;
            T.vertexFormat  = nvrhi::Format::RGB32_FLOAT;
            T.vertexStride  = sizeof(FRTVertex);
            T.vertexOffset  = static_cast<uint64_t>(Info.VertexOffset) * sizeof(FRTVertex);
            T.vertexCount   = Info.VertexCount;

            nvrhi::rt::AccelStructDesc BlasDesc{};
            BlasDesc.isTopLevel = false;
            BlasDesc.bottomLevelGeometries.push_back(Geom);
            nvrhi::rt::AccelStructHandle BLAS = NvrhiDevice->createAccelStruct(BlasDesc);

            nvrhi::utils::BuildBottomLevelAccelStruct(InitCmd, BLAS, BlasDesc);
            SceneBLASes.push_back(BLAS);
        }

        // TLAS — Sponza is large; scale by 0.01 (same as TestRTDispatch) and
        // rotate around Y by the turntable angle. The rotation must match the
        // raster ModelMatrix in UpdateViewConstants exactly: v' = S * Ry * v
        // (row-major 3x4, translation in the 4th column). Rebuilt per frame
        // with the current SceneRotationDeg (see Render).
        {
            nvrhi::rt::AccelStructDesc TlasDesc{};
            TlasDesc.isTopLevel = true;
            TlasDesc.topLevelMaxInstances = static_cast<uint32_t>(SceneBLASes.size());
            SceneTLAS = NvrhiDevice->createAccelStruct(TlasDesc);
            BuildTLAS(InitCmd);
        }

        InitCmd->setBufferState(VertexBuffer, nvrhi::ResourceStates::ShaderResource);
        InitCmd->setBufferState(IndexBuffer,  nvrhi::ResourceStates::ShaderResource);

        InitCmd->close();
        NvrhiDevice->executeCommandList(InitCmd);
        NvrhiDevice->waitForIdle();
        return true;
    }

    // Rebuild the TLAS with the current SceneRotationDeg (turntable). All
    // instances share the same scale(0.01)*Ry(angle) transform. Called at init
    // and once per frame so the ray tracer follows the rotating scene.
    bool BuildTLAS(nvrhi::ICommandList* Cmd)
    {
        if (!Cmd || !SceneTLAS || SceneBLASes.empty())
            return false;

        const float RotCos = glm::cos(glm::radians(SceneRotationDeg));
        const float RotSin = glm::sin(glm::radians(SceneRotationDeg));
        float Transform[12] = {
            0.01f * RotCos,  0.0f,          0.01f * RotSin, 0.0f,
            0.0f,            0.01f,         0.0f,           0.0f,
           -0.01f * RotSin,  0.0f,          0.01f * RotCos, 0.0f
        };

        std::vector<nvrhi::rt::InstanceDesc> InstanceDescs;
        InstanceDescs.reserve(SceneBLASes.size());
        for (nvrhi::rt::AccelStructHandle BLAS : SceneBLASes)
        {
            nvrhi::rt::InstanceDesc InstanceDesc{};
            InstanceDesc.bottomLevelAS = BLAS;
            InstanceDesc.instanceMask  = 1;
            InstanceDesc.flags         = nvrhi::rt::InstanceFlags::TriangleFrontCounterclockwise;
            memcpy(InstanceDesc.transform, Transform, sizeof(Transform));
            InstanceDescs.push_back(InstanceDesc);
        }
        Cmd->buildTopLevelAccelStruct(
            SceneTLAS, InstanceDescs.data(),
            static_cast<uint32_t>(InstanceDescs.size()));
        return true;
    }

    // ---- GBuffer textures + view + dummy depth ------------------------------
    bool CreateGBufferTextures()
    {
        const uint32_t W = WIDTH, H = HEIGHT;

        // v213 (Phase 5b): per-phase GPU timers (HLVM_RGI_GPU_TIMERS=1).
        bGpuTimers = (std::getenv("HLVM_RGI_GPU_TIMERS") != nullptr);
        if (bGpuTimers)
        {
            for (int i = 0; i < 6; ++i)
                GpuTimers[i] = NvrhiDevice->createTimerQuery();
        }

        nvrhi::TextureDesc WpDesc;
        WpDesc.dimension  = nvrhi::TextureDimension::Texture2D;
        WpDesc.width      = W; WpDesc.height = H;
        WpDesc.format     = nvrhi::Format::RGBA32_FLOAT;
        WpDesc.isRenderTarget = true;
        WpDesc.initialState  = nvrhi::ResourceStates::RenderTarget;
        WpDesc.keepInitialState = true;
        WpDesc.debugName = "GBufferWorldPos";
        GBufferWorldPos = NvrhiDevice->createTexture(WpDesc);

        nvrhi::TextureDesc NmDesc = WpDesc;
        NmDesc.debugName = "GBufferNormal";
        GBufferNormal = NvrhiDevice->createTexture(NmDesc);

        nvrhi::TextureDesc MtDesc = WpDesc;
        MtDesc.debugName = "GBufferMaterial";
        GBufferMaterial = NvrhiDevice->createTexture(MtDesc);

        // v210 (ZetaRay ground-truth port): true PREVIOUS-frame GBuffer chain.
        // Under the turntable the surface at a reprojected pixel in the
        // CURRENT frame is a different surface than the one that was there
        // last frame, so temporal reuse must read last frame's GBuffer (the
        // old same-frame aliases made every candidate fail validation and M
        // never accumulated). Copied from the current GBuffer after every
        // raster pass.
        nvrhi::TextureDesc PrevWpDesc = WpDesc;
        PrevWpDesc.debugName = "PrevGBufferWorldPos";
        PrevWpDesc.initialState = nvrhi::ResourceStates::CopyDest;
        PrevWpDesc.keepInitialState = true;
        PrevGBufferWorldPos = NvrhiDevice->createTexture(PrevWpDesc);

        nvrhi::TextureDesc PrevNmDesc = PrevWpDesc;
        PrevNmDesc.debugName = "PrevGBufferNormal";
        PrevGBufferNormal = NvrhiDevice->createTexture(PrevNmDesc);

        nvrhi::TextureDesc PrevMtDesc = PrevWpDesc;
        PrevMtDesc.debugName = "PrevGBufferMaterial";
        PrevGBufferMaterial = NvrhiDevice->createTexture(PrevMtDesc);

        // Depth attachment (D32) — was created but NEVER attached to the
        // framebuffer, and depth test/write were disabled (fixed 2026-08-10).
        // Without occlusion every overlapping Sponza mesh fragment wrote the
        // GBuffer and the last-drawn mesh won per pixel — a patchwork of front
        // AND back faces ("inside-out mesh" look).
        nvrhi::TextureDesc DpDesc;
        DpDesc.dimension = nvrhi::TextureDimension::Texture2D;
        DpDesc.width = W; DpDesc.height = H;
        DpDesc.format = nvrhi::Format::D32;
        DpDesc.isRenderTarget = true;
        DpDesc.isUAV = false;
        DpDesc.isTypeless = true;
        DpDesc.initialState = nvrhi::ResourceStates::DepthWrite;
        DpDesc.keepInitialState = true;
        DpDesc.debugName = "GBufferDepth";
        GBufferDepth = NvrhiDevice->createTexture(DpDesc);

        // MRT3 — linear view-space depth (R32F), written by GBufferPT_PS.
        // isRenderTarget=true so it can be attached to the GBuffer framebuffer.
        LinearDepthTexture = CreateTexture2D(
            NvrhiDevice, W, H, nvrhi::Format::R32_FLOAT,
            nvrhi::ResourceStates::RenderTarget, "LinearDepth");
        {
            nvrhi::TextureDesc LtDesc = LinearDepthTexture->getDesc();
            LtDesc.isRenderTarget = true;
            LtDesc.initialState = nvrhi::ResourceStates::RenderTarget;
            LtDesc.keepInitialState = true;
            LtDesc.debugName = "LinearDepth";
            LinearDepthTexture = NvrhiDevice->createTexture(LtDesc);
        }
        nvrhi::TextureDesc PrevLtDesc = LinearDepthTexture->getDesc();
        PrevLtDesc.debugName = "PrevLinearDepth";
        PrevLtDesc.initialState = nvrhi::ResourceStates::CopyDest;
        PrevLtDesc.keepInitialState = true;
        PrevLinearDepth = NvrhiDevice->createTexture(PrevLtDesc);

        // Pipeline color/depth outputs for the GBuffer pass
        // Phase D (PLAN_REALTIME_RESTIR_GAP): ReSTIR GI traces and reuses at
        // HALF resolution; a depth/normal-weighted resolve pass upsamples the
        // result to full res for ReBLUR/accumulate/display.
        const uint32_t HalfW = W / 2;
        const uint32_t HalfH = H / 2;
        HalfResWidth = HalfW;
        HalfResHeight = HalfH;
        OutputTexture = CreateTexture2D(
            NvrhiDevice, HalfW, HalfH, nvrhi::Format::RGBA32_FLOAT,
            nvrhi::ResourceStates::UnorderedAccess, "GIRawHDR");
        DirectionTexture = CreateTexture2D(
            NvrhiDevice, HalfW, HalfH, nvrhi::Format::RGBA32_FLOAT,
            nvrhi::ResourceStates::UnorderedAccess, "GIPrimaryDirection");
        // v210 (ZetaRay ground-truth port): candidate sample state UAVs.
        SampleInfoTexture = CreateTexture2D(
            NvrhiDevice, HalfW, HalfH, nvrhi::Format::RGBA32_FLOAT,
            nvrhi::ResourceStates::UnorderedAccess, "GISampleInfo");
        DirectTexture = CreateTexture2D(
            NvrhiDevice, HalfW, HalfH, nvrhi::Format::RGBA32_FLOAT,
            nvrhi::ResourceStates::UnorderedAccess, "GIDirect");
        DenoisedTexture = CreateTexture2D(
            NvrhiDevice, W, H, nvrhi::Format::RGBA32_FLOAT,
            nvrhi::ResourceStates::UnorderedAccess, "DenoisedHDR");

        // ReSTIR reservoirs
        ReservoirTex0 = CreateTexture2D(NvrhiDevice, HalfW, HalfH, nvrhi::Format::RGBA32_FLOAT,
            nvrhi::ResourceStates::UnorderedAccess, "Reservoir0");
        ReservoirTex1 = CreateTexture2D(NvrhiDevice, HalfW, HalfH, nvrhi::Format::RGBA32_FLOAT,
            nvrhi::ResourceStates::UnorderedAccess, "Reservoir1");
        ReservoirTex2 = CreateTexture2D(NvrhiDevice, HalfW, HalfH, nvrhi::Format::RGBA32_FLOAT,
            nvrhi::ResourceStates::UnorderedAccess, "Reservoir2");
        TemporalReservoir0 = CreateTexture2D(NvrhiDevice, HalfW, HalfH, nvrhi::Format::RGBA32_FLOAT,
            nvrhi::ResourceStates::UnorderedAccess, "TemporalReservoir0");
        TemporalReservoir1 = CreateTexture2D(NvrhiDevice, HalfW, HalfH, nvrhi::Format::RGBA32_FLOAT,
            nvrhi::ResourceStates::UnorderedAccess, "TemporalReservoir1");
        // Second temporal pair (2/3): the temporal pass ping-pongs between
        // the two PAIRS (0/1 <-> 2/3) instead of aliasing a single pair, so
        // no texture is ever bound as both SRV history and UAV output in the
        // same dispatch (that alias is a Vulkan layout conflict — the SRV
        // descriptor says SHADER_READ_ONLY while the UAV binding needs
        // GENERAL for the same image).
        TemporalReservoir2 = CreateTexture2D(NvrhiDevice, HalfW, HalfH, nvrhi::Format::RGBA32_FLOAT,
            nvrhi::ResourceStates::UnorderedAccess, "TemporalReservoir2");
        TemporalReservoir3 = CreateTexture2D(NvrhiDevice, HalfW, HalfH, nvrhi::Format::RGBA32_FLOAT,
            nvrhi::ResourceStates::UnorderedAccess, "TemporalReservoir3");
        TemporalReservoir4 = CreateTexture2D(NvrhiDevice, HalfW, HalfH, nvrhi::Format::RGBA32_FLOAT,
            nvrhi::ResourceStates::UnorderedAccess, "TemporalReservoir4");
        TemporalReservoir5 = CreateTexture2D(NvrhiDevice, HalfW, HalfH, nvrhi::Format::RGBA32_FLOAT,
            nvrhi::ResourceStates::UnorderedAccess, "TemporalReservoir5");
        SpatialRadiance = CreateTexture2D(NvrhiDevice, HalfW, HalfH, nvrhi::Format::RGBA32_FLOAT,
            nvrhi::ResourceStates::UnorderedAccess, "SpatialRadiance");

        // Phase D: full-res resolve outputs (upscaled from the half-res trace).
        FullResGIRaw = CreateTexture2D(
            NvrhiDevice, W, H, nvrhi::Format::RGBA32_FLOAT,
            nvrhi::ResourceStates::UnorderedAccess, "FullResGIRaw");
        FullResSpatial = CreateTexture2D(
            NvrhiDevice, W, H, nvrhi::Format::RGBA32_FLOAT,
            nvrhi::ResourceStates::UnorderedAccess, "FullResSpatial");
        FullResDirect = CreateTexture2D(
            NvrhiDevice, W, H, nvrhi::Format::RGBA32_FLOAT,
            nvrhi::ResourceStates::UnorderedAccess, "FullResDirect");

        AccumTexture  = CreateTexture2D(NvrhiDevice, W, H, nvrhi::Format::RGBA32_FLOAT,
            nvrhi::ResourceStates::UnorderedAccess, "Accum");
        DisplayTexture = CreateTexture2D(NvrhiDevice, W, H, nvrhi::Format::RGBA32_FLOAT,
            nvrhi::ResourceStates::UnorderedAccess, "Display");

        return true;
    }

    // ---- GBuffer fill ------------------------------------------------------
    // Replace the missing Sponza GBuffer pass with a CPU writeTexture of
    // known-good values into the GBuffer UAVs. This gives FGIPass real input
    // (non-zero worldPos, valid normal, valid material) so the path tracer
    // actually has a primary hit to integrate. Out-of-scope: a real
    // Sponza GBuffer VS/PS — that's a follow-up card.
    //
    // Encoding note: GIPathTracing.hlsl decodes normals as
    //   normal = GBufferNormal[i].rgb * 2.0 - 1.0
    // so we must store the *encoded* `(n*0.5+0.5)` form, not the raw
    // [-1,+1] direction. WorldPos and Material pass through unchanged.
    //
    // SUPERSEDED by the real Sponza GBuffer PT pass (GBufferPT_VS/PS +
    // RenderGBuffer) — this function is retained as a documented
    // "no-Sponza GBuffer" fallback for tests where the raster pass is
    // not desired. Kept here so the build does not need a deep edit
    // when someone wants to disable the raster pass temporarily.
    void FillGBufferHardcoded()
    {
        const uint32_t W = WIDTH, H = HEIGHT;
        const size_t N = static_cast<size_t>(W) * H;

        // WorldPos = (-0.5, -0.5, 0.5), alpha = 1.0 (encoded "hit present")
        std::vector<float> WpData(N * 4);
        for (size_t i = 0; i < N; ++i)
        {
            WpData[i*4 + 0] = -0.5f;
            WpData[i*4 + 1] = -0.5f;
            WpData[i*4 + 2] =  0.5f;
            WpData[i*4 + 3] =  1.0f;
        }

        // Normal = (0, 0, 1) encoded -> (0.5, 0.5, 1.0), alpha = 1.0
        std::vector<float> NmData(N * 4);
        for (size_t i = 0; i < N; ++i)
        {
            NmData[i*4 + 0] = 0.5f;
            NmData[i*4 + 1] = 0.5f;
            NmData[i*4 + 2] = 1.0f;
            NmData[i*4 + 3] = 1.0f;
        }

        // Material albedo = (0.8, 0.2, 0.2), alpha reserved (1.0)
        std::vector<float> MtData(N * 4);
        for (size_t i = 0; i < N; ++i)
        {
            MtData[i*4 + 0] = 0.8f;
            MtData[i*4 + 1] = 0.2f;
            MtData[i*4 + 2] = 0.2f;
            MtData[i*4 + 3] = 1.0f;
        }

        nvrhi::CommandListHandle Cmd = NvrhiDevice->createCommandList();
        Cmd->open();
        // GBuffer textures are created in RenderTarget/UnorderedAccess initial
        // state; transition to CopyDest for the upload.
        Cmd->setTextureState(GBufferWorldPos, nvrhi::AllSubresources, nvrhi::ResourceStates::CopyDest);
        Cmd->setTextureState(GBufferNormal,   nvrhi::AllSubresources, nvrhi::ResourceStates::CopyDest);
        Cmd->setTextureState(GBufferMaterial, nvrhi::AllSubresources, nvrhi::ResourceStates::CopyDest);
        Cmd->writeTexture(GBufferWorldPos, 0, 0, WpData.data(), static_cast<size_t>(W) * sizeof(float) * 4);
        Cmd->writeTexture(GBufferNormal,   0, 0, NmData.data(), static_cast<size_t>(W) * sizeof(float) * 4);
        Cmd->writeTexture(GBufferMaterial, 0, 0, MtData.data(), static_cast<size_t>(W) * sizeof(float) * 4);
        // Transition to SRV so FGIPass's `Texture2D<float4>` GBuffer bindings
        // find the correct layout when the first ray trace runs.
        Cmd->setTextureState(GBufferWorldPos, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        Cmd->setTextureState(GBufferNormal,   nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        Cmd->setTextureState(GBufferMaterial, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        Cmd->close();
        NvrhiDevice->executeCommandList(Cmd);
        NvrhiDevice->waitForIdle();

        // The CPU fill command list transitioned the textures into
        // ShaderResource state. When the first per-frame command list opens
        // and calls FGIPass, nvrhi tracks those textures as ShaderResource,
        // so the SRV reads inside the path tracer are valid (no validation
        // layer complaint about GENERAL vs SHADER_READ_ONLY_OPTIMAL).

        HLVM_LOG(LogTest, info, TXT("Filled GBuffer with hardcoded quad: "
            "worldPos=(-0.5,-0.5,0.5), normal=(0,0,1), material=(0.8,0.2,0.2)"));
    }

    // ---- GBuffer sentinel writes (per-frame, BEFORE the GBuffer pass) ------
    // Fills the 3 GBuffer textures with unique magic values per channel. The
    // GBuffer pass immediately overwrites whatever it actually rasterizes; any
    // pixel the pass does NOT write keeps the sentinel. A downstream dump then
    // proves "did the pass touch this pixel?" — if a pixel still shows the
    // sentinel value, the GBuffer rasterizer skipped it.
    //
    // Sentinel values are chosen so they cannot be confused with legitimate
    // Sponza data:
    //   WorldPos  = (-100, -200, -300, 1.0)
    //               Sponza's TLAS is scaled 0.01 and centered at floor → bbox is
    //               roughly ±0.06 in scaled units. -100/-200/-300 is far outside.
    //   Normal    = (0.111, 0.222, 0.333, 1.0)        (encoded form [0,1])
    //               Decodes to (-0.778, -0.556, -0.334) — magnitude 1.016, not a
    //               unit vector; any real normal from normalize() is exactly
    //               length 1.0 after decoding. The 0.111/0.222/0.333 pattern is
    //               also distinctive vs the (0.5, 0.5, 1.0) normal-up default.
    //   Material  = (0.999, 0.001, 0.500, 1.0)
    //               Distinctive pattern (super-bright R, near-zero G, mid B);
    //               no plausible PBR albedo looks like this.
    //
    // Upload path mirrors FillGBufferHardcoded(): transition RT → CopyDest,
    // writeTexture, transition back to RenderTarget for the raster pass (the
    // GBufferFrameBuffer attaches the textures as RTVs).
    //
    // Per-frame CPU upload cost is ~5.76 MB (3 × 800×600×4 floats). Cheap.
    void WriteGBufferSentinels()
    {
        if (!NvrhiDevice || !GBufferWorldPos || !GBufferNormal || !GBufferMaterial)
            return;

        const uint32_t W = WIDTH, H = HEIGHT;
        const size_t N = static_cast<size_t>(W) * H;

        // WorldPos sentinel: (99, 99, 99), alpha = 1.0 — bright canary so we can
        // distinguish raster-written pixels (visible geometry) from sentinel-
        // only pixels. If the GBuffer dump shows all-99, the raster pass is
        // not overwriting; if it shows real positions, the raster worked.
        std::vector<float> WpData(N * 4);
        for (size_t i = 0; i < N; ++i)
        {
            WpData[i*4 + 0] =  99.0f;
            WpData[i*4 + 1] =  99.0f;
            WpData[i*4 + 2] =  99.0f;
            WpData[i*4 + 3] =   1.0f;
        }
        // Normal sentinel (encoded): (0.111, 0.222, 0.333), alpha = 1.0
        std::vector<float> NmData(N * 4);
        for (size_t i = 0; i < N; ++i)
        {
            NmData[i*4 + 0] = 0.111f;
            NmData[i*4 + 1] = 0.222f;
            NmData[i*4 + 2] = 0.333f;
            NmData[i*4 + 3] = 1.0f;
        }
        // Material sentinel: (0.999, 0.001, 0.500), alpha = 1.0
        std::vector<float> MtData(N * 4);
        for (size_t i = 0; i < N; ++i)
        {
            MtData[i*4 + 0] = 0.999f;
            MtData[i*4 + 1] = 0.001f;
            MtData[i*4 + 2] = 0.500f;
            MtData[i*4 + 3] = 1.000f;
        }

        nvrhi::CommandListHandle Cmd = NvrhiDevice->createCommandList();
        Cmd->open();
        // GBuffer textures start in RenderTarget initial state (created by
        // CreateGBufferTextures) and nvrhi tracks per-frame transitions. Before
        // a writeTexture upload we need CopyDest; the raster pass will move
        // them back to RenderTarget implicitly when the framebuffer binds.
        Cmd->setTextureState(GBufferWorldPos, nvrhi::AllSubresources, nvrhi::ResourceStates::CopyDest);
        Cmd->setTextureState(GBufferNormal,   nvrhi::AllSubresources, nvrhi::ResourceStates::CopyDest);
        Cmd->setTextureState(GBufferMaterial, nvrhi::AllSubresources, nvrhi::ResourceStates::CopyDest);
        Cmd->writeTexture(GBufferWorldPos, 0, 0, WpData.data(), static_cast<size_t>(W) * sizeof(float) * 4);
        Cmd->writeTexture(GBufferNormal,   0, 0, NmData.data(), static_cast<size_t>(W) * sizeof(float) * 4);
        Cmd->writeTexture(GBufferMaterial, 0, 0, MtData.data(), static_cast<size_t>(W) * sizeof(float) * 4);
        // Move back to RenderTarget so the framebuffer-driven GBuffer pass can
        // write into them. (nvrhi Vulkan: COLOR_ATTACHMENT_OPTIMAL for the FB
        // attachment slot.)
        Cmd->setTextureState(GBufferWorldPos, nvrhi::AllSubresources, nvrhi::ResourceStates::RenderTarget);
        Cmd->setTextureState(GBufferNormal,   nvrhi::AllSubresources, nvrhi::ResourceStates::RenderTarget);
        Cmd->setTextureState(GBufferMaterial, nvrhi::AllSubresources, nvrhi::ResourceStates::RenderTarget);
        Cmd->close();
        NvrhiDevice->executeCommandList(Cmd);
        NvrhiDevice->waitForIdle();
    }

    // ---- GBuffer PT pipeline ------------------------------------------------
    // Builds the real Sponza GBuffer raster pass that produces the inputs
    // FGIPass reads. The GBuffer pass uses:
    //   - GBufferPT_VS.hlsl   (input: FVertex POS/NORMAL/UV/TANGENT, 44B stride)
    //   - GBufferPT_PS.hlsl   (output: 3 MRTs -> WorldPos, Normal, Material)
    //
    // Bindings (see GBufferPT_VS.hlsl):
    //   b0  ViewConstants    (per-frame, shared across draws)
    //   b1  PerInstanceInfo  (one FInstanceInfo per draw, 48B)
    //
    // Per-draw constants are bound via a small 48B constant buffer
    // (GBufferPerInstanceCB). We do N draws (one per FStaticMesh) so that
    // each draw only sees its own FInstanceInfo via b1.
    //
    // Note: GBuffer textures are created in `ResourceStates::RenderTarget`
    // initial state by CreateGBufferTextures. The framebuffer therefore
    // attaches them as color attachments directly; no extra transition
    // required at init time. RenderGBuffer transitions them to ShaderResource
    // after the draw for FGIPass's SRV reads.
    bool CreateGBufferPipeline(const FString& DataDir)
    {
        // ---- Load shaders from sblob --------------------------------------
        auto VSBlob = ReadBinaryFile(
            FPath::Combine(DataDir, TXT("GBufferPT_VS.sblob")).string());
        const void* VSBin = nullptr; size_t VSBinSize = 0;
        if (!ShaderMake::FindPermutationInBlob(VSBlob.data(), VSBlob.size(),
                nullptr, 0, &VSBin, &VSBinSize))
        {
            HLVM_LOG(LogTest, err, TXT("Failed to extract GBufferPT_VS"));
            return false;
        }
        nvrhi::ShaderDesc VSDesc;
        VSDesc.setShaderType(nvrhi::ShaderType::Vertex);
        GBufferVS = NvrhiDevice->createShader(VSDesc, VSBin, VSBinSize);

        auto PSBlob = ReadBinaryFile(
            FPath::Combine(DataDir, TXT("GBufferPT_PS.sblob")).string());
        const void* PSBin = nullptr; size_t PSBinSize = 0;
        if (!ShaderMake::FindPermutationInBlob(PSBlob.data(), PSBlob.size(),
                nullptr, 0, &PSBin, &PSBinSize))
        {
            HLVM_LOG(LogTest, err, TXT("Failed to extract GBufferPT_PS"));
            return false;
        }
        nvrhi::ShaderDesc PSDesc;
        PSDesc.setShaderType(nvrhi::ShaderType::Pixel);
        GBufferPS = NvrhiDevice->createShader(PSDesc, PSBin, PSBinSize);

        if (!GBufferVS || !GBufferPS)
        {
            HLVM_LOG(LogTest, err, TXT("Failed to create GBuffer PT shaders"));
            return false;
        }
        HLVM_LOG(LogTest, info, TXT("GBuffer PT shaders loaded"));

        // ---- Input layout (matches GBufferPT_VS.hlsl VSInput) -------------
        // FVertex layout from IMesh.h, but FVec3 is glm::aligned_lowp vec3
        // (16-byte padded, not 12), so sizeof(FVertex) is actually 64 bytes
        // on this build, not 44 as the comment in IMesh.h claims.
        //   Position FVec3  -> offset 0   (16B: xyz + 4B pad)
        //   Normal   FVec3  -> offset 16  (16B)
        //   UV       FVec2  -> offset 32  (8B)
        //   Tangent  FVec3  -> offset 40  (16B: xyz + 4B pad, ends at 56)
        //   elementStride = 64 (sizeof(FVertex) = 64 on aligned build)
        // 2026-08-09: only POSITION/NORMAL were consumed (UV/TANGENT removed,
        // killing two Vulkan warnings). 2026-08-10 Phase 1 (material rework)
        // re-adds TEXCOORD0 — the GBuffer PS samples the Sponza albedo texture.
        // TANGENT stays out until normal mapping.
        nvrhi::VertexAttributeDesc Attrs[3];
        Attrs[0].setName("POSITION").setFormat(nvrhi::Format::RGB32_FLOAT)
            .setOffset(0).setElementStride(sizeof(FVertex));
        Attrs[1].setName("NORMAL").setFormat(nvrhi::Format::RGB32_FLOAT)
            .setOffset(16).setElementStride(sizeof(FVertex));
        Attrs[2].setName("TEXCOORD0").setFormat(nvrhi::Format::RG32_FLOAT)
            .setOffset(32).setElementStride(sizeof(FVertex));
        GBufferInputLayout = NvrhiDevice->createInputLayout(
            Attrs, 3, GBufferVS);
        HLVM_LOG(LogTest, info, TXT("GBufferInputLayout={} stride={} sizeof(FVertex)={}"),
            GBufferInputLayout ? 1 : 0,
            static_cast<uint32_t>(sizeof(FVertex)), static_cast<uint32_t>(sizeof(FVertex)));
        HLVM_LOG(LogTest, info, TXT("GBuffer input layout: stride={} (sizeof(FVertex)={})"),
            static_cast<uint32_t>(sizeof(FVertex)), static_cast<uint32_t>(sizeof(FVertex)));

        // ---- Binding layout: b0 ViewConstants, b1 PerInstanceInfo ---------
        FBindingLayoutBuilder BLB;
        BLB.SetVisibility(nvrhi::ShaderType::Vertex | nvrhi::ShaderType::Pixel)
           .AddConstantBuffer(0)
           .AddConstantBuffer(1)
           .AddTextureSRV(0)   // t0 - material albedo (2026-08-10 Phase 1)
           .AddSampler(0);     // s0 - linear sampler

        nvrhi::VulkanBindingOffsets Offsets;
        Offsets.setConstantBufferOffset(0)
               .setShaderResourceOffset(0)
               .setSamplerOffset(0)
               .setUnorderedAccessViewOffset(0);
        BLB.SetBindingOffsets(Offsets);

        GBufferBindingLayout = NvrhiDevice->createBindingLayout(BLB.Build());

        // ---- Per-instance constants buffer (48B; one FInstanceInfo) -------
        {
            nvrhi::BufferDesc Desc;
            Desc.byteSize = sizeof(FInstanceInfo);
            Desc.isConstantBuffer = true;
            Desc.initialState = nvrhi::ResourceStates::ConstantBuffer;
            Desc.keepInitialState = true;
            Desc.debugName = "GBufferPerInstanceCB";
            GBufferPerInstanceCB = NvrhiDevice->createBuffer(Desc);
        }

        // ---- GBuffer framebuffer (4 color attachments + D32 depth) --------
        {
            nvrhi::FramebufferDesc FBDesc;
            nvrhi::FramebufferAttachment Wp, Nm, Mt, Ld, D;
            Wp.setTexture(GBufferWorldPos);
            Nm.setTexture(GBufferNormal);
            Mt.setTexture(GBufferMaterial);
            Ld.setTexture(LinearDepthTexture);
            D.setTexture(GBufferDepth);
            FBDesc.addColorAttachment(Wp);
            FBDesc.addColorAttachment(Nm);
            FBDesc.addColorAttachment(Mt);
            FBDesc.addColorAttachment(Ld);
            FBDesc.setDepthAttachment(D);
            GBufferFrameBuffer = NvrhiDevice->createFramebuffer(FBDesc);
            if (!GBufferFrameBuffer)
            {
                HLVM_LOG(LogTest, err, TXT("Failed to create GBuffer framebuffer"));
                return false;
            }
        }
        HLVM_LOG(LogTest, info, TXT("GBuffer framebuffer created (4 MRTs + D32 depth)"));

        // ---- Graphics pipeline -------------------------------------------
        {
            nvrhi::GraphicsPipelineDesc PipelineDesc;
            PipelineDesc.setVertexShader(GBufferVS);
            PipelineDesc.setPixelShader(GBufferPS);
            PipelineDesc.setInputLayout(GBufferInputLayout);
            PipelineDesc.addBindingLayout(GBufferBindingLayout);
            PipelineDesc.setPrimType(nvrhi::PrimitiveType::TriangleList);
            PipelineDesc.renderState.setRasterState(
                nvrhi::RasterState().setCullNone());
            PipelineDesc.renderState.depthStencilState
                .setDepthTestEnable(true)
                .setDepthWriteEnable(true)
                .setDepthFunc(nvrhi::ComparisonFunc::Less);

            GBufferPipeline = NvrhiDevice->createGraphicsPipeline(
                PipelineDesc, GBufferFrameBuffer->getFramebufferInfo());
            HLVM_LOG(LogTest, info, TXT("GBufferPS={} GBufferVS={} GBufferPipeline={} GBufferInputLayout={}"),
                GBufferPS ? 1 : 0, GBufferVS ? 1 : 0,
                GBufferPipeline ? 1 : 0, GBufferInputLayout ? 1 : 0);
            if (!GBufferPipeline)
            {
                HLVM_LOG(LogTest, err, TXT("Failed to create GBuffer PT pipeline"));
                return false;
            }
        }
        HLVM_LOG(LogTest, info, TXT("GBuffer PT pipeline created"));

        // ---- GBuffer vertex/index buffers (FVertex 44B layout) -----------
        // Build a flat concatenation of all FStaticMesh vertex/index data
        // so the GBuffer pass can issue N drawIndexed calls (one per mesh)
        // against a single VB/IB pair. Per-mesh offsets are encoded in
        // FInstanceInfo (VertexOffset / IndexOffset) and used as draw-time
        // startVertexLocation / startIndexLocation.
        if (!Scene)
        {
            HLVM_LOG(LogTest, err, TXT("GBuffer PT: Scene is null"));
            return false;
        }
        std::vector<FVertex>  AllGBufferVerts;
        std::vector<uint32_t> AllGBufferIndices;
        for (auto& Entry : Scene->MeshTree)
        {
            auto StaticMesh = std::dynamic_pointer_cast<FStaticMesh>(Entry.second);
            if (!StaticMesh) continue;
            const auto& V = StaticMesh->GetVertices();
            const auto& I = StaticMesh->GetIndices();
            if (V.empty() || I.empty()) continue;
            for (const auto& v : V)     AllGBufferVerts.push_back(v);
            for (uint32_t idx : I)      AllGBufferIndices.push_back(idx);
        }
        if (AllGBufferVerts.empty() || AllGBufferIndices.empty())
        {
            HLVM_LOG(LogTest, err, TXT("GBuffer PT: Sponza produced no geometry"));
            return false;
        }
        {
            nvrhi::BufferDesc VDesc;
            VDesc.byteSize = static_cast<uint32_t>(AllGBufferVerts.size() * sizeof(FVertex));
            VDesc.isVertexBuffer = true;
            VDesc.isVolatile = false;
            VDesc.initialState = nvrhi::ResourceStates::CopyDest;
            VDesc.debugName = "GBufferVertices";
            GBufferVertexBuffer = NvrhiDevice->createBuffer(VDesc);
        }
        {
            nvrhi::BufferDesc IDesc;
            IDesc.byteSize = static_cast<uint32_t>(AllGBufferIndices.size() * sizeof(uint32_t));
            IDesc.isIndexBuffer = true;
            IDesc.isVolatile = false;
            IDesc.initialState = nvrhi::ResourceStates::CopyDest;
            IDesc.debugName = "GBufferIndices";
            GBufferIndexBuffer = NvrhiDevice->createBuffer(IDesc);
        }
        nvrhi::CommandListHandle InitCmd = NvrhiDevice->createCommandList();
        InitCmd->open();
        InitCmd->beginTrackingBufferState(GBufferVertexBuffer, nvrhi::ResourceStates::CopyDest);
        InitCmd->writeBuffer(GBufferVertexBuffer, AllGBufferVerts.data(),
            static_cast<uint32_t>(AllGBufferVerts.size() * sizeof(FVertex)));
        InitCmd->setPermanentBufferState(GBufferVertexBuffer, nvrhi::ResourceStates::VertexBuffer);
        InitCmd->beginTrackingBufferState(GBufferIndexBuffer, nvrhi::ResourceStates::CopyDest);
        InitCmd->writeBuffer(GBufferIndexBuffer, AllGBufferIndices.data(),
            static_cast<uint32_t>(AllGBufferIndices.size() * sizeof(uint32_t)));
        InitCmd->setPermanentBufferState(GBufferIndexBuffer, nvrhi::ResourceStates::IndexBuffer);

        // Sun-only NEE light buffer for the "GI under sunlight" interior look
        // (2026-08-10). Passed via FGIPassDesc::LightsBuffer so it replaces the
        // default fallback lights (1 dim sun + 3 interior point lights uploaded
        // by FGIPass::UploadLights) — sunlight + sky GI drive the interior.
        {
            Renderer::FLight SunLight{};
            SunLight.type = static_cast<uint32_t>(Renderer::ELightType::Directional);
            // World-space sun direction; stays fixed while the scene rotates,
            // so the sunlit facade changes as Sponza turns. Override with
            // HLVM_RGI_SUN_DIR="x y z".
            const glm::vec3 SunDir = glm::normalize(
                EnvVec3("HLVM_RGI_SUN_DIR", glm::vec3(-0.55f, 0.75f, 0.35f)));
            SunLight.direction[0] = SunDir.x;
            SunLight.direction[1] = SunDir.y;
            SunLight.direction[2] = SunDir.z;
            SunLight.intensity = 8.0f;
            SunLight.color[0] = 1.0f;
            SunLight.color[1] = 0.98f;
            SunLight.color[2] = 0.92f;
            SunLight.range = 1e20f;
            SunLight.flags = Renderer::kLightFlag_CastShadow;
            SunLight.shadowMapIndex = Renderer::kNoShadowMap;

            nvrhi::BufferDesc SunDesc;
            SunDesc.byteSize = sizeof(Renderer::FLight);
            SunDesc.structStride = sizeof(Renderer::FLight);
            SunDesc.initialState = nvrhi::ResourceStates::ShaderResource;
            SunDesc.keepInitialState = true;
            SunDesc.debugName = "SunLightBuffer";
            SunLightBuffer = NvrhiDevice->createBuffer(SunDesc);
            InitCmd->writeBuffer(SunLightBuffer, &SunLight, sizeof(SunLight));
        }

        InitCmd->close();
        NvrhiDevice->executeCommandList(InitCmd);
        NvrhiDevice->waitForIdle();

        HLVM_LOG(LogTest, info, TXT("GBuffer VB/IB built: {} verts, {} indices"),
            static_cast<uint32_t>(AllGBufferVerts.size()),
            static_cast<uint32_t>(AllGBufferIndices.size()));
        return true;
    }

    // ---- GBuffer raster pass (per-frame) -----------------------------------
    // Issues N drawIndexed calls — one per FStaticMesh — to populate the
    // GBuffer MRTs. Each draw binds:
    //   - GBufferVertexBuffer (FVertex 44B) with offset=0
    //   - GBufferIndexBuffer with offset=0
    //   - ViewConstantsBuffer (b0) — same per-frame matrix for all draws
    //   - GBufferPerInstanceCB (b1) — updated per-draw with that mesh's FInstanceInfo
    //
    // After the loop, transitions the 3 GBuffer MRTs to ShaderResource so
    // FGIPass's Texture2D<float4> SRV reads find the correct layout.
        // v197 (card K): takes NO extent parameters. It rasterises into
    // GBufferFrameBuffer, whose MRTs come from `const uint32_t W = WIDTH` and
    // are never recreated on resize — so the extent belongs to the target, not
    // the caller. v195 pinned the viewport below to WIDTH/HEIGHT but left the
    // parameters in place as `/*W*/`, `/*H*/`; the call site then still read as
    // though the raster extent followed the swapchain, and un-commenting either
    // one would have silently restored the v195 defect in the pass v195 fixed.
    // No query shape flags that: a FB.width sweep sees a plausible hit, a
    // signature sweep sees inert parameters. Dropping them makes it structural.
    void RenderGBuffer()
    {
        if (!GBufferPipeline || !GBufferFrameBuffer) return;
        if (!Scene) return;

        // v212 (real-time pass): per-mesh FInstanceInfo + binding-set cache
        // (member — a static here outlived the device and tripped
        // VUID-vkDestroyDevice-device-05137 at teardown). The old loop
        // re-derived the info with an O(N^2) mesh scan and called
        // NvrhiDevice->createBindingSet once PER MESH PER FRAME — the measured
        // GBuffer phase was ~18ms of the ~31ms frame. The binding set only
        // changes when the async-loaded albedo texture handle appears.

        // Clear the 4 GBuffer MRTs BEFORE the raster draws (fixed 2026-08-09).
        // Without this, un-rasterized sky texels keep uninitialized GPU memory
        // (NaN bit patterns in RGBA32F), which ReBLUR's neighbor gather then
        // turned into NaN weights → the blur fell back to pass-through. The
        // clears define sky as: worldpos=(0,0,0,0), normal=(0,0,0,1) (decodes
        // to a finite -1,-1,-1), material=0, linear depth=0.
        nvrhi::Color ClearBlack(0.f, 0.f, 0.f, 0.f);
        nvrhi::Color ClearNormal(0.f, 0.f, 0.f, 1.f);
        nvrhi::utils::ClearColorAttachment(CommandList, GBufferFrameBuffer, 0, ClearBlack);   // WorldPos
        nvrhi::utils::ClearColorAttachment(CommandList, GBufferFrameBuffer, 1, ClearNormal);  // Normal
        nvrhi::utils::ClearColorAttachment(CommandList, GBufferFrameBuffer, 2, ClearBlack);   // Material
        nvrhi::utils::ClearColorAttachment(CommandList, GBufferFrameBuffer, 3, ClearBlack);   // LinearDepth
        nvrhi::utils::ClearDepthStencilAttachment(CommandList, GBufferFrameBuffer, 1.0f, 0u); // Depth

        // (a) Constant-sentinel writes BEFORE the GBuffer pass. Any pixel the
        // raster pass does not write keeps the sentinel; any pixel it DOES
        // write gets the real Sponza value. The downstream dump therefore
        // proves "did the pass touch this pixel?" — sentinel still visible
        // means no rasterization at that pixel.
        //
        // REMOVED (2026-07-25): WriteGBufferSentinels was identified as the
        // root cause of the uniform-magenta GI symptom. The sentinel write
        // left the GBuffer textures in a state where the FGIPass SRV reads
        // returned the sentinel value rather than the post-raster pixel
        // data. With the dump normalization fix (commit 2fab7d6) the
        // post-raster Sponza geometry is visible in the dumps directly, so
        // the diagnostic purpose of the sentinels is no longer needed. The
        // original "without sentinels the GPU work stays in cache" symptom
        // (from commit t_139c4e41) was a separate issue fixed by the
        // CommandList isolation in commit 9a09df2 (bug-088).

        // The textures were created in RenderTarget initial state; transitions
        // for the FB-attached draws are emitted implicitly by the first draw.
        // After the draws we must move them back to ShaderResource for FGIPass.
        size_t MeshCount = 0;
        static bool bDebugLogged = false;
        for (auto& Entry : Scene->MeshTree)
        {
            auto StaticMesh = std::dynamic_pointer_cast<FStaticMesh>(Entry.second);
            if (!StaticMesh) continue;
            const auto& V = StaticMesh->GetVertices();
            const auto& I = StaticMesh->GetIndices();
            if (V.empty() || I.empty()) continue;

            if (!bDebugLogged && FrameCount == 1)
            {
                bDebugLogged = true;
                HLVM_LOG(LogTest, info, TXT("RenderGBuffer debug: mesh '{}' has {} verts, {} indices, first V pos=({},{},{}) first I={}"),
                    Entry.second->GetName(),
                    static_cast<uint32_t>(V.size()), static_cast<uint32_t>(I.size()),
                    V[0].Position.x, V[0].Position.y, V[0].Position.z,
                    I.size() > 0 ? I[0] : 0u);
                if (GBufferVertexBuffer && GBufferIndexBuffer)
                {
                    nvrhi::BufferDesc VbDesc = GBufferVertexBuffer->getDesc();
                    nvrhi::BufferDesc IbDesc = GBufferIndexBuffer->getDesc();
                    HLVM_LOG(LogTest, info, TXT("  GBuffer VB byteSize={} IB byteSize={}"),
                        VbDesc.byteSize, IbDesc.byteSize);
                }
            }

            // Locate matching FInstanceInfo by mesh pointer — same loop order
            // as LoadSponza uses.
            auto& Cache = GBufferDrawCache[StaticMesh.get()];
            if (Cache.Info.VertexCount == 0)
            {
                // First encounter: derive the info once (O(N^2) at init only).
                FInstanceInfo ThisInfo{};
                bool Found = false;
                {
                    uint32_t VOff = 0, IOff = 0;
                    for (auto& E2 : Scene->MeshTree)
                    {
                        auto M2 = std::dynamic_pointer_cast<FStaticMesh>(E2.second);
                        if (!M2) continue;
                        if (M2 == StaticMesh)
                        {
                            ThisInfo.VertexOffset = VOff;
                            ThisInfo.IndexOffset  = IOff;
                            ThisInfo.VertexCount  = static_cast<uint32_t>(M2->GetVertices().size());
                            ThisInfo.IndexCount   = static_cast<uint32_t>(M2->GetIndices().size());
                            ThisInfo.AlbedoTextureIndex = 0;
                            ThisInfo.Roughness = 0.9f;
                            ThisInfo.Metallic  = 0.0f;
                            // Pull material color if available
                            auto MatIt = Scene->MeshMultiMaterialMap.find(E2.second);
                            if (MatIt != Scene->MeshMultiMaterialMap.end() && !MatIt->second.empty())
                            {
                                const auto& M = MatIt->second[0];
                                const FVec3 A = GetMeshAlbedo(E2.second->GetName(), M->AlbedoColor);
                                ThisInfo.AlbedoColor[0] = A.x;
                                ThisInfo.AlbedoColor[1] = A.y;
                                ThisInfo.AlbedoColor[2] = A.z;
                                // Mirror LoadSponza's textured-mesh flag.
                                if (auto PBRMat = std::dynamic_pointer_cast<FPBRMaterial>(M))
                                {
                                    if (PBRMat->HasTexture(IMaterial::ETextureType::Albedo))
                                        ThisInfo.MaterialFlags |= 1u;
                                    ThisInfo.Roughness = PBRMat->GetRoughness();
                                    ThisInfo.Metallic  = PBRMat->GetMetallic();
                                }
                            }
                            else
                            {
                                const FVec3 A = GetMeshAlbedo(E2.second->GetName(), FVec3(0.7f, 0.7f, 0.7f));
                                ThisInfo.AlbedoColor[0] = A.x;
                                ThisInfo.AlbedoColor[1] = A.y;
                                ThisInfo.AlbedoColor[2] = A.z;
                            }
                            Found = true;
                            break;
                        }
                        VOff += static_cast<uint32_t>(M2->GetVertices().size());
                        IOff += static_cast<uint32_t>(M2->GetIndices().size());
                    }
                }
                if (!Found)
                    continue;
                Cache.Info = ThisInfo;
            }
            const FInstanceInfo& ThisInfo = Cache.Info;

            // Upload this mesh's FInstanceInfo to the per-instance CB.
            CommandList->writeBuffer(GBufferPerInstanceCB, &ThisInfo, sizeof(ThisInfo));

            // 2026-08-10 Phase 1: bind this mesh's real Sponza albedo texture
            // (loaded by FAsyncTextureLoader at Initialize). Falls back to the
            // white placeholder when the mesh has no texture / failed to load.
            nvrhi::TextureHandle AlbedoTex = PlaceholderTexture;
            {
                auto TexIt = Scene->MeshMultiMaterialMap.find(StaticMesh);
                if (TexIt != Scene->MeshMultiMaterialMap.end() && !TexIt->second.empty())
                {
                    if (auto PBRMat = std::dynamic_pointer_cast<FPBRMaterial>(TexIt->second[0]))
                    {
                        if (PBRMat->HasGPUTexture(IMaterial::ETextureType::Albedo))
                            AlbedoTex = PBRMat->GetGPUTexture(IMaterial::ETextureType::Albedo).GetTextureHandle();
                    }
                }
            }

            // v212: reuse the cached binding set unless the async-loaded
            // albedo texture handle changed (happens only during startup).
            if (!Cache.BS || Cache.Tex != AlbedoTex)
            {
                FBindingSetBuilder SetBuilder;
                SetBuilder.SetConstantBuffer(0, ViewConstantsBuffer)
                          .SetConstantBuffer(1, GBufferPerInstanceCB)
                          .SetTextureSRV(0, AlbedoTex)
                          .SetSampler(0, LinearSampler);
                Cache.BS = NvrhiDevice->createBindingSet(
                    SetBuilder.Build(), GBufferBindingLayout);
                Cache.Tex = AlbedoTex;
            }
            nvrhi::BindingSetHandle BS = Cache.BS;

            nvrhi::GraphicsState State;
            State.setPipeline(GBufferPipeline);
            State.setFramebuffer(GBufferFrameBuffer);
            State.addBindingSet(BS);

            nvrhi::VertexBufferBinding VBB;
            VBB.setBuffer(GBufferVertexBuffer);
            VBB.setSlot(0);
            VBB.setOffset(0);
            State.addVertexBuffer(VBB);

            nvrhi::IndexBufferBinding IBB;
            IBB.setBuffer(GBufferIndexBuffer);
            IBB.setOffset(0);
            IBB.setFormat(nvrhi::Format::R32_UINT);
            State.setIndexBuffer(IBB);

            // v195: WIDTH/HEIGHT, NOT LastWidth/LastHeight. This viewport
            // rasterises into GBufferFrameBuffer, whose MRTs are fixed-size
            // (created from `const uint32_t W = WIDTH` and never recreated on
            // resize). LastWidth/LastHeight are assigned from the swapchain, so
            // a resize pointed a wrong-sized viewport at a fixed-size target:
            // widening clips geometry away at the MRT edge, narrowing leaves
            // the outer region unrasterised and holding its cleared value —
            // which the GI pass then reads as "no geometry" and shades as sky.
            // NOTE: LastWidth/LastHeight themselves are deliberately left
            // swapchain-derived. Their other role is resize DETECTION at the
            // top of Render(), where `FB.width != LastWidth` gates
            // BindingCache.Clear(). Substituting the variable rather than this
            // use of it would make that comparison `WIDTH != WIDTH`, never
            // fire, and leave the binding cache stale across a resize.
            nvrhi::Viewport Vp(0.f, float(WIDTH), 0.f, float(HEIGHT), 0.f, 1.f);
            State.viewport.addViewportAndScissorRect(Vp);

            CommandList->setGraphicsState(State);

            nvrhi::DrawArguments Args;
            Args.vertexCount         = ThisInfo.IndexCount;
            Args.instanceCount       = 1;
            Args.startIndexLocation  = ThisInfo.IndexOffset;
            Args.startVertexLocation = ThisInfo.VertexOffset;
            Args.startInstanceLocation = 0;
            CommandList->drawIndexed(Args);
            if (!bDebugLogged && FrameCount == 1 && MeshCount == 1)
            {
                bDebugLogged = true;
                HLVM_LOG(LogTest, info, TXT("After drawIndexed[0]: VS={} PS={} PL={} IL={} BS={} FB={} VB={} IB={}"),
                    GBufferVS ? 1 : 0, GBufferPS ? 1 : 0, GBufferPipeline ? 1 : 0,
                    GBufferInputLayout ? 1 : 0, BS ? 1 : 0,
                    GBufferFrameBuffer ? 1 : 0, GBufferVertexBuffer ? 1 : 0,
                    GBufferIndexBuffer ? 1 : 0);
            }

            ++MeshCount;
        }

        // After all GBuffer draws, transition MRTs to ShaderResource for
        // FGIPass's SRV reads. nvrhi Vulkan wants
        // SHADER_READ_ONLY_OPTIMAL for Texture2D<float4> loads.
        CommandList->setTextureState(GBufferWorldPos, nvrhi::AllSubresources,
            nvrhi::ResourceStates::ShaderResource);
        CommandList->setTextureState(GBufferNormal,   nvrhi::AllSubresources,
            nvrhi::ResourceStates::ShaderResource);
        CommandList->setTextureState(GBufferMaterial, nvrhi::AllSubresources,
            nvrhi::ResourceStates::ShaderResource);
        CommandList->setTextureState(LinearDepthTexture, nvrhi::AllSubresources,
            nvrhi::ResourceStates::ShaderResource);

        // v128 (six-role-pipeline, tick 113, 2026-07-30): handle-identity probe.
        // Log the texture handles the GBuffer raster pass just transitioned.
        // Compare with FGIPass::DispatchRays's log line at FGIPass.cpp:533 to
        // discriminate "handles differ between passes" (binding issue) vs
        // "handles match but binding is wrong at descriptor level". Frame-rate
        // gated to once-per-N-frames to avoid log spam; FrameCount < 4 already
        // triggers the unconditional log below.
        if (FrameCount < 4 || FrameCount % 120 == 0)
        {
            HLVM_LOG(LogTest, info, TXT("[handle-id] RenderGBuffer: GBufferMaterial={:#x} WorldPos={:#x} Normal={:#x}"),
                reinterpret_cast<uintptr_t>(GBufferMaterial.Get()), reinterpret_cast<uintptr_t>(GBufferWorldPos.Get()), reinterpret_cast<uintptr_t>(GBufferNormal.Get()));
        }

        // NOTE (v5 — six-role-pipeline): the prior v1 HLVM-bypass
        // `close+execute+waitForIdle+open` block was the regression
        // (gi_raw became 0,0,0 after it landed). The 2026-07-25 working
        // shape had RenderGBuffer just leave the CommandList open so the
        // post-raster work (FGIPass/bilateral/ReSTIR/accumulate/blit)
        // appends into the same submission. End-of-Render
        // `executeCommandList` at line 691 then submits the whole frame.
        // Do NOT add a mid-frame execute here.

        if (MeshCount == 0)
        {
            HLVM_LOG(LogTest, warn, TXT("RenderGBuffer: 0 meshes drawn (frame {})"), FrameCount);
        }
        else if (FrameCount < 4 || FrameCount % 120 == 0)
        {
            // v197: WIDTH/HEIGHT, not LastWidth/LastHeight — this reports the
            // viewport set above, which v195 pinned to the fixed extent while
            // leaving this log on the swapchain one. Equal at 800x600, so the
            // line was accidentally truthful; on resize it would have lied
            // exactly when consulted.
            HLVM_LOG(LogTest, info, TXT("RenderGBuffer frame {}: drew {} meshes, viewport {}x{}"),
                FrameCount, MeshCount, WIDTH, HEIGHT);
        }
    }

    // ---- Previous-frame GBuffer snapshot (v210) ---------------------------
    // Copies the freshly rasterized GBuffer into the PREV textures so the
    // temporal pass can validate/reuse LAST frame's surfaces at the
    // reprojected pixels. The old aliases (PrevDepth = LinearDepth etc.) were
    // same-frame reads, which under the turntable always rejected candidates.
    void CopyGBufferToPrev()
    {
        if (!CommandList) return;
        if (!GBufferWorldPos || !PrevGBufferWorldPos) return;

        auto CopyOne = [&](nvrhi::TextureHandle Src, nvrhi::TextureHandle Dst)
        {
            CommandList->setTextureState(Src, nvrhi::AllSubresources,
                nvrhi::ResourceStates::CopySource);
            CommandList->setTextureState(Dst, nvrhi::AllSubresources,
                nvrhi::ResourceStates::CopyDest);
            CommandList->copyTexture(Dst, nvrhi::TextureSlice(), Src, nvrhi::TextureSlice());
        };

        CopyOne(GBufferWorldPos, PrevGBufferWorldPos);
        CopyOne(GBufferNormal, PrevGBufferNormal);
        CopyOne(GBufferMaterial, PrevGBufferMaterial);
        CopyOne(LinearDepthTexture, PrevLinearDepth);
    }

    // ---- View constants ----------------------------------------------------
    void UpdateViewConstants(uint32_t W, uint32_t H)
    {
        glm::vec3 CamPos    = GetCameraPos();
        glm::vec3 CamTarget = GetCameraTarget();
        glm::vec3 CamUp     = GetCameraUp();
        float Fov = GetCameraFovDeg();

        // Sponza vertices live in the original GLTF coord space (±hundreds).
        // Apply the 0.01 scale used by the TLAS so rasterized worldPos matches
        // what FGIPass sees, and the same 90° Y-rotation as the TLAS instances
        // (2026-08-10). Without this, all vertices are clipped by the far
        // plane (vertices are at distance ~500 from the camera at z=8).
        // Same Y-rotation as the per-frame TLAS rebuild (turntable).
        const glm::mat4 SceneRot = glm::rotate(
            glm::mat4(1.0f), glm::radians(SceneRotationDeg), glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 Model = SceneRot * glm::scale(glm::mat4(1.0f), glm::vec3(0.01f));
        glm::mat4 View  = glm::lookAt(CamPos, CamTarget, CamUp);
        // Use RH-ZO perspective WITHOUT Z flip. glm::perspective already
        // produces [-1, 1] Z (OpenGL convention). Don't flip — flipping moves
        // geometry away from the viewer when the view matrix expects -Z forward.
        glm::mat4 Proj  = glm::perspective(
            glm::radians(Fov), float(W) / float(H), 0.001f, 50.0f);

        // Remember view-proj for the ReSTIR temporal pass (real reprojection
        // matrices — the old identity hardcode made temporal reuse a no-op).
        PrevViewProj = CurrViewProj;
        CurrViewProj = Proj * View;

        struct FVC { glm::mat4 Model; glm::mat4 View; glm::mat4 Proj; glm::vec2 Size; float FrameIndex; float Pad; };
        FVC VC{Model, View, Proj, {float(W), float(H)}, float(AccumFrameCount), 0.0f};
        CommandList->writeBuffer(ViewConstantsBuffer, &VC, sizeof(VC));
    }

    // ---- GI accumulate pass ------------------------------------------------
    bool CreateAccumulationPipeline(const FString& DataDir)
    {
        const std::string SblobPath = FPath::Combine(DataDir, TXT("GIAccumulate_cs.sblob")).string();
        auto Blob = ReadBinaryFile(SblobPath);
        if (Blob.empty())
        {
            HLVM_LOG(LogTest, err, TXT("Failed to read GIAccumulate_cs.sblob at {}"), *FString(SblobPath.c_str()));
            return false;
        }
        const void* Bin = nullptr; size_t BinSize = 0;
        if (!ShaderMake::FindPermutationInBlob(Blob.data(), Blob.size(), nullptr, 0, &Bin, &BinSize))
        {
            HLVM_LOG(LogTest, err, TXT("Failed to extract GIAccumulate_cs compute shader"));
            return false;
        }
        nvrhi::ShaderDesc CSDesc;
        CSDesc.setShaderType(nvrhi::ShaderType::Compute);
        AccumulateCS = NvrhiDevice->createShader(CSDesc, Bin, BinSize);
        if (!AccumulateCS) return false;

        FBindingLayoutBuilder BLB;
        BLB.SetVisibility(nvrhi::ShaderType::Compute)
           .AddConstantBuffer(0)
           .AddTextureSRV(0)
           .AddTextureSRV(1)   // v210: DirectTexture combine
           .AddTextureUAV(0)
           .AddTextureUAV(1);

        // AGENTS.md gotcha: NVRHI defaults `constantBufferOffset` to 256; the
        // shader's b0 must end up at Vulkan binding 0. Set offsets explicitly.
        nvrhi::VulkanBindingOffsets Offsets;
        Offsets.setConstantBufferOffset(0)
               .setShaderResourceOffset(0)
               .setSamplerOffset(0)
               .setUnorderedAccessViewOffset(0);
        BLB.SetBindingOffsets(Offsets);

        AccumulateBindingLayout = NvrhiDevice->createBindingLayout(BLB.Build());

        nvrhi::ComputePipelineDesc CPDesc;
        CPDesc.setComputeShader(AccumulateCS)
              .addBindingLayout(AccumulateBindingLayout);
        AccumulatePipeline = NvrhiDevice->createComputePipeline(CPDesc);
        if (!AccumulatePipeline) return false;

        nvrhi::BufferDesc CD;
        CD.byteSize = 16;   // 4 constants (uint, uint, uint, float)
        CD.isConstantBuffer = true;
        CD.initialState = nvrhi::ResourceStates::ConstantBuffer;
        CD.keepInitialState = true;
        CD.debugName = "GIAccumulateConstants";
        AccumulateConstants = NvrhiDevice->createBuffer(CD);
        return true;
    }

    // Phase D: half-res -> full-res depth/normal-weighted upscale.
    bool CreateResolvePipeline(const FString& DataDir)
    {
        const std::string SblobPath = FPath::Combine(DataDir, TXT("Resolve_cs.sblob")).string();
        auto Blob = ReadBinaryFile(SblobPath);
        if (Blob.empty())
        {
            HLVM_LOG(LogTest, err, TXT("Failed to read Resolve_cs.sblob at {}"), *FString(SblobPath.c_str()));
            return false;
        }
        const void* Bin = nullptr; size_t BinSize = 0;
        if (!ShaderMake::FindPermutationInBlob(Blob.data(), Blob.size(), nullptr, 0, &Bin, &BinSize))
        {
            HLVM_LOG(LogTest, err, TXT("Failed to extract Resolve_cs compute shader"));
            return false;
        }
        nvrhi::ShaderDesc CSDesc;
        CSDesc.setShaderType(nvrhi::ShaderType::Compute);
        nvrhi::ShaderHandle ResolveCS = NvrhiDevice->createShader(CSDesc, Bin, BinSize);
        if (!ResolveCS) return false;

        FBindingLayoutBuilder BLB;
        BLB.SetVisibility(nvrhi::ShaderType::Compute)
           .AddConstantBuffer(0)
           .AddTextureSRV(0)   // t0 half-res radiance
           .AddTextureSRV(1)   // t1 full-res depth
           .AddTextureSRV(2)   // t2 full-res normal
           .AddTextureUAV(0);  // u0 full-res output
        nvrhi::VulkanBindingOffsets Offsets;
        Offsets.setConstantBufferOffset(0)
               .setShaderResourceOffset(0)
               .setSamplerOffset(0)
               .setUnorderedAccessViewOffset(0);
        BLB.SetBindingOffsets(Offsets);
        ResolveBindingLayout = NvrhiDevice->createBindingLayout(BLB.Build());

        nvrhi::ComputePipelineDesc CPDesc;
        CPDesc.setComputeShader(ResolveCS)
              .addBindingLayout(ResolveBindingLayout);
        ResolvePipeline = NvrhiDevice->createComputePipeline(CPDesc);
        if (!ResolvePipeline) return false;

        nvrhi::BufferDesc CD;
        CD.byteSize = 32;
        CD.isConstantBuffer = true;
        CD.initialState = nvrhi::ResourceStates::ConstantBuffer;
        CD.keepInitialState = true;
        CD.debugName = "ResolveConstants";
        ResolveConstantsBuffer = NvrhiDevice->createBuffer(CD);
        return ResolveConstantsBuffer != nullptr;
    }

    // ---- Dump last-frame textures -----------------------------------------
    // Reads back the displayed final frame to CPU, then writes to PNG via
    // FImageDump::DumpToPNG. Following the pattern in TestPathTraceGI.
    void DumpCurrentFrame()
    {
        const FPath DumpDir = FPath(FString::Format(
            TXT("{}/Engine/Source/Runtime/Test/{}_Data/dumps"),
            *GProjectRoot, *GExecutableName));
        std::string dir = DumpDir.string();
        std::filesystem::create_directories(dir);

        // DIAGNOSTIC (2026-08-10): read back the swapchain back-buffer after the
        // frame CL executed — proves whether the blit actually wrote the image
        // that gets presented (the visible window otherwise shows black).
        // Gate: HLVM_DUMP_SWAPCHAIN=1.
        if (std::getenv("HLVM_DUMP_SWAPCHAIN") && CurrentBackBufferTexture)
        {
            // The swapchain back-buffer is 8-bit (B8G8R8A8); read it with a
            // matching staging texture and average the bytes per channel.
            const nvrhi::TextureDesc TexDesc = CurrentBackBufferTexture->getDesc();
            nvrhi::TextureDesc StageDesc = TexDesc;
            StageDesc.isRenderTarget = false;
            StageDesc.isUAV = false;
            StageDesc.isTypeless = false;
            StageDesc.initialState = nvrhi::ResourceStates::CopyDest;
            StageDesc.keepInitialState = false;
            StageDesc.debugName = "SwapchainReadbackStaging";
            nvrhi::StagingTextureHandle Staging = NvrhiDevice->createStagingTexture(
                StageDesc, nvrhi::CpuAccessMode::Read);
            if (Staging)
            {
                nvrhi::CommandListHandle Cmd = NvrhiDevice->createCommandList();
                Cmd->open();
                Cmd->setTextureState(CurrentBackBufferTexture, nvrhi::AllSubresources,
                    nvrhi::ResourceStates::CopySource);
                nvrhi::TextureSlice Slice;
                Slice.width = TexDesc.width;
                Slice.height = TexDesc.height;
                Slice.depth = 1;
                Cmd->copyTexture(Staging.Get(), Slice, CurrentBackBufferTexture, Slice);
                Cmd->close();
                NvrhiDevice->executeCommandList(Cmd);
                NvrhiDevice->waitForIdle();

                size_t RowPitch = 0;
                void* Mapped = NvrhiDevice->mapStagingTexture(
                    Staging.Get(), Slice, nvrhi::CpuAccessMode::Read, &RowPitch);
                if (Mapped)
                {
                    const bool bBGR = (TexDesc.format == nvrhi::Format::BGRA8_UNORM ||
                                       TexDesc.format == nvrhi::Format::SBGRA8_UNORM);
                    double R = 0, G = 0, B = 0;
                    const uint8_t* SrcRow = static_cast<const uint8_t*>(Mapped);
                    const size_t NPix = static_cast<size_t>(TexDesc.width) * TexDesc.height;
                    for (uint32_t y = 0; y < TexDesc.height; ++y)
                    {
                        const uint8_t* Src = SrcRow + static_cast<size_t>(y) * RowPitch;
                        for (uint32_t x = 0; x < TexDesc.width; ++x)
                        {
                            const uint8_t C0 = Src[x*4 + 0];
                            const uint8_t C1 = Src[x*4 + 1];
                            const uint8_t C2 = Src[x*4 + 2];
                            R += bBGR ? C2 : C0;
                            G += C1;
                            B += bBGR ? C0 : C2;
                        }
                    }
                    HLVM_LOG(LogTest, info, TXT("SWAPCHAIN readback (8-bit): mean=({:.1f},{:.1f},{:.1f}) fmt={}"),
                        R / static_cast<double>(NPix), G / static_cast<double>(NPix), B / static_cast<double>(NPix),
                        static_cast<int>(TexDesc.format));
                }
                NvrhiDevice->unmapStagingTexture(Staging.Get());
            }
        }

        // Final-image dump is what validate_restir_gi.py actually inspects.
        DumpRGBA32FTexture(DisplayTexture, TXT("display"), dir);
        DumpRGBA32FTexture(FullResSpatial, TXT("spatial"), dir);
        DumpRGBA32FTexture(DenoisedTexture, TXT("denoised"), dir);
        // bug-075 followup: gi_raw is HDR (radiance * exposure); per-channel
        // normalization surfaces the real distribution even when values are
        // small (e.g. (0.9, 0.9, 0.97)) which would otherwise dump as nearly
        // uniform (0.9*255=229, almost the same color).
        DumpRGBA32FTexture(FullResGIRaw, TXT("gi_raw"), dir, /*bNormalizePerChannel=*/true);
        // GBuffer channel dumps (per-frame sentinel + post-pass values).
        // Same HLVM_DUMP_RGI gate; same dir; same naming convention
        // (timestamp_channel_frameN.png) so the validator can consume them
        // unchanged. Sentinel pixels remaining in these dumps mark pixels the
        // raster pass did not write. WorldPos is normalized per-channel so
        // world-space positions (which fall outside [0,1]) are visible in the
        // PNG; normal and material are already in [0,1] and stay unnormalized.
        DumpRGBA32FTexture(GBufferWorldPos, TXT("gbuffer_worldpos"), dir, /*bNormalizePerChannel=*/true);
        DumpRGBA32FTexture(GBufferNormal,   TXT("gbuffer_normal"),   dir);
        DumpRGBA32FTexture(GBufferMaterial, TXT("gbuffer_material"), dir);
        DumpRGBA32FTexture(LinearDepthTexture, TXT("gbuffer_depth"), dir);
        HLVM_LOG(LogTest, info, TXT("Dumped frames to {}"), *FString(dir));
    }

    // CPU readback of one RGBA32F texture into a float RGBA vector.
    // Creates a one-shot staging texture, copies, maps, unmaps.
    bool ReadbackTextureFloats(nvrhi::TextureHandle Texture, std::vector<float>& OutPixels)
    {
        if (!Texture || !NvrhiDevice)
            return false;
        // Phase D: use the texture's real size (ReSTIR reservoirs/traces are
        // half-res now; the old hardcoded WIDTH×HEIGHT readback hung on them).
        const nvrhi::TextureDesc TexDesc = Texture->getDesc();
        const uint32_t TW = TexDesc.width;
        const uint32_t TH = TexDesc.height;

        nvrhi::TextureDesc StagingDesc;
        StagingDesc.dimension = nvrhi::TextureDimension::Texture2D;
        StagingDesc.width     = TW;
        StagingDesc.height    = TH;
        StagingDesc.format    = nvrhi::Format::RGBA32_FLOAT;
        StagingDesc.isRenderTarget = false;
        StagingDesc.isUAV     = false;
        StagingDesc.isTypeless = false;
        StagingDesc.initialState = nvrhi::ResourceStates::CopyDest;
        StagingDesc.keepInitialState = false;
        StagingDesc.debugName = "ReadbackStaging";
        nvrhi::StagingTextureHandle Staging = NvrhiDevice->createStagingTexture(
            StagingDesc, nvrhi::CpuAccessMode::Read);
        if (!Staging)
            return false;

        nvrhi::CommandListHandle Cmd = NvrhiDevice->createCommandList();
        Cmd->open();
        Cmd->setTextureState(Texture, nvrhi::AllSubresources, nvrhi::ResourceStates::CopySource);
        nvrhi::TextureSlice Slice;
        Slice.width  = TW;
        Slice.height = TH;
        Slice.depth  = 1;
        Cmd->copyTexture(Staging.Get(), Slice, Texture.Get(), Slice);
        Cmd->close();
        NvrhiDevice->executeCommandList(Cmd);
        NvrhiDevice->waitForIdle();

        size_t RowPitch = 0;
        void* Mapped = NvrhiDevice->mapStagingTexture(
            Staging.Get(), Slice, nvrhi::CpuAccessMode::Read, &RowPitch);
        if (!Mapped)
            return false;

        OutPixels.assign(static_cast<size_t>(TW) * TH * 4, 0.0f);
        const uint8_t* SrcRow = reinterpret_cast<const uint8_t*>(Mapped);
        for (uint32_t y = 0; y < TH; ++y)
        {
            const float* Src = reinterpret_cast<const float*>(SrcRow + static_cast<size_t>(y) * RowPitch);
            for (uint32_t x = 0; x < TW; ++x)
            {
                size_t SrcIdx = x * 4;
                size_t DstIdx = (static_cast<size_t>(y) * TW + x) * 4;
                OutPixels[DstIdx + 0] = Src[SrcIdx + 0];
                OutPixels[DstIdx + 1] = Src[SrcIdx + 1];
                OutPixels[DstIdx + 2] = Src[SrcIdx + 2];
                // 2026-08-10: copy the real alpha instead of forcing 1.0.
                // The old hardcode hid the material roughness (GBufferMaterial.a)
                // and made the validator's alpha-sentinel check trivially pass
                // even when the dispatch never ran.
                OutPixels[DstIdx + 3] = Src[SrcIdx + 3];
            }
        }
        NvrhiDevice->unmapStagingTexture(Staging.Get());
        return true;
    }

    // Per-channel min/max/mean/std of a float RGBA buffer — the only
    // unambiguous way to validate HDR values (byte-clamped PNGs saturate
    // >= 1.0, hiding e.g. reservoir M up to MaxM=30).
    void LogFloatStats(const FString& Name, const std::vector<float>& Pixels)
    {
        float MinC[3] = {1e30f, 1e30f, 1e30f}, MaxC[3] = {-1e30f, -1e30f, -1e30f};
        double SumC[3] = {0.0, 0.0, 0.0}, SumSq[3] = {0.0, 0.0, 0.0};
        const size_t NPix = Pixels.size() / 4;
        for (size_t i = 0; i < NPix; ++i)
        {
            for (size_t c = 0; c < 3; ++c)
            {
                const float v = Pixels[i*4 + c];
                MinC[c] = std::min(MinC[c], v);
                MaxC[c] = std::max(MaxC[c], v);
                SumC[c] += static_cast<double>(v);
                SumSq[c] += static_cast<double>(v) * static_cast<double>(v);
            }
        }
        HLVM_LOG(LogTest, info, TXT("stats {} floats: R[{:.4f},{:.4f}] G[{:.4f},{:.4f}] B[{:.4f},{:.4f}] mean=[{:.4f},{:.4f},{:.4f}] std=[{:.4f},{:.4f},{:.4f}]"),
            *Name, MinC[0], MaxC[0], MinC[1], MaxC[1], MinC[2], MaxC[2],
            SumC[0]/static_cast<double>(NPix), SumC[1]/static_cast<double>(NPix), SumC[2]/static_cast<double>(NPix),
            std::sqrt(std::max(0.0, SumSq[0]/static_cast<double>(NPix) - (SumC[0]/static_cast<double>(NPix))*(SumC[0]/static_cast<double>(NPix)))),
            std::sqrt(std::max(0.0, SumSq[1]/static_cast<double>(NPix) - (SumC[1]/static_cast<double>(NPix))*(SumC[1]/static_cast<double>(NPix)))),
            std::sqrt(std::max(0.0, SumSq[2]/static_cast<double>(NPix) - (SumC[2]/static_cast<double>(NPix))*(SumC[2]/static_cast<double>(NPix)))));
    }

    // End-of-run numerical summary, always logged (no env gate): readback of
    // the key pipeline outputs + temporal reservoirs, plus derived metrics:
    //   - reservoir M (mean/max across the active M/W pair) and W mean
    //   - spatial grayscale channel error over lit pixels
    // This makes the pipeline claims (M accumulation, W, grayscale, display
    // range) verifiable from a plain run's log without HLVM_DUMP_RGI.
    void LogFinalFrameStats()
    {
        if (AccumFrameCount < AccumTargetFrames || bFinalStatsLogged)
            return;
        bFinalStatsLogged = true;

        std::vector<float> Px;
        const struct { const char* Tag; nvrhi::TextureHandle Tex; } Items[] = {
            { "display",          DisplayTexture },
            { "spatial",          FullResSpatial },
            { "denoised",         DenoisedTexture },
            { "gi_raw",           FullResGIRaw },
            // v210: 3-texture reservoir (ZetaRay layout).
            //   R0 = pos + asfloat(ID); R1 = Lo + M; R2 = w_sum + W + normal.
            { "reservoir_posA",   TemporalReservoir0 },
            { "reservoir_LoM_A",  TemporalReservoir1 },
            { "reservoir_C_A",    TemporalReservoir4 },
            { "reservoir_posB",   TemporalReservoir2 },
            { "reservoir_LoM_B",  TemporalReservoir3 },
            { "reservoir_C_B",    TemporalReservoir5 },
        };
        for (const auto& It : Items)
        {
            if (It.Tex && ReadbackTextureFloats(It.Tex, Px))
                LogFloatStats(FString(It.Tag), Px);
        }

        // Derived: spatial grayscale channel error over lit pixels.
        float GrayErr = 0.0f;
        {
            float ErrSum = 0.0f;
            size_t ErrCount = 0;
            if (ReadbackTextureFloats(FullResSpatial, Px))
            {
                const size_t NPix = Px.size() / 4;
                for (size_t i = 0; i < NPix; ++i)
                {
                    const float R = Px[i*4 + 0];
                    const float B = Px[i*4 + 2];
                    if (R > 1e-3f)
                    {
                        ErrSum += std::abs(R - B) / R;
                        ++ErrCount;
                    }
                }
                if (ErrCount > 0)
                    GrayErr = ErrSum / static_cast<float>(ErrCount);
            }
        }

        // Derived: reservoir M/W (v210 layout) — M lives in the LoM texture's
        // alpha (R1.w), W in the C texture's green (R2.y). The active pair is
        // the one with the larger M sum; mean/max across both pairs.
        float MMax = 0.0f, MSum = 0.0f, WMean = 0.0f;
        const nvrhi::TextureHandle LoMTexs[2] = { TemporalReservoir1, TemporalReservoir3 };
        const nvrhi::TextureHandle CTexs[2]   = { TemporalReservoir4, TemporalReservoir5 };
        {
            float BestPairMSum = -1.0f;
            size_t NPixPerTex = 0;
            for (int p = 0; p < 2; ++p)
            {
                if (!LoMTexs[p] || !ReadbackTextureFloats(LoMTexs[p], Px))
                    continue;
                NPixPerTex = Px.size() / 4;
                float PairMSum = 0.0f, PairWSum = 0.0f;
                std::vector<float> CPx;
                const bool HaveC = CTexs[p] && ReadbackTextureFloats(CTexs[p], CPx);
                for (size_t i = 0; i < NPixPerTex; ++i)
                {
                    const float M = Px[i*4 + 3];   // R1.w
                    PairMSum += M;
                    if (HaveC)
                        PairWSum += CPx[i*4 + 1];  // R2.y
                    if (M > MMax) MMax = M;
                }
                MSum += PairMSum;
                if (PairMSum > BestPairMSum)
                {
                    BestPairMSum = PairMSum;
                    WMean = PairWSum / static_cast<float>(NPixPerTex);
                }
            }
            const float TotalPix = static_cast<float>(NPixPerTex * 2);
            const float MMean = TotalPix > 0.0f ? static_cast<float>(MSum) / TotalPix : 0.0f;
            HLVM_LOG(LogTest, info, TXT("ReSTIR summary: reservoir M mean={:.2f} max={:.1f} (MaxM=30) | W mean={:.3f} | spatial grayscale err={:.4f}"),
                MMean, MMax, WMean, GrayErr);
        }
    }

    // CPU-readback then PNG for one RGBA32F texture. Creates a one-shot
    // staging texture, copies, maps, runs the bytes through FImageDump::DumpToPNG.
    //
    // For textures with values outside [0,1] (e.g. world-space positions in
    // GBufferWorldPos), pass bNormalizePerChannel=true. The dumper computes
    // min/max across RGB channels and rescales to [0,1] before byte
    // encoding, so the visualization reflects relative variation instead of
    // clamping to 0 or 255.
    void DumpRGBA32FTexture(nvrhi::TextureHandle Texture, const FString& Name, const std::string& dir,
                            bool bNormalizePerChannel = false)
    {
        if (!Texture || !NvrhiDevice) return;

        std::vector<float> Pixels;
        if (!ReadbackTextureFloats(Texture, Pixels))
            return;
        const nvrhi::TextureDesc TexDesc = Texture->getDesc();
        const int TW = static_cast<int>(TexDesc.width);
        const int TH = static_cast<int>(TexDesc.height);

        const std::string Filename = dir + "/" + MakeTimestampPrefix() + "_" +
            std::string(Name.begin(), Name.end()) + "_frame" + std::to_string(AccumFrameCount) + ".png";

        if (bNormalizePerChannel)
        {
            // Per-channel min/max across all pixels; rescale RGB to [0,1].
            // Preserves relative structure (which is what you want for a
            // worldpos debug visualization) and avoids the clamp-to-extreme
            // behavior that makes non-normalized textures look like solid
            // quadrants.
            float MinR = Pixels[0], MaxR = Pixels[0];
            float MinG = Pixels[1], MaxG = Pixels[1];
            float MinB = Pixels[2], MaxB = Pixels[2];
            const size_t NPix = Pixels.size() / 4;
            for (size_t i = 0; i < NPix; ++i)
            {
                const float r = Pixels[i*4 + 0];
                const float g = Pixels[i*4 + 1];
                const float b = Pixels[i*4 + 2];
                if (r < MinR) MinR = r; if (r > MaxR) MaxR = r;
                if (g < MinG) MinG = g; if (g > MaxG) MaxG = g;
                if (b < MinB) MinB = b; if (b > MaxB) MaxB = b;
            }
            const float RangeR = (MaxR - MinR) > 1e-6f ? (MaxR - MinR) : 1.0f;
            const float RangeG = (MaxG - MinG) > 1e-6f ? (MaxG - MinG) : 1.0f;
            const float RangeB = (MaxB - MinB) > 1e-6f ? (MaxB - MinB) : 1.0f;
            for (size_t i = 0; i < NPix; ++i)
            {
                Pixels[i*4 + 0] = (Pixels[i*4 + 0] - MinR) / RangeR;
                Pixels[i*4 + 1] = (Pixels[i*4 + 1] - MinG) / RangeG;
                Pixels[i*4 + 2] = (Pixels[i*4 + 2] - MinB) / RangeB;
            }
            HLVM_LOG(LogTest, info,
                TXT("DumpRGBA32FTexture: {} normalized per-channel — R[{:.3f},{:.3f}] G[{:.3f},{:.3f}] B[{:.3f},{:.3f}]"),
                *Name, MinR, MaxR, MinG, MaxG, MinB, MaxB);
        }

        if (FImageDump::DumpToPNG(FString(Filename.c_str()), TW, TH, Pixels.data()))
        {
            HLVM_LOG(LogTest, info, TXT("Dumped {} ({})"), *Name, *FString(Filename));
        }
        // Raw float stats (the PNG is byte-clamped; values >= 1.0 saturate to
        // 255 and cannot be distinguished, e.g. reservoir M up to MaxM=30).
        LogFloatStats(Name, Pixels);
    }

private:
    // ---- Members -----------------------------------------------------------
    nvrhi::IDevice*              NvrhiDevice = nullptr;
    nvrhi::FramebufferInfo        FBInfo;
    FString                       WindowTitle;
    FBindingCache                 BindingCache;
    nvrhi::CommandListHandle      CommandList;

    nvrhi::SamplerHandle          LinearSampler;
    nvrhi::BufferHandle           ViewConstantsBuffer;

    // GBuffer + scene
    nvrhi::TextureHandle          GBufferWorldPos;
    nvrhi::TextureHandle          GBufferNormal;
    nvrhi::TextureHandle          GBufferMaterial;
    nvrhi::TextureHandle          GBufferDepth;
    nvrhi::TextureHandle          PrevGBufferWorldPos;   // v210: true prev-frame chain
    nvrhi::TextureHandle          PrevGBufferNormal;
    nvrhi::TextureHandle          PrevGBufferMaterial;
    nvrhi::TextureHandle          PrevLinearDepth;
    nvrhi::BufferHandle           SunLightBuffer;
    nvrhi::TextureHandle          PlaceholderTexture;
    std::vector<FInstanceInfo>    AllInstanceInfos;   // Phase 3: patched averages
    TVector<nvrhi::TextureHandle> MaterialTextures;   // Phase 3b: per-texel bounce albedo
    nvrhi::ITexture*              CurrentBackBufferTexture = nullptr; // swapchain diag
    uint32_t                      HalfResWidth = 0;    // Phase D
    uint32_t                      HalfResHeight = 0;
    nvrhi::TextureHandle          FullResGIRaw;        // Phase D: upscaled gi_raw dump
    nvrhi::TextureHandle          FullResSpatial;      // Phase D: upscaled spatial -> ReBLUR
    nvrhi::TextureHandle          FullResDirect;       // v210: upscaled DirectTexture -> display combine
    nvrhi::ComputePipelineHandle  ResolvePipeline;
    nvrhi::BindingLayoutHandle    ResolveBindingLayout;
    nvrhi::BufferHandle           ResolveConstantsBuffer;
    nvrhi::TextureHandle          LinearDepthTexture;
    nvrhi::BufferHandle           VertexBuffer;
    nvrhi::BufferHandle           IndexBuffer;
    nvrhi::BufferHandle           InstanceInfoBuffer;
    nvrhi::rt::AccelStructHandle  SceneTLAS;
    std::vector<nvrhi::rt::AccelStructHandle> SceneBLASes;
    std::shared_ptr<FScene3DNode> Scene;

    // Pipeline passes
    GI::FGIPass                   GIPass;
    FBilateralDenoisePass         BilateralDenoisePass;
    ReSTIR::FReSTIRPass           ReSTIRPass;
    ReBLUR::FReBLURPass           ReBLURPass;
    nvrhi::TextureHandle          ReBLURHistoryTexture[2];
    bool                          bReBLURInitialized = false;

    // Per-frame intermediate textures
    // v212: per-mesh GBuffer draw cache (FInstanceInfo + binding set).
    struct FMeshDrawCacheEntry { FInstanceInfo Info; nvrhi::TextureHandle Tex; nvrhi::BindingSetHandle BS; };
    phmap::node_hash_map<const FStaticMesh*, FMeshDrawCacheEntry> GBufferDrawCache;
    nvrhi::TextureHandle          OutputTexture;
    nvrhi::TextureHandle          DirectionTexture;
    nvrhi::TextureHandle          SampleInfoTexture;   // v210: x2 normal + pdf (FGIPass u3)
    nvrhi::TextureHandle          DirectTexture;       // v210: primary direct+ambient (FGIPass u4)
    nvrhi::TextureHandle          DenoisedTexture;
    nvrhi::TextureHandle          ReservoirTex0;
    nvrhi::TextureHandle          ReservoirTex1;
    nvrhi::TextureHandle          ReservoirTex2;       // v210: w_sum/W/OctEncode(normal)
    nvrhi::TextureHandle          TemporalReservoir0;
    nvrhi::TextureHandle          TemporalReservoir1;
    nvrhi::TextureHandle          TemporalReservoir2;
    nvrhi::TextureHandle          TemporalReservoir3;
    nvrhi::TextureHandle          TemporalReservoir4;  // v210: reservoir C ping-pong pair A
    nvrhi::TextureHandle          TemporalReservoir5;  // v210: reservoir C ping-pong pair B
    nvrhi::TextureHandle          SpatialRadiance;
    nvrhi::TextureHandle          AccumTexture;
    nvrhi::TextureHandle          DisplayTexture;
    // v213 (Phase 5b): GPU timer queries for per-phase GPU time.
    nvrhi::TimerQueryHandle       GpuTimers[6];   // 0=GBuffer 1=RayTrace 2=ReSTIR 3=Resolve 4=BlitAccum 5=Total
    bool                          bGpuTimers = false;
    bool                          bTimersPending = false;
    std::vector<double>           GpuTimerSums[6];

    // GIAccumulate pipeline
    nvrhi::ComputePipelineHandle  AccumulatePipeline;
    nvrhi::BindingLayoutHandle    AccumulateBindingLayout;
    nvrhi::ShaderHandle           AccumulateCS;
    nvrhi::BufferHandle           AccumulateConstants;

    // GBuffer PT pipeline (renders Sponza into GBufferWorldPos /
    // GBufferNormal / GBufferMaterial MRTs before FGIPass).
    nvrhi::ShaderHandle           GBufferVS;
    nvrhi::ShaderHandle           GBufferPS;
    nvrhi::InputLayoutHandle      GBufferInputLayout;
    nvrhi::BindingLayoutHandle    GBufferBindingLayout;
    nvrhi::GraphicsPipelineHandle GBufferPipeline;
    nvrhi::FramebufferHandle      GBufferFrameBuffer;
    nvrhi::BufferHandle           GBufferVertexBuffer;
    nvrhi::BufferHandle           GBufferIndexBuffer;
    nvrhi::BufferHandle           GBufferPerInstanceCB;

    // Frame counters / env
    uint32_t  LastWidth  = 0;
    uint32_t  LastHeight = 0;
    uint32_t  AccumFrameCount = 0;
    uint32_t  AccumTargetFrames = DEFAULT_ACCUM_TARGET_FRAMES;
    float     Exposure = 1.0f;
    bool      bDumpRequested = false;
    bool      bFinalStatsLogged = false;
    bool      bBypass = false;
    uint32_t  FrameCount = 0;
    float     FPSUpdateTimer = 0.0f;
    float     SceneRotationDeg = 90.0f;        // scene Y-rotation (turntable)
    float     PrevSceneRotationDeg = 90.0f;    // previous frame (Phase C reprojection)
    float     SceneRotationDegSpeed = 0.0f;    // degrees/second (0 = still)
    bool      bShowWindow = false;             // HLVM_SHOW_WINDOW=1
    // View-proj history for the ReSTIR temporal pass (real reprojection).
    glm::mat4 CurrViewProj = glm::mat4(1.0f);
    glm::mat4 PrevViewProj = glm::mat4(1.0f);
};

// ============================================================================
// Test entry
// ============================================================================

RECORD_BOOL(test_ReSTIR_GI_Temporal)
{
    HLVM_LOG(LogTest, info, TXT("Starting ReSTIR GI Temporal Test..."));

    try
    {
        IWindow::Properties WindowProps;
        WindowProps.Title    = WINDOW_TITLE;
        WindowProps.Extent   = { WIDTH, HEIGHT };
        WindowProps.Resizable = true;
        // Default: show the window (a real display must be present). Headless
        // CI can opt in to a minimized window with HLVM_RGI_MINIMIZED=1.
        WindowProps.StartMinimized = (std::getenv("HLVM_RGI_MINIMIZED") != nullptr);
        WindowProps.VSync    = IWindow::EVsync::Off;

        auto DeviceManager = FDeviceManager::Create(nvrhi::GraphicsAPI::VULKAN);
        if (!DeviceManager) throw std::runtime_error("Failed to create DeviceManager");

        // Match the proven TestPathTraceGI device setup: every option consumed
        // during instance/device creation must be configured before that call.
        FDeviceCreationParameters& DeviceParams = const_cast<FDeviceCreationParameters&>(
            DeviceManager->GetDeviceParams());
        DeviceParams.BackBufferWidth = WIDTH;
        DeviceParams.BackBufferHeight = HEIGHT;
        DeviceParams.SwapChainBufferCount = 2;
        DeviceParams.VSyncMode = 0;
        DeviceParams.bEnableDebugRuntime = true;
        DeviceParams.bEnableNVRHIValidationLayer = true;
        DeviceParams.bEnableRayTracingExtensions = true;

        if (!DeviceManager->CreateWindowDeviceAndSwapChain(WindowProps))
        {
            throw std::runtime_error("Failed to create window, device and swap chain");
        }
        HLVM_LOG(LogTest, info, TXT("Device created with ray tracing enabled"));

        nvrhi::IDevice* NvrhiDevice = DeviceManager->GetDevice();
        if (!NvrhiDevice->queryFeatureSupport(nvrhi::Feature::RayTracingPipeline))
        {
            throw std::runtime_error("Ray tracing pipeline not supported on this device");
        }

        nvrhi::IFramebuffer* FirstFB = DeviceManager->GetFramebuffer(0);

        TSharedPtr<FReSTIRGITemporalPass> RTPass =
            std::make_shared<FReSTIRGITemporalPass>(DeviceManager.get());
        if (!RTPass->Initialize(NvrhiDevice, FirstFB, FString(TXT("ReSTIR GI Temporal"))))
        {
            throw std::runtime_error("Failed to initialize FReSTIRGITemporalPass");
        }

        DeviceManager->AddRenderPassToBack(RTPass);

        // Safety timer: stop the loop after a generous budget regardless of
        // what the test does internally (see TestPathTraceGI for the pattern).
        std::thread([&]() {
            const double TimeoutSec = std::max(30.0, 16.0);
            FTimer Timer;
            while (Timer.MarkSec() < TimeoutSec) {}
            DeviceManager->StopMessageLoop();
        }).detach();

        DeviceManager->RunMessageLoop();
    }
    catch (const std::exception& e)
    {
        HLVM_LOG(LogTest, critical, TXT("Test failed: {}"), TO_TCHAR_CSTR(e.what()));
        return false;
    }
    catch (...)
    {
        HLVM_LOG(LogTest, critical, TXT("Test failed: unknown exception"));
        return false;
    }

    HLVM_LOG(LogTest, info, TXT("ReSTIR GI Temporal test completed"));
    return true;
}

#else

RECORD_BOOL(test_ReSTIR_GI_Temporal)
{
    HLVM_LOG(LogTest, warn, TXT("ReSTIR GI Temporal test is Vulkan-only"));
    return true;
}

#endif // HLVM_VULKAN_RENDERER
