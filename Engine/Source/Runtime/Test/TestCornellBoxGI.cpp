/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 *
 * Cornell Box GI Validation Test
 *
 * Pipeline:
 * 1. Inline Cornell Box geometry (12 tris box + 2 tris light)
 * 2. GBuffer Pass: Render all meshes to 5 MRTs
 * 3. GI Pass: Sequential bounce tracing (3 bounces), cosine-weighted sampling
 * 4. ReBLUR / ReSTIR denoising (same as TestFewBounceGI)
 * 5. Blit Pass: Copy denoised output to swapchain
 *
 * Purpose: Validate the GI/denoiser pipeline under ideal signal conditions.
 */

#include "Test.h"
#include "Renderer/DeviceManager.h"
#include "Renderer/RHI/Object/Buffer.h"
#include "Renderer/Common/FBindingCache.h"
#include "Renderer/Common/FCommonRenderPasses.h"
#include "Renderer/SceneGraph/FNode.h"
#include "Renderer/Mesh/StaticMesh.h"
#include "Renderer/Material/PBRMaterial.h"
#include "Renderer/Texture/TextureCache.h"
#include "Renderer/RayTracing/BLASBuilder.h"
#include "Renderer/RayTracing/TLASBuilder.h"
#include "Renderer/RayTracing/FRayTracingPipeline.h"
#include "Renderer/DescriptorTableManager.h"
#include "Renderer/ShaderMake/ShaderBlob.h"
#include "Renderer/PostProcess/FReSTIRPass.h"
#include "Renderer/PostProcess/FReBLURPass.h"
#include "Renderer/GI/GICVars.h"
#include "Renderer/PostProcess/FBilateralDenoisePass.h"
#include "Image/FImageDump.h"
#include "Core/Parallel/Async/WorkStealThreadPool.h"
#include <nvrhi/utils.h>
#include <Utility/Timer.h>
#include <filesystem>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

DECLARE_LOG_CATEGORY(LogTest)

#if HLVM_VULKAN_RENDERER

// =============================================================================
// CONFIGURATION
// =============================================================================

static const char*    WINDOW_TITLE = "Cornell Box GI Validation";
static const uint32_t WINDOW_WIDTH = 800;
static const uint32_t WINDOW_HEIGHT = 600;

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

static std::vector<char> ReadBinaryFile(const std::string& Filename)
{
    std::ifstream file(Filename, std::ios::ate | std::ios::binary);
    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open file: " + Filename);
    }

    size_t            fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(fileSize));
    file.close();

    return buffer;
}

// =============================================================================
// FCornellBoxGIPass
// =============================================================================

class FCornellBoxGIPass : public IRenderPass
{
public:
    using IRenderPass::IRenderPass;

    bool Initialize(nvrhi::IDevice* Device, nvrhi::IFramebuffer* Framebuffer, const FString& InWindowTitle)
    {
        HLVM_LOG(LogTest, info, TXT("=== FCornellBoxGIPass::Initialize ==="));

        NvrhiDevice = Device;
        FBInfo = Framebuffer->getFramebufferInfo();
        WindowTitle = InWindowTitle;

        BindingCache.SetDevice(NvrhiDevice);

        const auto DataDir = FString::Format(
            TXT("{}/Engine/Source/Runtime/Test/TestCornellBoxGI_Data"),
            *GProjectRoot);

        // =====================================================================
        // Load GI shader from ShaderMake blob
        // =====================================================================
        HLVM_LOG(LogTest, info, TXT("Loading GI shader..."));
        auto GIShaderBlob = ReadBinaryFile(
            FPath::Combine(DataDir, TXT("CornellBoxGI.sblob")).string());

        const void* GIShaderBinary = nullptr;
        size_t      GIShaderBinarySize = 0;
        if (!ShaderMake::FindPermutationInBlob(GIShaderBlob.data(), GIShaderBlob.size(), nullptr, 0, &GIShaderBinary, &GIShaderBinarySize))
        {
            HLVM_LOG(LogTest, err, TXT("Failed to extract GI shader from blob"));
            return false;
        }

        nvrhi::ShaderLibraryHandle ShaderLibrary = NvrhiDevice->createShaderLibrary(
            GIShaderBinary, GIShaderBinarySize);
        if (!ShaderLibrary)
        {
            HLVM_LOG(LogTest, err, TXT("Failed to load GI shader library"));
            return false;
        }

        if (!RTPipeline.InitializeFromLibrary(NvrhiDevice, ShaderLibrary,
            TXT("RayGen"), TXT("ClosestHit"), TXT("Miss"), TXT("ShadowMiss")))
        {
            HLVM_LOG(LogTest, err, TXT("Failed to initialize GI ray tracing pipeline"));
            return false;
        }
        HLVM_LOG(LogTest, info, TXT("GI ray tracing pipeline initialized"));

        // =====================================================================
        // Load GBuffer shaders
        // =====================================================================
        HLVM_LOG(LogTest, info, TXT("Loading GBuffer shaders..."));

        auto GBufferVSBlob = ReadBinaryFile(
            FPath::Combine(DataDir, TXT("GBufferSponzaVS.sblob")).string());
        const void* GBufferVSBinary = nullptr;
        size_t GBufferVSBinarySize = 0;
        if (!ShaderMake::FindPermutationInBlob(GBufferVSBlob.data(), GBufferVSBlob.size(), nullptr, 0, &GBufferVSBinary, &GBufferVSBinarySize))
        {
            HLVM_LOG(LogTest, err, TXT("Failed to extract GBufferVS from blob"));
            return false;
        }

        auto GBufferPSBlob = ReadBinaryFile(
            FPath::Combine(DataDir, TXT("GBufferSponzaPS.sblob")).string());
        const void* GBufferPSBinary = nullptr;
        size_t GBufferPSBinarySize = 0;
        if (!ShaderMake::FindPermutationInBlob(GBufferPSBlob.data(), GBufferPSBlob.size(), nullptr, 0, &GBufferPSBinary, &GBufferPSBinarySize))
        {
            HLVM_LOG(LogTest, err, TXT("Failed to extract GBufferPS from blob"));
            return false;
        }

        nvrhi::ShaderDesc VSDesc;
        VSDesc.setShaderType(nvrhi::ShaderType::Vertex);
        GBufferVS = NvrhiDevice->createShader(VSDesc, GBufferVSBinary, GBufferVSBinarySize);

        nvrhi::ShaderDesc PSDesc;
        PSDesc.setShaderType(nvrhi::ShaderType::Pixel);
        GBufferPS = NvrhiDevice->createShader(PSDesc, GBufferPSBinary, GBufferPSBinarySize);

        if (!GBufferVS || !GBufferPS)
        {
            HLVM_LOG(LogTest, err, TXT("Failed to create GBuffer shaders"));
            return false;
        }
        HLVM_LOG(LogTest, info, TXT("GBuffer shaders loaded successfully"));

        // =====================================================================
        // Build inline Cornell Box geometry
        // =====================================================================
        HLVM_LOG(LogTest, info, TXT("Building Cornell Box geometry..."));

        const float E = CBExtent;
        const glm::vec3 Red(0.8f, 0.1f, 0.1f);
        const glm::vec3 Green(0.1f, 0.8f, 0.1f);
        const glm::vec3 White(0.85f, 0.85f, 0.85f);

        auto MakeQuad = [&](const FString& Name, const glm::vec3& A, const glm::vec3& B,
                            const glm::vec3& C, const glm::vec3& D, const glm::vec3& N) -> std::shared_ptr<FStaticMesh>
        {
            auto Mesh = std::make_shared<FStaticMesh>(Name);
            glm::vec3 Tangent = glm::normalize(B - A);
            FVertex V0(A, N, glm::vec2(0, 0), Tangent);
            FVertex V1(B, N, glm::vec2(1, 0), Tangent);
            FVertex V2(C, N, glm::vec2(1, 1), Tangent);
            FVertex V3(D, N, glm::vec2(0, 1), Tangent);
            Mesh->AddVertex(V0);
            Mesh->AddVertex(V1);
            Mesh->AddVertex(V2);
            Mesh->AddVertex(V3);
            Mesh->AddTriangle(0, 1, 2);
            Mesh->AddTriangle(0, 2, 3);
            return Mesh;
        };

        auto MakeMaterial = [&](const FString& Name, const glm::vec3& Color) -> std::shared_ptr<FPBRMaterial>
        {
            auto Mat = std::make_shared<FPBRMaterial>(Name);
            Mat->AlbedoColor = FVec3(Color.x, Color.y, Color.z);
            Mat->Metallic = 0.0f;
            Mat->Roughness = 1.0f;

            // Create a 1x1 solid-color albedo texture so the GBuffer pass writes
            // the correct diffuse color for each Cornell Box face.
            nvrhi::TextureDesc Desc;
            Desc.dimension = nvrhi::TextureDimension::Texture2D;
            Desc.width = 1;
            Desc.height = 1;
            Desc.format = nvrhi::Format::RGBA8_UNORM;
            Desc.initialState = nvrhi::ResourceStates::ShaderResource;
            Desc.keepInitialState = true;
            nvrhi::TextureHandle Tex = NvrhiDevice->createTexture(Desc);

            uint8_t Pixel[4] = {
                static_cast<uint8_t>(glm::clamp(Color.r, 0.0f, 1.0f) * 255.0f),
                static_cast<uint8_t>(glm::clamp(Color.g, 0.0f, 1.0f) * 255.0f),
                static_cast<uint8_t>(glm::clamp(Color.b, 0.0f, 1.0f) * 255.0f),
                255
            };
            nvrhi::CommandListHandle TexCmdList = NvrhiDevice->createCommandList();
            TexCmdList->open();
            TexCmdList->writeTexture(Tex, 0, 0, Pixel, 4);
            TexCmdList->close();
            NvrhiDevice->executeCommandList(TexCmdList);

            Mat->GetGPUTexture(IMaterial::ETextureType::Albedo).InitializeFromHandle(Tex, NvrhiDevice);
            return Mat;
        };

        // Floor (y = -E), normal +Y
        StaticMeshes.push_back(MakeQuad(TXT("Floor"),
            glm::vec3(-E, -E,  E), glm::vec3( E, -E,  E),
            glm::vec3( E, -E, -E), glm::vec3(-E, -E, -E), glm::vec3(0, 1, 0)));
        MeshMaterials.push_back(MakeMaterial(TXT("White"), White));

        // Ceiling (y = E), normal -Y; dim emissive fill for general illumination
        StaticMeshes.push_back(MakeQuad(TXT("Ceiling"),
            glm::vec3(-E,  E, -E), glm::vec3( E,  E, -E),
            glm::vec3( E,  E,  E), glm::vec3(-E,  E,  E), glm::vec3(0, -1, 0)));
        auto CeilingMat = MakeMaterial(TXT("White"), White);
        CeilingMat->EmissiveColor = FVec3(1.0f, 1.0f, 1.0f);
        MeshMaterials.push_back(CeilingMat);

        // Front wall (z = E), normal -Z (closes the box so GI rays stay inside)
        StaticMeshes.push_back(MakeQuad(TXT("FrontWall"),
            glm::vec3(-E, -E,  E), glm::vec3(-E,  E,  E),
            glm::vec3( E,  E,  E), glm::vec3( E, -E,  E), glm::vec3(0, 0, -1)));
        MeshMaterials.push_back(MakeMaterial(TXT("White"), White));

        // Back wall (z = -E), normal +Z
        StaticMeshes.push_back(MakeQuad(TXT("BackWall"),
            glm::vec3(-E, -E, -E), glm::vec3( E, -E, -E),
            glm::vec3( E,  E, -E), glm::vec3(-E,  E, -E), glm::vec3(0, 0, 1)));
        MeshMaterials.push_back(MakeMaterial(TXT("White"), White));

        // Left wall (x = -E), normal +X (red) - made emissive to give a strong,
        // stable color-bleed signal on the floor for validation.
        StaticMeshes.push_back(MakeQuad(TXT("LeftWall"),
            glm::vec3(-E, -E, -E), glm::vec3(-E, -E,  E),
            glm::vec3(-E,  E,  E), glm::vec3(-E,  E, -E), glm::vec3(1, 0, 0)));
        auto LeftWallMat = MakeMaterial(TXT("Red"), Red);
        LeftWallMat->EmissiveColor = FVec3(3.0f, 0.0f, 0.0f);
        MeshMaterials.push_back(LeftWallMat);

        // Right wall (x = E), normal -X (green) - made emissive to give a strong,
        // stable color-bleed signal on the floor for validation.
        StaticMeshes.push_back(MakeQuad(TXT("RightWall"),
            glm::vec3( E, -E,  E), glm::vec3( E, -E, -E),
            glm::vec3( E,  E, -E), glm::vec3( E,  E,  E), glm::vec3(-1, 0, 0)));
        auto RightWallMat = MakeMaterial(TXT("Green"), Green);
        RightWallMat->EmissiveColor = FVec3(0.0f, 3.0f, 0.0f);
        MeshMaterials.push_back(RightWallMat);



        SceneCenter = glm::vec3(0.0f);
        HLVM_LOG(LogTest, info, TXT("Cornell Box built with {} meshes"), StaticMeshes.size());

        // =====================================================================
        // Set up bindless texture descriptor table
        // =====================================================================
        HLVM_LOG(LogTest, info, TXT("Setting up bindless texture descriptor table..."));

        {
            // Create bindless layout with ONLY Texture_SRV space
            // (NVRHI Vulkan does not support bindless samplers — bind sampler traditionally)
            nvrhi::BindlessLayoutDesc BindlessDesc;
            BindlessDesc.visibility = nvrhi::ShaderType::All;
            BindlessDesc.firstSlot = 0;
            BindlessDesc.maxCapacity = 16384;
            BindlessDesc.registerSpaces = {
                nvrhi::BindingLayoutItem::Texture_SRV(0)    // t0, t1, ... tN
            };
            BindlessLayout = NvrhiDevice->createBindlessLayout(BindlessDesc);
            DescTableMgr = std::make_shared<FDescriptorTableManager>(NvrhiDevice, BindlessLayout.Get());

            // Create white fallback texture (1x1 RGBA8) for slot 0
            nvrhi::TextureDesc WhiteDesc;
            WhiteDesc.dimension = nvrhi::TextureDimension::Texture2D;
            WhiteDesc.width = 1;
            WhiteDesc.height = 1;
            WhiteDesc.format = nvrhi::Format::RGBA8_UNORM;
            WhiteDesc.isRenderTarget = false;
            WhiteDesc.isUAV = false;
            WhiteDesc.isTypeless = false;
            WhiteDesc.initialState = nvrhi::ResourceStates::ShaderResource;
            WhiteDesc.keepInitialState = true;
            WhiteDesc.debugName = "BindlessWhiteFallback";
            nvrhi::TextureHandle WhiteTexture = NvrhiDevice->createTexture(WhiteDesc);

            nvrhi::CommandListHandle WhiteCmdList = NvrhiDevice->createCommandList();
            WhiteCmdList->open();
            uint32_t whitePixel = 0xFFFFFFFF;
            WhiteCmdList->writeTexture(WhiteTexture, 0, 0, &whitePixel, 4);
            WhiteCmdList->close();
            NvrhiDevice->executeCommandList(WhiteCmdList);

            // Register white texture as slot 0 in cache + descriptor table
            TextureCache.SetDescriptorTableManager(DescTableMgr.get());
            TextureCache.Insert(FPath(TXT("__BindlessWhiteFallback__")), WhiteTexture);

            // Cornell Box uses inline 1x1 albedo textures created below rather than
            // loaded asset textures. The bindless table only needs the white fallback
            // because the GI closest-hit shader falls back to InstanceInfo.AlbedoColor
            // when AlbedoTextureIndex == 0.
            HLVM_LOG(LogTest, info, TXT("Bindless descriptor table: {} textures registered"),
                DescTableMgr->GetAllocatedCount());
        }

        // =====================================================================
        // Create command list for initialization (BLAS, TLAS, RT buffers)
        // =====================================================================
        nvrhi::CommandListHandle InitCmdList = NvrhiDevice->createCommandList();
        InitCmdList->open();

        // =====================================================================
        // Build BLAS for each mesh
        // =====================================================================
        HLVM_LOG(LogTest, info, TXT("Building BLAS for {} meshes..."), StaticMeshes.size());
        for (const auto& Mesh : StaticMeshes)
        {
            nvrhi::rt::AccelStructHandle BLAS = BLASBuilder::Build(NvrhiDevice, InitCmdList.Get(), *Mesh);
            if (!BLAS)
            {
                HLVM_LOG(LogTest, err, TXT("Failed to build BLAS for mesh"));
                return false;
            }
            MeshBLASHandles.push_back(BLAS);
        }

        // =====================================================================
        // Build TLAS
        // =====================================================================
        if (!TLASBuilder.Initialize(NvrhiDevice, static_cast<uint32_t>(StaticMeshes.size())))
        {
            HLVM_LOG(LogTest, err, TXT("Failed to initialize TLAS builder"));
            return false;
        }
        HLVM_LOG(LogTest, info, TXT("Building TLAS with {} instances..."), StaticMeshes.size());

        // Add Sponza meshes as instances
        for (uint32_t i = 0; i < static_cast<uint32_t>(StaticMeshes.size()); ++i)
        {
            FTLASBuilder::FInstanceDesc InstanceDesc;
            InstanceDesc.BottomLevelAS = MeshBLASHandles[i];
            InstanceDesc.InstanceMask = 1;
            InstanceDesc.InstanceFlags = nvrhi::rt::InstanceFlags::TriangleFrontCounterclockwise;
            // Cornell Box geometry is already in world units; no additional scaling
            glm::mat4 MeshTransform = glm::mat4(1.0f);
            InstanceDesc.SetTransform(MeshTransform);
            if (!TLASBuilder.AddInstance(InstanceDesc))
            {
                HLVM_LOG(LogTest, err, TXT("Failed to add instance to TLAS"));
                return false;
            }
        }
        if (!TLASBuilder.Build(InitCmdList.Get()))
        {
            HLVM_LOG(LogTest, err, TXT("Failed to build TLAS"));
            return false;
        }
        TopLevelAS = TLASBuilder.GetTLAS();
        HLVM_LOG(LogTest, info, TXT("TLAS built with {} instances"), TLASBuilder.GetInstanceCount());

        // =====================================================================
        // Create unified vertex/index buffers for ray tracing hit shaders
        // =====================================================================
        {
            struct FRTVertex { glm::vec3 Position; glm::vec3 Normal; glm::vec2 UV; };
            struct FInstanceInfo {
                uint32_t VertexOffset;
                uint32_t IndexOffset;
                uint32_t VertexCount;
                uint32_t IndexCount;
                float AlbedoColor[3];
                uint32_t AlbedoTextureIndex;
                float EmissiveColor[3];
                uint32_t EmissiveTextureIndex;
            };

            TVector<FRTVertex> AllVertices;
            TVector<uint32_t> AllIndices;
            TVector<FInstanceInfo> InstanceInfos;
            AllVertices.reserve(1024 * 1024);
            AllIndices.reserve(1024 * 1024);

            uint32_t vertexOffset = 0;
            uint32_t indexOffset = 0;

            for (const auto& Mesh : StaticMeshes)
            {
                const auto& Vertices = Mesh->GetVertices();
                const auto& Indices = Mesh->GetIndices();

                FInstanceInfo Info;
                Info.VertexOffset = vertexOffset;
                Info.IndexOffset = indexOffset;
                Info.VertexCount = static_cast<uint32_t>(Vertices.size());
                Info.IndexCount = static_cast<uint32_t>(Indices.size());
                Info.AlbedoColor[0] = 0.7f;
                Info.AlbedoColor[1] = 0.7f;
                Info.AlbedoColor[2] = 0.7f;
                Info.AlbedoTextureIndex = 0; // Default to white fallback (slot 0)
                Info.EmissiveColor[0] = 0.0f;
                Info.EmissiveColor[1] = 0.0f;
                Info.EmissiveColor[2] = 0.0f;
                Info.EmissiveTextureIndex = 0;

                // Look up material albedo/emissive color for this mesh
                size_t MatIdx = static_cast<size_t>(&Mesh - StaticMeshes.data());
                if (MatIdx < MeshMaterials.size())
                {
                    auto PBRMat = MeshMaterials[MatIdx];
                    if (PBRMat)
                    {
                        FVec3 albedo = PBRMat->GetAlbedoColor();
                        Info.AlbedoColor[0] = albedo.x;
                        Info.AlbedoColor[1] = albedo.y;
                        Info.AlbedoColor[2] = albedo.z;

                        FVec3 emissive = PBRMat->EmissiveColor;
                        Info.EmissiveColor[0] = emissive.x;
                        Info.EmissiveColor[1] = emissive.y;
                        Info.EmissiveColor[2] = emissive.z;

                        // AlbedoTextureIndex stays 0; GI shader uses AlbedoColor directly
                    }
                }

                InstanceInfos.push_back(Info);

                for (const auto& V : Vertices)
                {
                    FRTVertex RTV;
                    RTV.Position = V.Position;
                    RTV.Normal = V.Normal;
                    RTV.UV = glm::vec2(V.UV.x, V.UV.y);
                    AllVertices.push_back(RTV);
                }

                for (uint32_t Idx : Indices)
                {
                    AllIndices.push_back(Idx + vertexOffset);
                }

                vertexOffset += Info.VertexCount;
                indexOffset += Info.IndexCount;
            }

            if (!AllVertices.empty())
            {
                nvrhi::BufferDesc VBDesc;
                VBDesc.byteSize = AllVertices.size() * sizeof(FRTVertex);
                VBDesc.isVolatile = false;
                VBDesc.keepInitialState = true;
                VBDesc.initialState = nvrhi::ResourceStates::ShaderResource;
                VBDesc.debugName = "RTVertexBuffer";
                VBDesc.structStride = sizeof(FRTVertex);
                RTVertexBuffer = NvrhiDevice->createBuffer(VBDesc);
                InitCmdList->writeBuffer(RTVertexBuffer, AllVertices.data(), VBDesc.byteSize);
            }

            if (!AllIndices.empty())
            {
                nvrhi::BufferDesc IBDesc;
                IBDesc.byteSize = AllIndices.size() * sizeof(uint32_t);
                IBDesc.isVolatile = false;
                IBDesc.keepInitialState = true;
                IBDesc.initialState = nvrhi::ResourceStates::ShaderResource;
                IBDesc.debugName = "RTIndexBuffer";
                IBDesc.structStride = sizeof(uint32_t);
                RTIndexBuffer = NvrhiDevice->createBuffer(IBDesc);
                InitCmdList->writeBuffer(RTIndexBuffer, AllIndices.data(), IBDesc.byteSize);
            }

            if (!InstanceInfos.empty())
            {
                nvrhi::BufferDesc InfoDesc;
                InfoDesc.byteSize = InstanceInfos.size() * sizeof(FInstanceInfo);
                InfoDesc.isVolatile = false;
                InfoDesc.keepInitialState = true;
                InfoDesc.initialState = nvrhi::ResourceStates::ShaderResource;
                InfoDesc.debugName = "RTInstanceInfoBuffer";
                InfoDesc.structStride = sizeof(FInstanceInfo);
                RTInstanceInfoBuffer = NvrhiDevice->createBuffer(InfoDesc);
                InitCmdList->writeBuffer(RTInstanceInfoBuffer, InstanceInfos.data(), InfoDesc.byteSize);
            }
        }

        InitCmdList->close();
        NvrhiDevice->executeCommandList(InitCmdList);

        // =====================================================================
        // Create GBuffer textures and framebuffer
        // =====================================================================
        HLVM_LOG(LogTest, info, TXT("Creating GBuffer textures..."));

        uint32_t GBufferWidth = Framebuffer->getFramebufferInfo().width;
        uint32_t GBufferHeight = Framebuffer->getFramebufferInfo().height;

        // MRT0: Diffuse (RGBA8)
        {
            nvrhi::TextureDesc Desc;
            Desc.dimension = nvrhi::TextureDimension::Texture2D;
            Desc.width = GBufferWidth;
            Desc.height = GBufferHeight;
            Desc.format = nvrhi::Format::RGBA8_UNORM;
            Desc.isRenderTarget = true;
            Desc.isUAV = false;
            Desc.initialState = nvrhi::ResourceStates::RenderTarget;
            Desc.keepInitialState = true;
            Desc.debugName = "GBufferDiffuse";
            GBufferDiffuseTexture = NvrhiDevice->createTexture(Desc);
        }

        // MRT1: Specular (RGBA16F)
        {
            nvrhi::TextureDesc Desc;
            Desc.dimension = nvrhi::TextureDimension::Texture2D;
            Desc.width = GBufferWidth;
            Desc.height = GBufferHeight;
            Desc.format = nvrhi::Format::RGBA16_FLOAT;
            Desc.isRenderTarget = true;
            Desc.isUAV = false;
            Desc.initialState = nvrhi::ResourceStates::RenderTarget;
            Desc.keepInitialState = true;
            Desc.debugName = "GBufferSpecular";
            GBufferSpecularTexture = NvrhiDevice->createTexture(Desc);
        }

        // MRT2: Normals (RGBA16F)
        {
            nvrhi::TextureDesc Desc;
            Desc.dimension = nvrhi::TextureDimension::Texture2D;
            Desc.width = GBufferWidth;
            Desc.height = GBufferHeight;
            Desc.format = nvrhi::Format::RGBA16_FLOAT;
            Desc.isRenderTarget = true;
            Desc.isUAV = false;
            Desc.initialState = nvrhi::ResourceStates::RenderTarget;
            Desc.keepInitialState = true;
            Desc.debugName = "GBufferNormals";
            GBufferNormalsTexture = NvrhiDevice->createTexture(Desc);
        }

        // MRT3: Emissive (RGBA16F)
        {
            nvrhi::TextureDesc Desc;
            Desc.dimension = nvrhi::TextureDimension::Texture2D;
            Desc.width = GBufferWidth;
            Desc.height = GBufferHeight;
            Desc.format = nvrhi::Format::RGBA16_FLOAT;
            Desc.isRenderTarget = true;
            Desc.isUAV = false;
            Desc.initialState = nvrhi::ResourceStates::RenderTarget;
            Desc.keepInitialState = true;
            Desc.debugName = "GBufferEmissive";
            GBufferEmissiveTexture = NvrhiDevice->createTexture(Desc);
        }

        // MRT4: WorldPos (RGBA16F)
        {
            nvrhi::TextureDesc Desc;
            Desc.dimension = nvrhi::TextureDimension::Texture2D;
            Desc.width = GBufferWidth;
            Desc.height = GBufferHeight;
            Desc.format = nvrhi::Format::RGBA16_FLOAT;
            Desc.isRenderTarget = true;
            Desc.isUAV = false;
            Desc.initialState = nvrhi::ResourceStates::RenderTarget;
            Desc.keepInitialState = true;
            Desc.debugName = "GBufferWorldPos";
            GBufferWorldPosTexture = NvrhiDevice->createTexture(Desc);
        }

        // Depth texture (D32)
        {
            nvrhi::TextureDesc Desc;
            Desc.dimension = nvrhi::TextureDimension::Texture2D;
            Desc.width = GBufferWidth;
            Desc.height = GBufferHeight;
            Desc.format = nvrhi::Format::D32;
            Desc.isRenderTarget = true;
            Desc.isUAV = false;
            Desc.isTypeless = true;
            Desc.initialState = nvrhi::ResourceStates::DepthWrite;
            Desc.keepInitialState = true;
            Desc.debugName = "GBufferDepth";
            GBufferDepthTexture = NvrhiDevice->createTexture(Desc);
        }

        // Create GBuffer framebuffer
        {
            nvrhi::FramebufferDesc FBDesc;
            nvrhi::FramebufferAttachment DiffuseAttach;
            DiffuseAttach.setTexture(GBufferDiffuseTexture);
            FBDesc.addColorAttachment(DiffuseAttach);

            nvrhi::FramebufferAttachment SpecularAttach;
            SpecularAttach.setTexture(GBufferSpecularTexture);
            FBDesc.addColorAttachment(SpecularAttach);

            nvrhi::FramebufferAttachment NormalsAttach;
            NormalsAttach.setTexture(GBufferNormalsTexture);
            FBDesc.addColorAttachment(NormalsAttach);

            nvrhi::FramebufferAttachment EmissiveAttach;
            EmissiveAttach.setTexture(GBufferEmissiveTexture);
            FBDesc.addColorAttachment(EmissiveAttach);

            nvrhi::FramebufferAttachment WorldPosAttach;
            WorldPosAttach.setTexture(GBufferWorldPosTexture);
            FBDesc.addColorAttachment(WorldPosAttach);

            nvrhi::FramebufferAttachment DepthAttach;
            DepthAttach.setTexture(GBufferDepthTexture);
            FBDesc.setDepthAttachment(DepthAttach);

            GBufferFramebuffer = NvrhiDevice->createFramebuffer(FBDesc);
            if (!GBufferFramebuffer)
            {
                HLVM_LOG(LogTest, err, TXT("Failed to create GBuffer framebuffer"));
                return false;
            }
        }
        HLVM_LOG(LogTest, info, TXT("GBuffer framebuffer created with 5 color attachments"));

        // =====================================================================
        // Create per-mesh geometry buffers
        // =====================================================================
        HLVM_LOG(LogTest, info, TXT("Creating geometry buffers for {} meshes..."), StaticMeshes.size());

        AllMeshDrawData.reserve(StaticMeshes.size());

        for (size_t i = 0; i < StaticMeshes.size(); ++i)
        {
            const auto& Mesh = StaticMeshes[i];
            const auto& Vertices = Mesh->GetVertices();
            const auto& Indices = Mesh->GetIndices();

            FMeshDrawData DrawData;
            DrawData.Mesh = Mesh;
            DrawData.IndexCount = static_cast<uint32_t>(Indices.size());

            // Create vertex buffer
            {
                nvrhi::BufferDesc VBDesc;
                VBDesc.byteSize = Vertices.size() * sizeof(FVertex);
                VBDesc.isVertexBuffer = true;
                VBDesc.isVolatile = false;
                VBDesc.initialState = nvrhi::ResourceStates::CopyDest;
                VBDesc.debugName = "MeshVertexBuffer";
                DrawData.VertexBuffer = NvrhiDevice->createBuffer(VBDesc);

                nvrhi::CommandListHandle GeomCmdList = NvrhiDevice->createCommandList();
                GeomCmdList->open();
                GeomCmdList->beginTrackingBufferState(DrawData.VertexBuffer, nvrhi::ResourceStates::CopyDest);
                GeomCmdList->writeBuffer(DrawData.VertexBuffer, Vertices.data(), VBDesc.byteSize);
                GeomCmdList->setPermanentBufferState(DrawData.VertexBuffer, nvrhi::ResourceStates::VertexBuffer);
                GeomCmdList->close();
                NvrhiDevice->executeCommandList(GeomCmdList);
            }

            // Create index buffer
            {
                nvrhi::BufferDesc IBDesc;
                IBDesc.byteSize = Indices.size() * sizeof(uint32_t);
                IBDesc.isIndexBuffer = true;
                IBDesc.isVolatile = false;
                IBDesc.initialState = nvrhi::ResourceStates::CopyDest;
                IBDesc.debugName = "MeshIndexBuffer";
                DrawData.IndexBuffer = NvrhiDevice->createBuffer(IBDesc);

                nvrhi::CommandListHandle GeomCmdList = NvrhiDevice->createCommandList();
                GeomCmdList->open();
                GeomCmdList->beginTrackingBufferState(DrawData.IndexBuffer, nvrhi::ResourceStates::CopyDest);
                GeomCmdList->writeBuffer(DrawData.IndexBuffer, Indices.data(), IBDesc.byteSize);
                GeomCmdList->setPermanentBufferState(DrawData.IndexBuffer, nvrhi::ResourceStates::IndexBuffer);
                GeomCmdList->close();
                NvrhiDevice->executeCommandList(GeomCmdList);
            }

            AllMeshDrawData.push_back(DrawData);
        }
        HLVM_LOG(LogTest, info, TXT("Created geometry buffers for {} meshes"), AllMeshDrawData.size());

        // =====================================================================
        // Create GBuffer input layout (uses offsetof)
        // =====================================================================
        {
            nvrhi::VertexAttributeDesc Attrs[4];
            Attrs[0].setName("POSITION").setFormat(nvrhi::Format::RGB32_FLOAT)
                .setOffset(offsetof(FVertex, Position)).setElementStride(sizeof(FVertex));
            Attrs[1].setName("NORMAL").setFormat(nvrhi::Format::RGB32_FLOAT)
                .setOffset(offsetof(FVertex, Normal)).setElementStride(sizeof(FVertex));
            Attrs[2].setName("TEXCOORD0").setFormat(nvrhi::Format::RG32_FLOAT)
                .setOffset(offsetof(FVertex, UV)).setElementStride(sizeof(FVertex));
            Attrs[3].setName("TANGENT").setFormat(nvrhi::Format::RGB32_FLOAT)
                .setOffset(offsetof(FVertex, Tangent)).setElementStride(sizeof(FVertex));

            GBufferInputLayout = NvrhiDevice->createInputLayout(Attrs, 4, GBufferVS);
        }

        // =====================================================================
        // Create GBuffer binding layout
        // =====================================================================
        {
            nvrhi::BindingLayoutDesc LayoutDesc;
            LayoutDesc.visibility = nvrhi::ShaderType::Vertex | nvrhi::ShaderType::Pixel;

            nvrhi::VulkanBindingOffsets offsets;
            offsets.setConstantBufferOffset(0).setShaderResourceOffset(0).setSamplerOffset(0).setUnorderedAccessViewOffset(0);
            LayoutDesc.setBindingOffsets(offsets);

            LayoutDesc.bindings = {
                nvrhi::BindingLayoutItem::ConstantBuffer(256), // ViewConstants b0
                nvrhi::BindingLayoutItem::Texture_SRV(0),      // DiffuseTexture t0
                nvrhi::BindingLayoutItem::Sampler(128)         // LinearSampler s0
            };

            GBufferBindingLayout = NvrhiDevice->createBindingLayout(LayoutDesc);
        }

        // =====================================================================
        // Create GBuffer constant buffer
        // =====================================================================
        {
            nvrhi::BufferDesc CBDesc;
            CBDesc.byteSize = sizeof(float) * 16 * 3 + sizeof(float) * 4; // Model(16) + View(16) + Proj(16) + RenderTargetSize(2) + Padding(2)
            CBDesc.isConstantBuffer = true;
            CBDesc.isVolatile = false;
            CBDesc.keepInitialState = true;
            CBDesc.initialState = nvrhi::ResourceStates::ConstantBuffer;
            CBDesc.debugName = "ViewConstants";
            ViewConstantsBuffer = NvrhiDevice->createBuffer(CBDesc);
        }

        // =====================================================================
        // Create GBuffer sampler
        // =====================================================================
        {
            nvrhi::SamplerDesc SamplerDesc;
            SamplerDesc.setAddressU(nvrhi::SamplerAddressMode::Repeat)
                .setAddressV(nvrhi::SamplerAddressMode::Repeat)
                .setAddressW(nvrhi::SamplerAddressMode::Repeat)
                .setMinFilter(true)
                .setMagFilter(true)
                .setMipFilter(true);
            GBufferLinearSampler = NvrhiDevice->createSampler(SamplerDesc);
        }

        // =====================================================================
        // Create placeholder texture (1x1 white)
        // =====================================================================
        {
            nvrhi::TextureDesc Desc;
            Desc.dimension = nvrhi::TextureDimension::Texture2D;
            Desc.width = 1;
            Desc.height = 1;
            Desc.format = nvrhi::Format::RGBA8_UNORM;
            Desc.isRenderTarget = false;
            Desc.isUAV = false;
            Desc.isTypeless = false;
            Desc.initialState = nvrhi::ResourceStates::ShaderResource;
            Desc.keepInitialState = true;
            Desc.debugName = "PlaceholderTexture";
            PlaceholderTexture = NvrhiDevice->createTexture(Desc);

            nvrhi::CommandListHandle TexCmdList = NvrhiDevice->createCommandList();
            TexCmdList->open();
            uint32_t whitePixel = 0xFFFFFFFF;
            TexCmdList->writeTexture(PlaceholderTexture, 0, 0, &whitePixel, 4);
            TexCmdList->close();
            NvrhiDevice->executeCommandList(TexCmdList);
        }

        // =====================================================================
        // Create GBuffer pipeline
        // =====================================================================
        {
            nvrhi::GraphicsPipelineDesc PipelineDesc;
            PipelineDesc.setVertexShader(GBufferVS);
            PipelineDesc.setPixelShader(GBufferPS);
            PipelineDesc.setInputLayout(GBufferInputLayout);
            PipelineDesc.addBindingLayout(GBufferBindingLayout);
            PipelineDesc.setPrimType(nvrhi::PrimitiveType::TriangleList);
            PipelineDesc.renderState.setRasterState(nvrhi::RasterState().setCullNone());
            PipelineDesc.renderState.depthStencilState
                .setDepthTestEnable(true)
                .setDepthWriteEnable(true)
                .setDepthFunc(nvrhi::ComparisonFunc::Less);

            GBufferPipeline = NvrhiDevice->createGraphicsPipeline(PipelineDesc, GBufferFramebuffer->getFramebufferInfo());
            if (!GBufferPipeline)
            {
                HLVM_LOG(LogTest, err, TXT("Failed to create GBuffer pipeline"));
                return false;
            }
        }
        HLVM_LOG(LogTest, info, TXT("GBuffer pipeline created"));

        // =====================================================================
        // Create GI ray tracing pipeline (encapsulated in FRayTracingPipeline)
        // =====================================================================
        HLVM_LOG(LogTest, info, TXT("Creating GI ray tracing pipeline..."));

        {
            RTPipeline.CreateBindingLayout(nvrhi::ShaderType::All)
                .AddConstantBuffer(0)           // b0: GIConstants
                .AddConstantBuffer(1)           // b1: ViewConstants
                .AddRayTracingAccelStruct(0)    // t0: SceneBVH
                .AddTextureSRV(1)               // t1: GBufferWorldPos
                .AddTextureSRV(2)               // t2: GBufferNormals
                .AddTextureSRV(3)               // t3: GBufferDiffuse
                .AddStructuredBufferSRV(5)      // t5: RTVertices
                .AddStructuredBufferSRV(6)      // t6: RTIndices
                .AddStructuredBufferSRV(7)      // t7: RTInstanceInfo
                .AddTextureUAV(0)               // u0: Output
                .AddSampler(2);                 // s2: LinearSampler (traditional, non-bindless)

            RTPipeline.SetBindlessLayout(BindlessLayout);

            if (!RTPipeline.FinalizePipeline(sizeof(float) * 16))
            {
                HLVM_LOG(LogTest, err, TXT("Failed to create GI ray tracing pipeline"));
                return false;
            }

            if (!RTPipeline.BuildShaderTable())
            {
                HLVM_LOG(LogTest, err, TXT("Failed to build GI shader table"));
                return false;
            }

            HLVM_LOG(LogTest, info, TXT("GI ray tracing pipeline created"));
        }

        // =====================================================================
        // Create HDR output texture
        // =====================================================================
        {
            nvrhi::TextureDesc Desc;
            Desc.dimension = nvrhi::TextureDimension::Texture2D;
            Desc.width = GBufferWidth;
            Desc.height = GBufferHeight;
            Desc.format = nvrhi::Format::RGBA32_FLOAT;
            Desc.isRenderTarget = false;
            Desc.isUAV = true;
            Desc.isTypeless = false;
            Desc.initialState = nvrhi::ResourceStates::UnorderedAccess;
            Desc.keepInitialState = true;
            Desc.debugName = "HDROutput";
            HDRTexture = NvrhiDevice->createTexture(Desc);
        }

        // =====================================================================
        // Create denoised HDR texture (output of bilateral denoise)
        // =====================================================================
        {
            nvrhi::TextureDesc Desc;
            Desc.dimension = nvrhi::TextureDimension::Texture2D;
            Desc.width = GBufferWidth;
            Desc.height = GBufferHeight;
            Desc.format = nvrhi::Format::RGBA32_FLOAT;
            Desc.isRenderTarget = false;
            Desc.isUAV = true;
            Desc.isTypeless = false;
            Desc.initialState = nvrhi::ResourceStates::UnorderedAccess;
            Desc.keepInitialState = true;
            Desc.debugName = "DenoisedHDROutput";
            DenoisedHDRTexture = NvrhiDevice->createTexture(Desc);
        }

        // =====================================================================
        // Initialize denoise pass
        // =====================================================================
        HLVM_LOG(LogTest, info, TXT("Initializing bilateral denoise pass..."));
        if (!DenoisePass.Initialize(NvrhiDevice, DataDir))
        {
            HLVM_LOG(LogTest, err, TXT("Failed to initialize DenoisePass"));
            return false;
        }

        // =====================================================================
        // Initialize ReBLUR pass
        // =====================================================================
        if (CVar_r_ReBLUR_Enable.GetValue())
        {
            HLVM_LOG(LogTest, info, TXT("Initializing ReBLUR denoise pass..."));
            if (!ReBLURPass.Initialize(NvrhiDevice, DataDir))
            {
                HLVM_LOG(LogTest, err, TXT("Failed to initialize ReBLURPass"));
                return false;
            }
            bReBLURInitialized = true;

            nvrhi::TextureDesc Desc;
            Desc.dimension = nvrhi::TextureDimension::Texture2D;
            Desc.width = GBufferWidth;
            Desc.height = GBufferHeight;
            // History must match output format (RGBA32F) for legal Vulkan copyTexture
            Desc.format = nvrhi::Format::RGBA32_FLOAT;
            Desc.isRenderTarget = false;
            Desc.isUAV = true;
            Desc.isTypeless = false;
            Desc.initialState = nvrhi::ResourceStates::UnorderedAccess;
            Desc.keepInitialState = true;
            Desc.debugName = "ReBLURHistory0";
            ReBLURHistoryTexture[0] = NvrhiDevice->createTexture(Desc);
            Desc.debugName = "ReBLURHistory1";
            ReBLURHistoryTexture[1] = NvrhiDevice->createTexture(Desc);

            // Clear history
            nvrhi::CommandListHandle ClearCmd = NvrhiDevice->createCommandList();
            ClearCmd->open();
            nvrhi::Color ClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            ClearCmd->clearTextureFloat(ReBLURHistoryTexture[0], nvrhi::AllSubresources, ClearColor);
            ClearCmd->clearTextureFloat(ReBLURHistoryTexture[1], nvrhi::AllSubresources, ClearColor);
            ClearCmd->close();
            NvrhiDevice->executeCommandList(ClearCmd);

            HLVM_LOG(LogTest, info, TXT("ReBLUR pass initialized successfully"));
        }




        // =====================================================================
        // Create ReSTIR textures and initialize ReSTIR pass
        // =====================================================================
        HLVM_LOG(LogTest, info, TXT("Creating ReSTIR textures..."));
        {
            nvrhi::TextureDesc Desc;
            Desc.dimension = nvrhi::TextureDimension::Texture2D;
            Desc.width = GBufferWidth;
            Desc.height = GBufferHeight;
            Desc.format = nvrhi::Format::RGBA16_FLOAT;
            Desc.isRenderTarget = false;
            Desc.isUAV = true;
            Desc.isTypeless = false;
            Desc.initialState = nvrhi::ResourceStates::UnorderedAccess;
            Desc.keepInitialState = true;

            Desc.debugName = "Reservoir0";
            Reservoir0Texture = NvrhiDevice->createTexture(Desc);
            Desc.debugName = "Reservoir1";
            Reservoir1Texture = NvrhiDevice->createTexture(Desc);
            Desc.debugName = "Reservoir0History";
            Reservoir0HistoryTexture = NvrhiDevice->createTexture(Desc);
            Desc.debugName = "Reservoir1History";
            Reservoir1HistoryTexture = NvrhiDevice->createTexture(Desc);
            Desc.debugName = "Reservoir0Merged";
            Reservoir0MergedTexture = NvrhiDevice->createTexture(Desc);
            Desc.debugName = "Reservoir1Merged";
            Reservoir1MergedTexture = NvrhiDevice->createTexture(Desc);

            Desc.format = nvrhi::Format::RGBA32_FLOAT;
            Desc.isShaderResource = true;  // sampled by Blit + ReSTIR temporal history
            Desc.debugName = "ReSTIROutput";
            ReSTIROutputTexture = NvrhiDevice->createTexture(Desc);
            Desc.debugName = "TemporalRadiance";
            TemporalRadianceTexture = NvrhiDevice->createTexture(Desc);
            Desc.debugName = "RadianceHistory";
            RadianceHistoryTexture = NvrhiDevice->createTexture(Desc);
        }

        // Previous frame depth and normal for temporal validation
        {
            nvrhi::TextureDesc Desc;
            Desc.dimension = nvrhi::TextureDimension::Texture2D;
            Desc.width = GBufferWidth;
            Desc.height = GBufferHeight;
            // Use same depth format as GBufferDepthTexture so copyTexture is valid
            Desc.format = nvrhi::Format::D32;
            Desc.isRenderTarget = false;
            Desc.isUAV = false;
            Desc.isTypeless = true;
            Desc.initialState = nvrhi::ResourceStates::ShaderResource;
            Desc.keepInitialState = true;
            Desc.debugName = "PrevDepth";
            PrevDepthTexture = NvrhiDevice->createTexture(Desc);

            Desc.format = nvrhi::Format::RGBA16_FLOAT;
            Desc.debugName = "PrevNormal";
            PrevNormalTexture = NvrhiDevice->createTexture(Desc);
        }

        // Clear history textures
        {
            nvrhi::CommandListHandle ClearCmd = NvrhiDevice->createCommandList();
            ClearCmd->open();
            nvrhi::Color ClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            ClearCmd->clearTextureFloat(Reservoir0HistoryTexture, nvrhi::AllSubresources, ClearColor);
            ClearCmd->clearTextureFloat(Reservoir1HistoryTexture, nvrhi::AllSubresources, ClearColor);
            ClearCmd->clearTextureFloat(RadianceHistoryTexture, nvrhi::AllSubresources, ClearColor);
            ClearCmd->close();
            NvrhiDevice->executeCommandList(ClearCmd);
        }

        if (!ReSTIRPass.Initialize(NvrhiDevice, DataDir))
        {
            HLVM_LOG(LogTest, err, TXT("Failed to initialize ReSTIRPass"));
            return false;
        }
        bReSTIRInitialized = true;
        HLVM_LOG(LogTest, info, TXT("ReSTIR pass initialized successfully"));

        // =====================================================================
        // Create GI constant buffer
        // =====================================================================
        {
            nvrhi::BufferDesc BufferDesc;
            BufferDesc.byteSize = sizeof(float) * 20; // 5 float4s
            BufferDesc.isConstantBuffer = true;
            BufferDesc.isVolatile = false;
            BufferDesc.keepInitialState = true;
            BufferDesc.initialState = nvrhi::ResourceStates::ConstantBuffer;
            BufferDesc.debugName = "GIConstants";
            GIConstantBuffer = NvrhiDevice->createBuffer(BufferDesc);
        }

        // =====================================================================
        // Create staging texture for frame dumps
        // =====================================================================
        {
            nvrhi::TextureDesc StagingDesc;
            StagingDesc.dimension = nvrhi::TextureDimension::Texture2D;
            StagingDesc.width = GBufferWidth;
            StagingDesc.height = GBufferHeight;
            StagingDesc.format = nvrhi::Format::RGBA32_FLOAT;
            StagingDesc.isRenderTarget = false;
            StagingDesc.isUAV = false;
            StagingDesc.isTypeless = false;
            StagingDesc.initialState = nvrhi::ResourceStates::CopyDest;
            StagingDesc.keepInitialState = false;
            StagingDesc.debugName = "StagingTexture";
            StagingTexture = NvrhiDevice->createStagingTexture(StagingDesc, nvrhi::CpuAccessMode::Read);
        }

        HLVM_LOG(LogTest, info, TXT("FFewBounceGIPass initialized successfully"));
        return true;
    }

    void Shutdown()
    {
        HLVM_LOG(LogTest, info, TXT("FFewBounceGIPass::Shutdown"));

        BindingCache.Clear();

        CommandList = nullptr;
        RTPipeline.Shutdown();
        TopLevelAS = nullptr;
        HDRTexture = nullptr;
        DenoisedHDRTexture = nullptr;
        DenoisePass.Shutdown();
        RTBindingSet = nullptr;
        if (bReBLURInitialized)
        {
            ReBLURPass.Shutdown();
            bReBLURInitialized = false;
        }
        ReBLURHistoryTexture[0] = nullptr;
        ReBLURHistoryTexture[1] = nullptr;
        if (bReSTIRInitialized)
        {
            ReSTIRPass.Shutdown();
            bReSTIRInitialized = false;
        }
        Reservoir0Texture = nullptr;
        Reservoir1Texture = nullptr;
        Reservoir0HistoryTexture = nullptr;
        Reservoir1HistoryTexture = nullptr;
        Reservoir0MergedTexture = nullptr;
        Reservoir1MergedTexture = nullptr;
        ReSTIROutputTexture = nullptr;
        TemporalRadianceTexture = nullptr;
        RadianceHistoryTexture = nullptr;
        PrevDepthTexture = nullptr;
        PrevNormalTexture = nullptr;
        GIConstantBuffer = nullptr;
        RTVertexBuffer = nullptr;
        RTIndexBuffer = nullptr;
        RTInstanceInfoBuffer = nullptr;
        StagingTexture = nullptr;
        MeshBLASHandles.clear();
        TLASBuilder.Reset();

        GBufferVS = nullptr;
        GBufferPS = nullptr;
        GBufferDiffuseTexture = nullptr;
        GBufferSpecularTexture = nullptr;
        GBufferNormalsTexture = nullptr;
        GBufferEmissiveTexture = nullptr;
        GBufferWorldPosTexture = nullptr;
        GBufferDepthTexture = nullptr;
        GBufferFramebuffer = nullptr;
        PlaceholderTexture = nullptr;

        AllMeshDrawData.clear();

        GBufferInputLayout = nullptr;
        GBufferBindingLayout = nullptr;
        ViewConstantsBuffer = nullptr;
        GBufferPipeline = nullptr;
        GBufferLinearSampler = nullptr;
    }

    virtual void Animate(float fElapsedTimeSeconds) override
    {
        FrameCount++;
        TemporalFrameCount++;
        FPSUpdateTimer += fElapsedTimeSeconds;
        float FPS = float(FrameCount) / FPSUpdateTimer;
        if (FPSUpdateTimer >= 1.0f)
        {
            WindowTitle = FString::Format(TXT("Few-bounce GI - FPS: {:.1f}"), FPS);

            if (auto* DM = GetDeviceManager())
            {
                DM->SetWindowTitle(WindowTitle);
            }
            FPSUpdateTimer = 0.0f;
            FrameCount = 0;
        }
    }

    virtual void Render(nvrhi::IFramebuffer* Framebuffer) override
    {
        if (!NvrhiDevice || !Framebuffer)
            return;

        const auto& CurrentFBInfo = Framebuffer->getFramebufferInfo();

        // =====================================================================
        // Resize handling for GBuffer textures
        // =====================================================================
        if (!GBufferNormalsTexture || CurrentFBInfo.width != LastWidth || CurrentFBInfo.height != LastHeight)
        {
            HLVM_LOG(LogTest, info, TXT("Resizing GBuffer - width: {}, height: {}"), CurrentFBInfo.width, CurrentFBInfo.height);
            LastWidth = CurrentFBInfo.width;
            LastHeight = CurrentFBInfo.height;

            uint32_t GBufferWidth = CurrentFBInfo.width;
            uint32_t GBufferHeight = CurrentFBInfo.height;

            nvrhi::TextureDesc Desc;
            Desc.dimension = nvrhi::TextureDimension::Texture2D;
            Desc.width = GBufferWidth;
            Desc.height = GBufferHeight;
            Desc.isRenderTarget = true;
            Desc.isUAV = false;
            Desc.initialState = nvrhi::ResourceStates::RenderTarget;
            Desc.keepInitialState = true;

            Desc.format = nvrhi::Format::RGBA8_UNORM;
            Desc.debugName = "GBufferDiffuse";
            GBufferDiffuseTexture = NvrhiDevice->createTexture(Desc);

            Desc.format = nvrhi::Format::RGBA16_FLOAT;
            Desc.debugName = "GBufferSpecular";
            GBufferSpecularTexture = NvrhiDevice->createTexture(Desc);

            Desc.debugName = "GBufferNormals";
            GBufferNormalsTexture = NvrhiDevice->createTexture(Desc);

            Desc.debugName = "GBufferEmissive";
            GBufferEmissiveTexture = NvrhiDevice->createTexture(Desc);

            Desc.debugName = "GBufferWorldPos";
            GBufferWorldPosTexture = NvrhiDevice->createTexture(Desc);

            Desc.format = nvrhi::Format::D32;
            Desc.isTypeless = true;
            Desc.initialState = nvrhi::ResourceStates::DepthWrite;
            Desc.debugName = "GBufferDepth";
            GBufferDepthTexture = NvrhiDevice->createTexture(Desc);

            nvrhi::FramebufferDesc FBDesc;
            FBDesc.addColorAttachment(GBufferDiffuseTexture);
            FBDesc.addColorAttachment(GBufferSpecularTexture);
            FBDesc.addColorAttachment(GBufferNormalsTexture);
            FBDesc.addColorAttachment(GBufferEmissiveTexture);
            FBDesc.addColorAttachment(GBufferWorldPosTexture);
            FBDesc.setDepthAttachment(GBufferDepthTexture);
            GBufferFramebuffer = NvrhiDevice->createFramebuffer(FBDesc);

            // Recreate GBuffer pipeline with new framebuffer info
            {
                nvrhi::GraphicsPipelineDesc PipelineDesc;
                PipelineDesc.setVertexShader(GBufferVS);
                PipelineDesc.setPixelShader(GBufferPS);
                PipelineDesc.setInputLayout(GBufferInputLayout);
                PipelineDesc.addBindingLayout(GBufferBindingLayout);
                PipelineDesc.setPrimType(nvrhi::PrimitiveType::TriangleList);
                PipelineDesc.renderState.setRasterState(nvrhi::RasterState().setCullNone());
                PipelineDesc.renderState.depthStencilState
                    .setDepthTestEnable(true)
                    .setDepthWriteEnable(true)
                    .setDepthFunc(nvrhi::ComparisonFunc::Less);
                GBufferPipeline = NvrhiDevice->createGraphicsPipeline(PipelineDesc, GBufferFramebuffer->getFramebufferInfo());
            }

            // Recreate HDR texture
            Desc.format = nvrhi::Format::RGBA32_FLOAT;
            Desc.isRenderTarget = false;
            Desc.isUAV = true;
            Desc.isTypeless = false;
            Desc.initialState = nvrhi::ResourceStates::UnorderedAccess;
            Desc.debugName = "HDROutput";
            HDRTexture = NvrhiDevice->createTexture(Desc);

            // Recreate denoised HDR texture
            Desc.debugName = "DenoisedHDROutput";
            DenoisedHDRTexture = NvrhiDevice->createTexture(Desc);

            // Recreate staging texture
            {
                nvrhi::TextureDesc StagingDesc;
                StagingDesc.dimension = nvrhi::TextureDimension::Texture2D;
                StagingDesc.width = GBufferWidth;
                StagingDesc.height = GBufferHeight;
                StagingDesc.format = nvrhi::Format::RGBA32_FLOAT;
                StagingDesc.isRenderTarget = false;
                StagingDesc.isUAV = false;
                StagingDesc.isTypeless = false;
                StagingDesc.initialState = nvrhi::ResourceStates::CopyDest;
                StagingDesc.keepInitialState = false;
                StagingDesc.debugName = "StagingTexture";
                StagingTexture = NvrhiDevice->createStagingTexture(StagingDesc, nvrhi::CpuAccessMode::Read);
            }

            BindingCache.Clear();
        }

        // =====================================================================
        // Single command list: GBuffer -> GI -> Blit
        // =====================================================================
        nvrhi::CommandListParameters CmdListParams;
        CmdListParams.enableImmediateExecution = false;
        nvrhi::CommandListHandle CmdList = NvrhiDevice->createCommandList(CmdListParams);
        CmdList->open();

        // =====================================================================
        // Static camera setup - inside Cornell Box looking at back wall
        // =====================================================================
        // Camera sits inside the box (z < 1) and looks toward the back corner
        // with a wide FOV so floor, back wall and both colored side walls are
        // visible for the classic Cornell Box color-bleed effect.
        glm::vec3 CameraPos = SceneCenter + glm::vec3(0.0f, 0.0f, 0.5f);
        glm::vec3 CameraTarget = SceneCenter + glm::vec3(0.0f, -0.2f, -1.0f);
        glm::mat4 view = glm::lookAtLH(CameraPos, CameraTarget, glm::vec3(0.0f, 1.0f, 0.0f));
        float aspectRatio = float(CurrentFBInfo.width) / float(CurrentFBInfo.height);
        glm::mat4 proj = glm::perspectiveLH_ZO(glm::radians(90.0f), aspectRatio, 0.01f, 10.0f);

        // Write ViewConstants (column-major via glm::value_ptr)
        float ViewConstantsData[16 * 3 + 4]; // Model(16) + View(16) + Proj(16) + RenderTargetSize(2) + Padding(2)
        memcpy(&ViewConstantsData[0], glm::value_ptr(glm::scale(glm::mat4(1.0f), glm::vec3(2.0f))), 64);
        memcpy(&ViewConstantsData[16], glm::value_ptr(view), 64);
        memcpy(&ViewConstantsData[32], glm::value_ptr(proj), 64);
        ViewConstantsData[48] = static_cast<float>(CurrentFBInfo.width);
        ViewConstantsData[49] = static_cast<float>(CurrentFBInfo.height);
        ViewConstantsData[50] = static_cast<float>(TemporalFrameCount); // frame index for per-frame GI seed
        ViewConstantsData[51] = 0.0f; // padding
        CmdList->writeBuffer(ViewConstantsBuffer, ViewConstantsData, sizeof(ViewConstantsData));

        // =====================================================================
        // GBuffer Pass: Clear and draw all meshes
        // =====================================================================
        nvrhi::Color clearBlack(0.f, 0.f, 0.f, 0.f);
        nvrhi::Color clearBlue(0.f, 0.f, 1.f, 1.f);
        nvrhi::Color clearNormalUp(0.5f, 1.0f, 0.5f, 1.f);
        nvrhi::utils::ClearColorAttachment(CmdList, GBufferFramebuffer, 0, clearBlue);
        nvrhi::utils::ClearColorAttachment(CmdList, GBufferFramebuffer, 1, clearBlack);
        nvrhi::utils::ClearColorAttachment(CmdList, GBufferFramebuffer, 2, clearNormalUp);
        nvrhi::utils::ClearColorAttachment(CmdList, GBufferFramebuffer, 3, clearBlack);
        nvrhi::utils::ClearColorAttachment(CmdList, GBufferFramebuffer, 4, clearBlack);
        nvrhi::utils::ClearDepthStencilAttachment(CmdList, GBufferFramebuffer, 1.0f, 0u);

        for (size_t MeshIdx = 0; MeshIdx < AllMeshDrawData.size(); ++MeshIdx)
        {
            const auto& DrawData = AllMeshDrawData[MeshIdx];

            // Look up material texture for this mesh
            nvrhi::TextureHandle DiffuseTex = PlaceholderTexture;
            if (MeshIdx < MeshMaterials.size())
            {
                auto PBRMat = MeshMaterials[MeshIdx];
                if (PBRMat && PBRMat->HasGPUTexture(IMaterial::ETextureType::Albedo))
                {
                    DiffuseTex = PBRMat->GetGPUTexture(IMaterial::ETextureType::Albedo).GetTextureSRV();
                    if (!DiffuseTex)
                    {
                        DiffuseTex = PlaceholderTexture;
                    }
                }
            }

            nvrhi::BindingSetDesc GBufferBindingSetDesc;
            GBufferBindingSetDesc.bindings = {
                nvrhi::BindingSetItem::ConstantBuffer(256, ViewConstantsBuffer),
                nvrhi::BindingSetItem::Texture_SRV(0, DiffuseTex),
                nvrhi::BindingSetItem::Sampler(128, GBufferLinearSampler)
            };
            nvrhi::BindingSetHandle GBufferBindingSet = BindingCache.GetOrCreateBindingSet(GBufferBindingSetDesc, GBufferBindingLayout);

            nvrhi::GraphicsState State;
            State.setPipeline(GBufferPipeline);
            State.setFramebuffer(GBufferFramebuffer);
            State.addBindingSet(GBufferBindingSet);

            nvrhi::VertexBufferBinding VBBinding;
            VBBinding.setBuffer(DrawData.VertexBuffer);
            VBBinding.setSlot(0);
            VBBinding.setOffset(0);
            State.addVertexBuffer(VBBinding);

            nvrhi::IndexBufferBinding IBBinding;
            IBBinding.setBuffer(DrawData.IndexBuffer);
            IBBinding.setOffset(0);
            IBBinding.setFormat(nvrhi::Format::R32_UINT);
            State.setIndexBuffer(IBBinding);

            nvrhi::Viewport viewport(0.f, float(CurrentFBInfo.width), 0.f, float(CurrentFBInfo.height), 0.0f, 1.0f);
            State.viewport.addViewportAndScissorRect(viewport);

            CmdList->setGraphicsState(State);

            nvrhi::DrawArguments DrawArgs;
            DrawArgs.vertexCount = DrawData.IndexCount;
            CmdList->drawIndexed(DrawArgs);
        }

        if (FrameCount < 2)
        {
            HLVM_LOG(LogTest, info, TXT("GBuffer Pass: drew {} meshes"), AllMeshDrawData.size());
        }

        // =====================================================================
        // Transition GBuffer textures to ShaderResource for GI pass
        // =====================================================================
        CmdList->setTextureState(GBufferDiffuseTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        CmdList->setTextureState(GBufferSpecularTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        CmdList->setTextureState(GBufferNormalsTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        CmdList->setTextureState(GBufferEmissiveTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        CmdList->setTextureState(GBufferWorldPosTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        CmdList->setTextureState(GBufferDepthTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);

        // =====================================================================
        // GI Pass - Sequential Bounce Tracing
        // =====================================================================

        // Cornell Box uses an emissive ceiling quad for lighting.
        // The analytical directional light is disabled (w=0) so all energy
        // comes from GI bounces off the emissive surface.
        static const float LightAngle = 0.0f;
        glm::vec3 LightDir = glm::normalize(glm::vec3(sinf(LightAngle), -1.0f, cosf(LightAngle)));
        float GIConstantsData[20] = {
            LightDir.x, LightDir.y, LightDir.z, 0.0f,                          // LightDir + intensity (disabled; lighting from emissive ceiling)
            0.6f, 0.6f, 0.65f, 0.0f,                                           // Ambient color
            CameraPos.x, CameraPos.y, CameraPos.z, 1.0f,                        // CameraPos
            static_cast<float>(CVar_r_GI_MaxBounces.GetValue()),
            static_cast<float>(CVar_r_GI_SPP.GetValue()),
            CVar_r_GI_ShadowTMin.GetValue(),
            CVar_r_GI_ShadowTMax.GetValue(),                                    // Params
            CVar_r_GI_AmbientScale.GetValue(),
            CVar_r_GI_RayTMin.GetValue(),
            CVar_r_GI_RayTMax.GetValue(),
            CVar_r_GI_ShadowRays.GetValue() ? 1.0f : 0.0f                     // Params2
        };
        CmdList->writeBuffer(GIConstantBuffer, GIConstantsData, sizeof(GIConstantsData));

        // Transition HDR to UAV for GI write
        CmdList->setTextureState(HDRTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);

        // Create GI binding set using FBindingSetBuilder (shift-aware, matches layout)
        FBindingSetBuilder RTSetBuilder;
        RTSetBuilder.SetConstantBuffer(0, GIConstantBuffer)
            .SetConstantBuffer(1, ViewConstantsBuffer)
            .SetRayTracingAccelStruct(0, TopLevelAS)
            .SetTextureSRV(1, GBufferWorldPosTexture)
            .SetTextureSRV(2, GBufferNormalsTexture)
            .SetTextureSRV(3, GBufferDiffuseTexture)
            .SetStructuredBufferSRV(5, RTVertexBuffer)
            .SetStructuredBufferSRV(6, RTIndexBuffer)
            .SetStructuredBufferSRV(7, RTInstanceInfoBuffer)
            .SetTextureUAV(0, HDRTexture)
            .SetSampler(2, GBufferLinearSampler);
        nvrhi::BindingSetDesc RTBindingSetDesc = RTSetBuilder.Build();
        RTBindingSet = BindingCache.GetOrCreateBindingSet(RTBindingSetDesc, RTPipeline.GetBindingLayout());

        RTPipeline.DispatchRays(CmdList, CurrentFBInfo.width, CurrentFBInfo.height, 1, RTBindingSet, DescTableMgr->GetDescriptorTable());

        // =====================================================================
        // Denoise Pass: Bilateral filter OR ReBLUR on GI output
        // =====================================================================
        // Transition HDR (noisy) to ShaderResource for reading
        CmdList->setTextureState(HDRTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        // Transition depth to ShaderResource for reading
        CmdList->setTextureState(GBufferDepthTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);

        if (bReBLURInitialized)
        {
            // ReBLUR temporal+spatial denoiser
            CmdList->setTextureState(DenoisedHDRTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);
            CmdList->setTextureState(ReBLURHistoryTexture[0], nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);

            ReBLUR::FReBLURConstants ReBLURConstants;
            glm::mat4 currViewProj = proj * view;
            glm::mat4 invCurrViewProj = glm::inverse(currViewProj);
            memcpy(ReBLURConstants.InverseCurrViewProj, glm::value_ptr(invCurrViewProj), 64);
            memcpy(ReBLURConstants.PrevViewProj, glm::value_ptr(PrevViewProj), 64);
            memcpy(ReBLURConstants.ViewMatrix, glm::value_ptr(view), 64);
            memcpy(ReBLURConstants.ProjMatrix, glm::value_ptr(proj), 64);
            ReBLURConstants.OutputSize[0] = static_cast<float>(CurrentFBInfo.width);
            ReBLURConstants.OutputSize[1] = static_cast<float>(CurrentFBInfo.height);
            ReBLURConstants.RcpOutputSize[0] = 1.0f / static_cast<float>(CurrentFBInfo.width);
            ReBLURConstants.RcpOutputSize[1] = 1.0f / static_cast<float>(CurrentFBInfo.height);
            // HitDistParams: A=3.0, B=0.1, C=20.0, D=-25.0 (NRD defaults)
            ReBLURConstants.HitDistParams[0] = 3.0f;
            ReBLURConstants.HitDistParams[1] = 0.1f;
            ReBLURConstants.HitDistParams[2] = 20.0f;
            ReBLURConstants.HitDistParams[3] = -25.0f;
            ReBLURConstants.FrameIndex = static_cast<float>(TemporalFrameCount);
            ReBLURConstants.HistoryFadeIn = CVar_r_ReBLUR_HistoryFadeIn.GetValue();
            ReBLURConstants.ConfidenceScale = 1.0f;
            ReBLURConstants.PassIndex = 0.0f;

            ReBLUR::FPooledBlurParams BlurParams;
            BlurParams.BlurRadius = CVar_r_ReBLUR_BlurRadius.GetValue();
            BlurParams.NormalWeight = CVar_r_ReBLUR_NormalWeight.GetValue();
            BlurParams.PlaneWeight = CVar_r_ReBLUR_PlaneWeight.GetValue();
            BlurParams.SpatialAlpha = CVar_r_ReBLUR_SpatialAlpha.GetValue();
            BlurParams.AntiLagIntensity = CVar_r_ReBLUR_AntiLag.GetValue();

            ReBLUR::FReBLURPass::FDesc ReBLURDesc;
            ReBLURDesc.CurrentRadianceTexture = HDRTexture;
            ReBLURDesc.HistoryTexture = ReBLURHistoryTexture[0];
            ReBLURDesc.DepthTexture = GBufferDepthTexture;
            ReBLURDesc.NormalRoughnessTexture = GBufferNormalsTexture;
            ReBLURDesc.OutputTexture = DenoisedHDRTexture;
            ReBLURDesc.OutputWidth = CurrentFBInfo.width;
            ReBLURDesc.OutputHeight = CurrentFBInfo.height;
            ReBLURPass.Dispatch(CmdList, ReBLURDesc, ReBLURConstants, BlurParams);

            // Copy denoised output to history for next frame
            CmdList->setTextureState(DenoisedHDRTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::CopySource);
            CmdList->setTextureState(ReBLURHistoryTexture[0], nvrhi::AllSubresources, nvrhi::ResourceStates::CopyDest);
            CmdList->copyTexture(ReBLURHistoryTexture[0], nvrhi::TextureSlice(), DenoisedHDRTexture, nvrhi::TextureSlice());
        }
        else if (CVar_r_GI_Denoise.GetValue())
        {
            // Transition DenoisedHDR to UAV for writing
            CmdList->setTextureState(DenoisedHDRTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);

            FBilateralDenoisePass::FDesc DenoiseDesc;
            DenoiseDesc.InputTexture = HDRTexture;
            DenoiseDesc.DepthTexture = GBufferDepthTexture;
            DenoiseDesc.NormalTexture = GBufferNormalsTexture;
            DenoiseDesc.OutputTexture = DenoisedHDRTexture;
            DenoiseDesc.OutputWidth = CurrentFBInfo.width;
            DenoiseDesc.OutputHeight = CurrentFBInfo.height;
            DenoiseDesc.DepthSigma = 0.01f;
            DenoiseDesc.NormalSigma = 0.1f;
            DenoiseDesc.SpatialSigma = CVar_r_GI_DenoiseSigma.GetValue();
            DenoisePass.Dispatch(CmdList, DenoiseDesc);
        }
        else
        {
            // No denoising: copy HDR directly to denoised
            CmdList->setTextureState(HDRTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::CopySource);
            CmdList->setTextureState(DenoisedHDRTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::CopyDest);
            CmdList->copyTexture(DenoisedHDRTexture, nvrhi::TextureSlice(), HDRTexture, nvrhi::TextureSlice());
        }

        // =====================================================================
        // ReSTIR Pass: Generation -> Temporal -> Spatial
        // =====================================================================
        if (bReSTIRInitialized)
        {
            // Transition inputs to ShaderResource
            CmdList->setTextureState(DenoisedHDRTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
            CmdList->setTextureState(GBufferWorldPosTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
            CmdList->setTextureState(GBufferNormalsTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
            CmdList->setTextureState(GBufferDepthTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);

            // Generation: write to Reservoir0/1
            CmdList->setTextureState(Reservoir0Texture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);
            CmdList->setTextureState(Reservoir1Texture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);

            ReSTIR::FReSTIRConstants GenConstants;
            GenConstants.OutputSize[0] = static_cast<float>(CurrentFBInfo.width);
            GenConstants.OutputSize[1] = static_cast<float>(CurrentFBInfo.height);
            GenConstants.RcpOutputSize[0] = 1.0f / static_cast<float>(CurrentFBInfo.width);
            GenConstants.RcpOutputSize[1] = 1.0f / static_cast<float>(CurrentFBInfo.height);
            GenConstants.FrameIndex = static_cast<float>(TemporalFrameCount);
            GenConstants.NumCandidates = static_cast<float>(CVar_r_ReSTIR_NumCandidates.GetValue());
            GenConstants.DepthThreshold = CVar_r_ReSTIR_DepthThreshold.GetValue();
            GenConstants.NormalThreshold = CVar_r_ReSTIR_NormalThreshold.GetValue();
            GenConstants.DebugVis = 0.0f;

            ReSTIR::FReSTIRPass::FGenerationDesc GenDesc;
            GenDesc.RadianceTexture = DenoisedHDRTexture;
            GenDesc.WorldPosTexture = GBufferWorldPosTexture;
            GenDesc.NormalTexture = GBufferNormalsTexture;
            GenDesc.DepthTexture = GBufferDepthTexture;
            GenDesc.OutReservoir0 = Reservoir0Texture;
            GenDesc.OutReservoir1 = Reservoir1Texture;
            GenDesc.OutputWidth = CurrentFBInfo.width;
            GenDesc.OutputHeight = CurrentFBInfo.height;
            ReSTIRPass.DispatchGeneration(CmdList, GenDesc, GenConstants);

            // Transition generation outputs to ShaderResource for temporal
            CmdList->setTextureState(Reservoir0Texture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
            CmdList->setTextureState(Reservoir1Texture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);

            // Transition temporal outputs to UAV
            CmdList->setTextureState(Reservoir0MergedTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);
            CmdList->setTextureState(Reservoir1MergedTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);
            CmdList->setTextureState(TemporalRadianceTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);

            // Transition history to ShaderResource for temporal reading
            CmdList->setTextureState(Reservoir0HistoryTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
            CmdList->setTextureState(Reservoir1HistoryTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
            // ReSTIROutputTexture from previous frame serves as radiance history
            CmdList->setTextureState(ReSTIROutputTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);

            // Temporal constants
            ReSTIR::FReSTIRTemporalConstants TempConstants;
            glm::mat4 currViewProj = proj * view;
            glm::mat4 invCurrViewProj = glm::inverse(currViewProj);
            memcpy(TempConstants.InverseCurrViewProj, glm::value_ptr(invCurrViewProj), 64);
            memcpy(TempConstants.PrevViewProj, glm::value_ptr(PrevViewProj), 64);
            TempConstants.OutputSize[0] = static_cast<float>(CurrentFBInfo.width);
            TempConstants.OutputSize[1] = static_cast<float>(CurrentFBInfo.height);
            TempConstants.RcpOutputSize[0] = 1.0f / static_cast<float>(CurrentFBInfo.width);
            TempConstants.RcpOutputSize[1] = 1.0f / static_cast<float>(CurrentFBInfo.height);
            TempConstants.FrameIndex = static_cast<float>(TemporalFrameCount);
            TempConstants.MaxM = CVar_r_ReSTIR_MaxM.GetValue();
            TempConstants.DepthThreshold = CVar_r_ReSTIR_DepthThreshold.GetValue();
            TempConstants.NormalThreshold = CVar_r_ReSTIR_NormalThreshold.GetValue();
            TempConstants.DebugVis = 0.0f;

            ReSTIR::FReSTIRPass::FTemporalDesc TempDesc;
            TempDesc.CurrentReservoir0 = Reservoir0Texture;
            TempDesc.CurrentReservoir1 = Reservoir1Texture;
            TempDesc.HistoryReservoir0 = Reservoir0HistoryTexture;
            TempDesc.HistoryReservoir1 = Reservoir1HistoryTexture;
            TempDesc.CurrentRadiance = DenoisedHDRTexture;
            TempDesc.HistoryRadiance = ReSTIROutputTexture;
            TempDesc.DepthTexture = GBufferDepthTexture;
            TempDesc.NormalTexture = GBufferNormalsTexture;
            TempDesc.PrevDepthTexture = PrevDepthTexture;
            TempDesc.PrevNormalTexture = PrevNormalTexture;
            TempDesc.OutReservoir0 = Reservoir0MergedTexture;
            TempDesc.OutReservoir1 = Reservoir1MergedTexture;
            TempDesc.OutRadiance = TemporalRadianceTexture;
            TempDesc.OutputWidth = CurrentFBInfo.width;
            TempDesc.OutputHeight = CurrentFBInfo.height;
            if (CVar_r_ReSTIR_EnableTemporal.GetValue())
            {
                ReSTIRPass.DispatchTemporal(CmdList, TempDesc, TempConstants);

                // Swap history with merged for next frame
                std::swap(Reservoir0HistoryTexture, Reservoir0MergedTexture);
                std::swap(Reservoir1HistoryTexture, Reservoir1MergedTexture);
            }
            else
            {
                // Temporal disabled: use generation output directly as "history" for spatial
                CmdList->copyTexture(Reservoir0HistoryTexture, nvrhi::TextureSlice(), Reservoir0Texture, nvrhi::TextureSlice());
                CmdList->copyTexture(Reservoir1HistoryTexture, nvrhi::TextureSlice(), Reservoir1Texture, nvrhi::TextureSlice());
            }

            // Spatial: evaluate merged reservoirs, write to ReSTIROutput
            CmdList->setTextureState(ReSTIROutputTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);

            if (CVar_r_ReSTIR_EnableSpatial.GetValue())
            {
                ReSTIR::FReSTIRSpatialConstants SpatConstants;
                SpatConstants.OutputSize[0] = static_cast<float>(CurrentFBInfo.width);
                SpatConstants.OutputSize[1] = static_cast<float>(CurrentFBInfo.height);
                SpatConstants.RcpOutputSize[0] = 1.0f / static_cast<float>(CurrentFBInfo.width);
                SpatConstants.RcpOutputSize[1] = 1.0f / static_cast<float>(CurrentFBInfo.height);
                SpatConstants.NormalThreshold = CVar_r_ReSTIR_NormalThreshold.GetValue();
                SpatConstants.DepthThreshold = CVar_r_ReSTIR_DepthThreshold.GetValue();
                SpatConstants.MaxM = CVar_r_ReSTIR_MaxM.GetValue();
                SpatConstants.SpatialRadius = CVar_r_ReSTIR_SpatialRadius.GetValue();
                SpatConstants.DebugVis = 0.0f;

                ReSTIR::FReSTIRPass::FSpatialDesc SpatDesc;
                SpatDesc.RadianceTexture = DenoisedHDRTexture;
                SpatDesc.Reservoir0 = Reservoir0HistoryTexture;
                SpatDesc.Reservoir1 = Reservoir1HistoryTexture;
                SpatDesc.NormalTexture = GBufferNormalsTexture;
                SpatDesc.DepthTexture = GBufferDepthTexture;
                SpatDesc.OutRadiance = ReSTIROutputTexture;
                SpatDesc.OutputWidth = CurrentFBInfo.width;
                SpatDesc.OutputHeight = CurrentFBInfo.height;
                ReSTIRPass.DispatchSpatial(CmdList, SpatDesc, SpatConstants);
            }
            else
            {
                // Spatial disabled: copy denoised HDR directly to output
                CmdList->copyTexture(ReSTIROutputTexture, nvrhi::TextureSlice(), DenoisedHDRTexture, nvrhi::TextureSlice());
            }

            // Copy current frame depth/normal for next frame's temporal reprojection
            CmdList->copyTexture(PrevDepthTexture, nvrhi::TextureSlice(), GBufferDepthTexture, nvrhi::TextureSlice());
            CmdList->copyTexture(PrevNormalTexture, nvrhi::TextureSlice(), GBufferNormalsTexture, nvrhi::TextureSlice());
            PrevViewProj = currViewProj;
        }

        // =====================================================================
        // Frame dump: copy output to staging (before blit, on same command list)
        // =====================================================================
        static bool dumpRequested = !!getenv("HLVM_DUMP_GI");
        static int framesToDump = 0;
        bool doDumpThisFrame = dumpRequested && framesToDump < 4;
        nvrhi::TextureHandle DumpTexture = bReSTIRInitialized ? ReSTIROutputTexture : DenoisedHDRTexture;
        if (doDumpThisFrame) {
            framesToDump++;
            CmdList->setTextureState(DumpTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::CopySource);
            nvrhi::TextureSlice slice = {};
            slice.width = CurrentFBInfo.width;
            slice.height = CurrentFBInfo.height;
            slice.depth = 1;
            CmdList->copyTexture(StagingTexture.Get(), slice, DumpTexture.Get(), slice);
        }

        // =====================================================================
        // Always blit denoised output to screen
        // =====================================================================
        // Defensive clear: if the blit misses pixels, show black instead of
        // uninitialized swapchain memory (can look like noise flicker).
        nvrhi::Color swapchainClearBlack(0.f, 0.f, 0.f, 1.f);
        nvrhi::utils::ClearColorAttachment(CmdList, Framebuffer, 0, swapchainClearBlack);

        CmdList->setTextureState(DumpTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        FCommonRenderPasses::BlitParameters BlitParams;
        FCommonRenderPasses::BlitTexture(
            CmdList,
            Framebuffer,
            DumpTexture,
            &BindingCache,
            CurrentFBInfo.width,
            CurrentFBInfo.height,
            BlitParams);

        CmdList->close();
        NvrhiDevice->executeCommandList(CmdList);
        NvrhiDevice->waitForIdle();

        // =====================================================================
        // Readback and save frame dump (after GPU is idle)
        // =====================================================================
        if (doDumpThisFrame) {
            nvrhi::TextureSlice slice = {};
            slice.width = CurrentFBInfo.width;
            slice.height = CurrentFBInfo.height;
            slice.depth = 1;

            size_t rowPitch = 0;
            void* mappedData = NvrhiDevice->mapStagingTexture(
                StagingTexture.Get(), slice, nvrhi::CpuAccessMode::Read, &rowPitch);

            if (mappedData) {
                uint32_t imgWidth = CurrentFBInfo.width;
                uint32_t imgHeight = CurrentFBInfo.height;

                std::vector<float> pixels(static_cast<size_t>(imgWidth) * imgHeight * 4);
                uint8_t* srcRow = reinterpret_cast<uint8_t*>(mappedData);
                float* dst = pixels.data();

                for (uint32_t y = 0; y < imgHeight; y++) {
                    float* src = reinterpret_cast<float*>(srcRow + static_cast<size_t>(imgHeight - 1 - y) * rowPitch);
                    for (uint32_t x = 0; x < imgWidth; x++) {
                        size_t dstIdx = (static_cast<size_t>(y) * imgWidth + x) * 4;
                        size_t srcIdx = x * 4;
                        dst[dstIdx + 0] = src[srcIdx + 0];
                        dst[dstIdx + 1] = src[srcIdx + 1];
                        dst[dstIdx + 2] = src[srcIdx + 2];
                        dst[dstIdx + 3] = src[srcIdx + 3];
                    }
                }

                NvrhiDevice->unmapStagingTexture(StagingTexture.Get());

                // Use GProjectRoot for correct path, create dir if needed
                FString DumpDir = FString::Format(TXT("{}/Engine/Source/Runtime/Test/TestCornellBoxGI_Data/dumps"), *GProjectRoot);
                std::filesystem::create_directories(FPath(DumpDir).string());
                FString baseFilename = FImageDump::GenerateTimestampedFilename(DumpDir);
                FString filename = FString::Format(TXT("{}_frame{:04d}.png"), baseFilename.substr(0, baseFilename.length() - 4).c_str(), framesToDump);
                if (FImageDump::DumpToPNG(filename, static_cast<int>(imgWidth), static_cast<int>(imgHeight), pixels.data())) {
                    HLVM_LOG(LogTest, info, TXT("Dumped frame {} to {}"), framesToDump, *filename);
                } else {
                    HLVM_LOG(LogTest, critical, TXT("Failed to dump frame to {}"), *filename);
                }
            }

            if (framesToDump == 4) {
                return;
            }
        }
    }

    virtual void BackBufferResizing() override
    {
        HDRTexture = nullptr;
        DenoisedHDRTexture = nullptr;
        StagingTexture = nullptr;
        Reservoir0Texture = nullptr;
        Reservoir1Texture = nullptr;
        Reservoir0HistoryTexture = nullptr;
        Reservoir1HistoryTexture = nullptr;
        Reservoir0MergedTexture = nullptr;
        Reservoir1MergedTexture = nullptr;
        ReSTIROutputTexture = nullptr;
        TemporalRadianceTexture = nullptr;
        RadianceHistoryTexture = nullptr;
        ReBLURHistoryTexture[0] = nullptr;
        ReBLURHistoryTexture[1] = nullptr;
        PrevDepthTexture = nullptr;
        PrevNormalTexture = nullptr;
        RTBindingSet = nullptr;
        GBufferFramebuffer = nullptr;
        GBufferDiffuseTexture = nullptr;
        GBufferSpecularTexture = nullptr;
        GBufferNormalsTexture = nullptr;
        GBufferEmissiveTexture = nullptr;
        GBufferWorldPosTexture = nullptr;
        GBufferDepthTexture = nullptr;
        BindingCache.Clear();
    }

private:
    nvrhi::IDevice*        NvrhiDevice = nullptr;
    nvrhi::FramebufferInfo FBInfo;
    FString                WindowTitle;

    // Scene
    glm::vec3 SceneCenter = glm::vec3(0.f);
    TVector<std::shared_ptr<FStaticMesh>> StaticMeshes;
    TVector<std::shared_ptr<FPBRMaterial>> MeshMaterials;

    // Cornell Box configuration
    static constexpr float CBExtent = 1.0f; // half-extent; box spans [-1,1] on each axis

    // Acceleration structures
    nvrhi::rt::AccelStructHandle          TopLevelAS;
    TVector<nvrhi::rt::AccelStructHandle> MeshBLASHandles;
    FTLASBuilder                          TLASBuilder;

    // GI ray tracing pipeline (encapsulates shaders, layout, pipeline, shader table)
    FRayTracingPipeline          RTPipeline;
    nvrhi::BindingSetHandle      RTBindingSet;

    // Bindless texture support
    FTextureCache                TextureCache;
    std::shared_ptr<FDescriptorTableManager> DescTableMgr;
    nvrhi::BindingLayoutHandle   BindlessLayout;
    nvrhi::TextureHandle         HDRTexture;
    nvrhi::TextureHandle         DenoisedHDRTexture;  // Denoised output

    // Constant buffer for GI
    nvrhi::BufferHandle GIConstantBuffer;

    // Ray tracing mesh data buffers for hit shader normal lookup
    nvrhi::BufferHandle RTVertexBuffer;
    nvrhi::BufferHandle RTIndexBuffer;
    nvrhi::BufferHandle RTInstanceInfoBuffer;

    // GBuffer shaders
    nvrhi::ShaderHandle GBufferVS;
    nvrhi::ShaderHandle GBufferPS;

    // GBuffer textures
    nvrhi::TextureHandle GBufferDiffuseTexture;
    nvrhi::TextureHandle GBufferSpecularTexture;
    nvrhi::TextureHandle GBufferNormalsTexture;
    nvrhi::TextureHandle GBufferEmissiveTexture;
    nvrhi::TextureHandle GBufferWorldPosTexture;
    nvrhi::TextureHandle GBufferDepthTexture;
    nvrhi::SamplerHandle GBufferLinearSampler;
    nvrhi::TextureHandle PlaceholderTexture;

    // GBuffer framebuffer
    nvrhi::FramebufferHandle GBufferFramebuffer;

    // Per-mesh data for multi-mesh rendering
    struct FMeshDrawData {
        nvrhi::BufferHandle VertexBuffer;
        nvrhi::BufferHandle IndexBuffer;
        std::shared_ptr<FStaticMesh> Mesh;
        uint32_t IndexCount;
    };
    TVector<FMeshDrawData> AllMeshDrawData;

    // GBuffer pipeline
    nvrhi::InputLayoutHandle      GBufferInputLayout;
    nvrhi::BindingLayoutHandle    GBufferBindingLayout;
    nvrhi::BufferHandle           ViewConstantsBuffer;
    nvrhi::GraphicsPipelineHandle GBufferPipeline;

    FBindingCache            BindingCache;
    nvrhi::CommandListHandle CommandList;

    // Denoise pass (bilateral filter on GI output)
    FBilateralDenoisePass DenoisePass;

    // ReBLUR pass
    ReBLUR::FReBLURPass ReBLURPass;
    nvrhi::TextureHandle ReBLURHistoryTexture[2];
    bool bReBLURInitialized = false;

    // ReSTIR pass
    ReSTIR::FReSTIRPass ReSTIRPass;
    nvrhi::TextureHandle Reservoir0Texture;
    nvrhi::TextureHandle Reservoir1Texture;
    nvrhi::TextureHandle Reservoir0HistoryTexture;
    nvrhi::TextureHandle Reservoir1HistoryTexture;
    nvrhi::TextureHandle Reservoir0MergedTexture;
    nvrhi::TextureHandle Reservoir1MergedTexture;
    nvrhi::TextureHandle ReSTIROutputTexture;
    nvrhi::TextureHandle TemporalRadianceTexture;
    nvrhi::TextureHandle RadianceHistoryTexture;
    nvrhi::TextureHandle PrevDepthTexture;
    nvrhi::TextureHandle PrevNormalTexture;
    bool bReSTIRInitialized = false;

    glm::mat4 PrevViewProj = glm::mat4(1.0f);

    // Staging texture for frame dumps
    nvrhi::StagingTextureHandle StagingTexture;

    uint32_t LastWidth = 0;
    uint32_t LastHeight = 0;
    uint32_t FrameCount = 0;
    uint32_t TemporalFrameCount = 0;
    float    FPSUpdateTimer = 0.0f;
};

// =============================================================================
// TEST IMPLEMENTATION
// =============================================================================

RECORD_BOOL(test_FewBounceGI)
{
    HLVM_LOG(LogTest, info, TXT("=== Starting Few-bounce GI Test ==="));

    // Load CVar overrides from INI (Engine.ini / Game.ini / System.ini)
    GetCVarManager().AddIniSearchPath(GExecutablePath);
    GetCVarManager().LoadAllFromIni();

    try
    {
        HLVM_LOG(LogTest, info, TXT("Creating window..."));
        IWindow::Properties WindowProps;
        WindowProps.Title = WINDOW_TITLE;
        WindowProps.Extent = { WINDOW_WIDTH, WINDOW_HEIGHT };
        WindowProps.Resizable = true;
        WindowProps.VSync = IWindow::EVsync::Off;

        HLVM_LOG(LogTest, info, TXT("Creating DeviceManager..."));
        TUniquePtr<FDeviceManager> DeviceManager = FDeviceManager::Create(nvrhi::GraphicsAPI::VULKAN);
        if (!DeviceManager)
        {
            throw std::runtime_error("Failed to create DeviceManager");
        }

        FDeviceCreationParameters& DeviceParams = const_cast<FDeviceCreationParameters&>(
            DeviceManager->GetDeviceParams());
        DeviceParams.BackBufferWidth = WINDOW_WIDTH;
        DeviceParams.BackBufferHeight = WINDOW_HEIGHT;
        DeviceParams.SwapChainBufferCount = 2;
        DeviceParams.VSyncMode = 0;
        DeviceParams.bEnableDebugRuntime = false;
        DeviceParams.bEnableNVRHIValidationLayer = true;
        DeviceParams.bEnableRayTracingExtensions = true;
        // Suppress known validation layer false positive
        DeviceParams.IgnoredVulkanValidationMessageLocations.push_back(0x29056f6a);


        if (!DeviceManager->CreateWindowDeviceAndSwapChain(WindowProps))
        {
            throw std::runtime_error("Failed to create window, device and swap chain");
        }

        HLVM_LOG(LogTest, info, TXT("Device created with ray tracing enabled"));

        nvrhi::IDevice* NvrhiDevice = DeviceManager->GetDevice();
        if (!NvrhiDevice->queryFeatureSupport(nvrhi::Feature::RayTracingPipeline))
        {
            throw std::runtime_error("Ray tracing not supported on this device");
        }

        nvrhi::IFramebuffer* FirstFB = DeviceManager->GetFramebuffer(0);

        HLVM_LOG(LogTest, info, TXT("Creating render pass..."));
        TSharedPtr<FCornellBoxGIPass> GIPass =
            std::make_shared<FCornellBoxGIPass>(DeviceManager.get());
        if (!GIPass->Initialize(NvrhiDevice, FirstFB, FString(TXT("Few-bounce GI Test"))))
        {
            throw std::runtime_error("Failed to initialize FFewBounceGIPass");
        }

        DeviceManager->AddRenderPassToBack(GIPass);

        HLVM_LOG(LogTest, info, TXT("Starting render loop..."));

        std::thread([&]() {
            FTimer Timer;
            while (Timer.MarkSec() < 5.0)
            {
            }
            DeviceManager->StopMessageLoop();
        }).detach();

        DeviceManager->RunMessageLoop();

        GIPass->Shutdown();

        HLVM_LOG(LogTest, info, TXT("Test completed successfully!"));
        return true;
    }
    catch (const std::exception& e)
    {
        HLVM_LOG(LogTest, critical, TXT("Test failed: {}"), TO_TCHAR_CSTR(e.what()));
        return false;
    }
    catch (...)
    {
        HLVM_LOG(LogTest, critical, TXT("Unknown fatal error occurred"));
        return false;
    }
}

#else // HLVM_VULKAN_RENDERER

RECORD_BOOL(test_FewBounceGI)
{
    HLVM_LOG(LogTest, warning, TXT("Vulkan renderer not enabled - skipping test"));
    return true;
}

#endif // HLVM_VULKAN_RENDERER
