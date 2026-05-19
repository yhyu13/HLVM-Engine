/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 *
 * TestSponzaDeferred - Deferred Rendering with Sponza Scene
 *
 * Pipeline:
 * 1. Load Sponza scene geometry
 * 2. GBuffer Pass: Render Sponza geometry to 4 MRTs (Diffuse, Specular, Normal, Emissive)
 * 3. Lighting Pass: Compute shader PBR lighting
 * 4. Blit Pass: Copy to swapchain via FCommonRenderPasses::BlitTexture
 *
 * This test validates:
 * - Sponza scene loading
 * - GBuffer rendering with Sponza geometry
 * - Deferred PBR lighting pipeline
 */

#include "Test.h"
#include "Renderer/DeviceManager.h"
#include "Renderer/RHI/Object/Buffer.h"
#include "Renderer/Common/FBindingCache.h"
#include "Renderer/Common/FCommonRenderPasses.h"
#include "Renderer/SceneGraph/FNode.h"
#include "Renderer/Scene3D/Scene3DLoader.h"
#include "Renderer/Mesh/StaticMesh.h"
#include "Renderer/Material/PBRMaterial.h"
#include "Renderer/ShaderMake/ShaderBlob.h"
#include "Image/FImageDump.h"
#include "Image/FRenderPassDumper.h"
#include "Renderer/PostProcess/FJointBilateralUpsamplePass.h"
#include "Renderer/PostProcess/FSSAOPass.h"
#include "Renderer/PostProcess/FBloomPass.h"
#include "Renderer/Deferred/FDeferredLightingPass.h"
#include "Renderer/Deferred/FGBufferFillPass.h"
#include "Renderer/PostProcess/FToneMappingPass.h"
#include "Renderer/Shadow/FShadowMapPass.h"
#include <nvrhi/utils.h>
#include <unistd.h>
#include <climits>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

DECLARE_LOG_CATEGORY(LogTest)

#if HLVM_VULKAN_RENDERER

// =============================================================================
// CONFIGURATION
// =============================================================================

static const char*	  WINDOW_TITLE = "Sponza Deferred - Scene Test";
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

	size_t			  fileSize = static_cast<size_t>(file.tellg());
	std::vector<char> buffer(fileSize);

	file.seekg(0);
	file.read(buffer.data(), static_cast<std::streamsize>(fileSize));
	file.close();

	return buffer;
}

// =============================================================================
// FSponzaDeferredPass
// =============================================================================

class FSponzaDeferredPass : public IRenderPass
{
public:
	using IRenderPass::IRenderPass;

	bool Initialize(nvrhi::IDevice* Device, nvrhi::IFramebuffer* Framebuffer, const FString& InWindowTitle)
	{
		HLVM_LOG(LogTest, info, TXT("=== FSponzaDeferredPass::Initialize ==="));

		NvrhiDevice = Device;
		FBInfo = Framebuffer->getFramebufferInfo();
		WindowTitle = InWindowTitle;

		BindingCache.SetDevice(NvrhiDevice);

		const auto ShaderDataDir = FString::Format(
			TXT("{}/Engine/Source/Runtime/Test/TestSponzaDeferred_Data"),
			*GProjectRoot);

		// =====================================================================
		// Initialize HBAO pass
		// =====================================================================
		HLVM_LOG(LogTest, info, TXT("Initializing HBAO pass..."));
		const FString CommonShaderDir = FString::Format(
			TXT("{}/Engine/Source/Runtime/Shader"),
			*GProjectRoot);
		if (!HBAOPass.Initialize(NvrhiDevice, CommonShaderDir))
		{
			HLVM_LOG(LogTest, err, TXT("Failed to initialize HBAO pass"));
			return false;
		}
		HLVM_LOG(LogTest, info, TXT("HBAO pass initialized successfully"));

		// =====================================================================
		// Initialize deferred lighting pass
		// =====================================================================
		HLVM_LOG(LogTest, info, TXT("Initializing deferred lighting pass..."));
		if (!LightingPass.Initialize(NvrhiDevice, ShaderDataDir))
		{
			HLVM_LOG(LogTest, err, TXT("Failed to initialize deferred lighting pass"));
			return false;
		}
		HLVM_LOG(LogTest, info, TXT("Deferred lighting pass initialized successfully"));

		// =====================================================================
		// Initialize tone mapping pass
		// =====================================================================
		HLVM_LOG(LogTest, info, TXT("Initializing tone mapping pass..."));
		if (!ToneMapPass.Initialize(NvrhiDevice, ShaderDataDir))
		{
			HLVM_LOG(LogTest, err, TXT("Failed to initialize tone mapping pass"));
			return false;
		}
		HLVM_LOG(LogTest, info, TXT("Tone mapping pass initialized successfully"));

		// =====================================================================
		// Create command list for initialization
		// =====================================================================
		nvrhi::CommandListHandle InitCmdList = NvrhiDevice->createCommandList();
		InitCmdList->open();

		// =====================================================================
		// Load Sponza scene
		// =====================================================================
		HLVM_LOG(LogTest, info, TXT("Loading Sponza scene..."));
		const FString GitRoot = FString::Format(TXT("{}"), *GProjectRoot);
		// Note: Sponza.glTF uses JPEG textures but stb_image in vcpkg has STBI_NO_JPEG defined.
		// PNG textures load correctly via stbi_load. KTX textures use non-Basis compression.
		const FPath ScenePath = FPath(FString::Format(
			TXT("{}/Samples/Assets/Sponza/glTF/Sponza.gltf"), *GitRoot));

		Scene = FScene3DLoader::LoadFromFile(ScenePath);
		if (!Scene || Scene->IsEmpty())
		{
			HLVM_LOG(LogTest, err, TXT("Failed to load Sponza scene"));
			return false;
		}

		auto StaticMeshes = Scene->GetAllStaticMesh();
		auto Materials = Scene->GetAllMaterial();
		HLVM_LOG(LogTest, info, TXT("Loaded scene with {} meshes, {} materials"),
			StaticMeshes.size(), Materials.size());

		// Calculate scene center from bounding box (like TestRTShadowsGBuffer)
		BBoxMin = glm::vec3(FLT_MAX, FLT_MAX, FLT_MAX);
		BBoxMax = glm::vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
		for (const auto& Mesh : StaticMeshes)
		{
			for (auto& vert : Mesh->GetVertices())
			{
				glm::vec3 pos(vert.Position.x, vert.Position.y, vert.Position.z);
				BBoxMin = glm::min(BBoxMin, pos);
				BBoxMax = glm::max(BBoxMax, pos);
			}
		}
		SceneCenter = (BBoxMin + BBoxMax) * 0.5f;
		float sceneRadius = glm::length(BBoxMax - BBoxMin) * 0.5f;
		HLVM_LOG(LogTest, info, TXT("Scene center: ({:.2f}, {:.2f}, {:.2f}), radius: {:.2f}"),
			SceneCenter.x, SceneCenter.y, SceneCenter.z, sceneRadius);

		InitCmdList->close();
		NvrhiDevice->executeCommandList(InitCmdList);

		// =====================================================================
		// Load PBR textures for materials
		// =====================================================================
		HLVM_LOG(LogTest, info, TXT("Loading PBR textures..."));

		// Try to load albedo texture from glTF material
		// Note: Try all textures including JPG - stb_image can handle most JPEG formats
		for (auto& Mat : Materials)
		{
			if (Mat->HasTexture(IMaterial::ETextureType::Albedo))
			{
				FPath TexPath = Mat->GetTexturePath(IMaterial::ETextureType::Albedo);

				HLVM_LOG(LogTest, info, TXT("Loading albedo texture: {}"), *TexPath);

				// Create fresh command list for each texture load
				// Note: LoadTexture closes and executes the command list internally
				nvrhi::CommandListHandle TexCmdList = NvrhiDevice->createCommandList();
				TexCmdList->open();

				bool LoadSuccess = Mat->LoadTexture(IMaterial::ETextureType::Albedo, NvrhiDevice, TexCmdList);

				// LoadTexture closes and executes TexCmdList internally, so don't do it here

				if (LoadSuccess)
				{
					// Get the loaded GPU texture - use GetTextureSRV() for shader resource binding
					FTexture& GPUTex = Mat->GetGPUTexture(IMaterial::ETextureType::Albedo);
					if (!LoadedDiffuseTexture)
					{
						LoadedDiffuseTexture = GPUTex.GetTextureSRV();
					}
					HLVM_LOG(LogTest, info, TXT("Successfully loaded albedo texture: {}x{}"),
						GPUTex.GetWidth(), GPUTex.GetHeight());
					// Continue loading other textures too
				}
				else
				{
					HLVM_LOG(LogTest, warn, TXT("Failed to load albedo texture: {}"), *TexPath);
				}
			}
		}

		// Load normal maps for materials that have them
		for (auto& Mat : Materials)
		{
			if (Mat->HasTexture(IMaterial::ETextureType::Normal))
			{
				FPath TexPath = Mat->GetTexturePath(IMaterial::ETextureType::Normal);
				HLVM_LOG(LogTest, info, TXT("Loading normal texture: {}"), *TexPath);
				nvrhi::CommandListHandle TexCmdList = NvrhiDevice->createCommandList();
				TexCmdList->open();
				bool LoadSuccess = Mat->LoadTexture(IMaterial::ETextureType::Normal, NvrhiDevice, TexCmdList);
				if (LoadSuccess)
				{
					FTexture& GPUTex = Mat->GetGPUTexture(IMaterial::ETextureType::Normal);
					HLVM_LOG(LogTest, info, TXT("Successfully loaded normal texture: {}x{}"),
						GPUTex.GetWidth(), GPUTex.GetHeight());
				}
				else
				{
					HLVM_LOG(LogTest, warn, TXT("Failed to load normal texture: {}"), *TexPath);
				}
			}
		}

		// If no texture loaded, use placeholder
		if (!LoadedDiffuseTexture)
		{
			HLVM_LOG(LogTest, info, TXT("Using placeholder texture (1x1 white)"));
		}

		// =====================================================================
		// Initialize GBuffer fill pass
		// =====================================================================
		HLVM_LOG(LogTest, info, TXT("Initializing GBuffer fill pass..."));
		{
			FGBufferFillPass::FDesc GBufferDesc;
			GBufferDesc.Width = Framebuffer->getFramebufferInfo().width;
			GBufferDesc.Height = Framebuffer->getFramebufferInfo().height;
			if (!GBufferPass.Initialize(NvrhiDevice, ShaderDataDir, GBufferDesc))
			{
				HLVM_LOG(LogTest, err, TXT("Failed to initialize GBuffer fill pass"));
				return false;
			}
		}
		HLVM_LOG(LogTest, info, TXT("GBuffer fill pass initialized successfully"));

		// =====================================================================
		// Create Sponza geometry buffers (per-mesh)
		// =====================================================================
		HLVM_LOG(LogTest, info, TXT("Creating Sponza geometry buffers for {} meshes..."), StaticMeshes.size());

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

			HLVM_LOG(LogTest, info, TXT("Mesh[{}]: {} vertices, {} indices"),
				i, Vertices.size(), Indices.size());
		}

		// Store first mesh for backward compatibility
		if (!AllMeshDrawData.empty())
		{
			SponzaMesh = AllMeshDrawData[0].Mesh;
			SponzaIndexCount = AllMeshDrawData[0].IndexCount;
			SponzaVertexBuffer = AllMeshDrawData[0].VertexBuffer;
			SponzaIndexBuffer = AllMeshDrawData[0].IndexBuffer;
		}

		HLVM_LOG(LogTest, info, TXT("Created Sponza geometry: {} meshes total"), AllMeshDrawData.size());

		uint32_t GBufferWidth = Framebuffer->getFramebufferInfo().width;
		uint32_t GBufferHeight = Framebuffer->getFramebufferInfo().height;

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
		// Create SDR output texture (for tone mapping)
		// =====================================================================
		{
			nvrhi::TextureDesc Desc;
			Desc.dimension = nvrhi::TextureDimension::Texture2D;
			Desc.width = GBufferWidth;
			Desc.height = GBufferHeight;
			Desc.format = nvrhi::Format::RGBA8_UNORM;
			Desc.isRenderTarget = false;
			Desc.isUAV = true;
			Desc.isTypeless = false;
			Desc.initialState = nvrhi::ResourceStates::UnorderedAccess;
			Desc.keepInitialState = true;
			Desc.debugName = "SDROutput";
			SDRTexture = NvrhiDevice->createTexture(Desc);
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

			// Initialize with white for albedo placeholder
			nvrhi::CommandListHandle TexCmdList = NvrhiDevice->createCommandList();
			TexCmdList->open();
			uint32_t whitePixel = 0xFFFFFFFF;
			TexCmdList->writeTexture(PlaceholderTexture, 0, 0, &whitePixel, 4);

			// Create flat normal placeholder [0.5, 0.5, 1.0] = tangent-space "up"
			Desc.debugName = "NormalPlaceholderTexture";
			NormalPlaceholderTexture = NvrhiDevice->createTexture(Desc);
			uint32_t flatNormalPixel = 0xFF8080FF; // ABGR: A=255, B=128, G=128, R=255
			TexCmdList->writeTexture(NormalPlaceholderTexture, 0, 0, &flatNormalPixel, 4);

			TexCmdList->close();
			NvrhiDevice->executeCommandList(TexCmdList);
		}

		// =====================================================================
		// Initialize shadow map pass
		// =====================================================================
		HLVM_LOG(LogTest, info, TXT("Initializing shadow map pass..."));
		{
			FShadowMapPass::FDesc ShadowDesc;
			ShadowDesc.ShadowMapSize = 2048;
			if (!ShadowPass.Initialize(NvrhiDevice, ShaderDataDir, ShadowDesc))
			{
				HLVM_LOG(LogTest, err, TXT("Failed to initialize shadow map pass"));
				return false;
			}
		}
		HLVM_LOG(LogTest, info, TXT("Shadow map pass initialized successfully"));

		// =====================================================================
		// Create SSAO textures
		// =====================================================================
		HLVM_LOG(LogTest, info, TXT("Creating SSAO textures..."));
		{
			nvrhi::TextureDesc Desc;
			Desc.dimension = nvrhi::TextureDimension::Texture2D;
			Desc.width = GBufferWidth;
			Desc.height = GBufferHeight;
			Desc.format = nvrhi::Format::R8_UNORM;
			Desc.isRenderTarget = false;
			Desc.isUAV = true;
			Desc.isTypeless = false;
			Desc.initialState = nvrhi::ResourceStates::UnorderedAccess;
			Desc.keepInitialState = true;

			Desc.debugName = "SSAOTexture";
			SSAOTexture = NvrhiDevice->createTexture(Desc);

			Desc.debugName = "SSAOBlurTexture";
			SSAOBlurTexture = NvrhiDevice->createTexture(Desc);

			// Bloom textures (half-res + full-res)
			Desc.format = nvrhi::Format::R11G11B10_FLOAT;
			Desc.width = std::max(1u, GBufferWidth / 2);
			Desc.height = std::max(1u, GBufferHeight / 2);
			Desc.debugName = "BloomHalfRes";
			BloomHalfResTexture = NvrhiDevice->createTexture(Desc);
			Desc.debugName = "BloomBlurTemp";
			BloomBlurTempTexture = NvrhiDevice->createTexture(Desc);

			Desc.width = GBufferWidth;
			Desc.height = GBufferHeight;
			Desc.debugName = "BloomOutput";
			BloomTexture = NvrhiDevice->createTexture(Desc);
		}

		HLVM_LOG(LogTest, info, TXT("SSAO and Bloom textures created"));

		// =====================================================================
		// Initialize joint bilateral upsample pass (for SSAO blur)
		// =====================================================================
		HLVM_LOG(LogTest, info, TXT("Initializing bilateral blur pass..."));
		if (!BilateralBlurPass.Initialize(NvrhiDevice, CommonShaderDir))
		{
			HLVM_LOG(LogTest, err, TXT("Failed to initialize bilateral blur pass"));
			return false;
		}
		HLVM_LOG(LogTest, info, TXT("Bilateral blur pass initialized"));

		// =====================================================================
		// Initialize Bloom pass
		// =====================================================================
		HLVM_LOG(LogTest, info, TXT("Initializing bloom pass..."));
		if (!BloomPass.Initialize(NvrhiDevice, ShaderDataDir))
		{
			HLVM_LOG(LogTest, err, TXT("Failed to initialize bloom pass"));
			return false;
		}
		HLVM_LOG(LogTest, info, TXT("Bloom pass initialized"));

		// Initialize frame dumper - use RGBA8_UNORM to match SDRTexture format
		HLVM_LOG(LogTest, info, TXT("Initializing FrameDumper..."));
		FrameDumper.Initialize(NvrhiDevice, nvrhi::Format::RGBA8_UNORM);
		HLVM_LOG(LogTest, info, TXT("FrameDumper initialized."));
		FrameDumper.SetTestName(TXT("TestSponzaDeferred"));

		HLVM_LOG(LogTest, info, TXT("FSponzaDeferredPass initialized successfully"));
		return true;
	}

	void Shutdown()
	{
		HLVM_LOG(LogTest, info, TXT("FSponzaDeferredPass::Shutdown"));

		BindingCache.Clear();

		LightingPass.Shutdown();
		HDRTexture = nullptr;
		PlaceholderTexture = nullptr;
		NormalPlaceholderTexture = nullptr;
		LoadedDiffuseTexture = nullptr;

		ToneMapPass.Shutdown();
		SDRTexture = nullptr;

		ShadowPass.Shutdown();
		GBufferPass.Shutdown();

		SSAOTexture = nullptr;
		SSAOBlurTexture = nullptr;
		BloomHalfResTexture = nullptr;
		BloomBlurTempTexture = nullptr;
		BloomTexture = nullptr;
		HBAOPass.Shutdown();
		BilateralBlurPass.Shutdown();
		BloomPass.Shutdown();

		SponzaVertexBuffer = nullptr;
		SponzaIndexBuffer = nullptr;
		SponzaMesh.reset();
	}

	virtual void Animate(float fElapsedTimeSeconds) override
	{
		FrameCount++;
		FPSUpdateTimer += fElapsedTimeSeconds;
		float FPS = float(FrameCount) / FPSUpdateTimer;
		if (FPSUpdateTimer >= 1.0f)
		{
			WindowTitle = FString::Format(TXT("Sponza Deferred - FPS: {:.1f}"), FPS);

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
		// Resize handling
		// =====================================================================
		if (CurrentFBInfo.width != LastWidth || CurrentFBInfo.height != LastHeight)
		{
			HLVM_LOG(LogTest, info, TXT("Resizing - width: {}, height: {}"), CurrentFBInfo.width, CurrentFBInfo.height);
			LastWidth = CurrentFBInfo.width;
			LastHeight = CurrentFBInfo.height;

			// Resize GBuffer fill pass
			GBufferPass.Resize(CurrentFBInfo.width, CurrentFBInfo.height);

			nvrhi::TextureDesc Desc;
			Desc.dimension = nvrhi::TextureDimension::Texture2D;
			Desc.width = CurrentFBInfo.width;
			Desc.height = CurrentFBInfo.height;

			// Recreate HDR texture
			Desc.dimension = nvrhi::TextureDimension::Texture2D;
			Desc.format = nvrhi::Format::RGBA32_FLOAT;
			Desc.isRenderTarget = false;
			Desc.isUAV = true;
			Desc.isTypeless = false;
			Desc.initialState = nvrhi::ResourceStates::UnorderedAccess;
			Desc.debugName = "HDROutput";
			HDRTexture = NvrhiDevice->createTexture(Desc);

			// Recreate SDR texture
			Desc.format = nvrhi::Format::RGBA8_UNORM;
			Desc.debugName = "SDROutput";
			SDRTexture = NvrhiDevice->createTexture(Desc);

			// Recreate SSAO textures
			Desc.format = nvrhi::Format::R8_UNORM;
			Desc.debugName = "SSAOTexture";
			SSAOTexture = NvrhiDevice->createTexture(Desc);
			Desc.debugName = "SSAOBlurTexture";
			SSAOBlurTexture = NvrhiDevice->createTexture(Desc);

			// Recreate Bloom textures
			Desc.format = nvrhi::Format::R11G11B10_FLOAT;
			Desc.width = std::max(1u, CurrentFBInfo.width / 2);
			Desc.height = std::max(1u, CurrentFBInfo.height / 2);
			Desc.debugName = "BloomHalfRes";
			BloomHalfResTexture = NvrhiDevice->createTexture(Desc);
			Desc.debugName = "BloomBlurTemp";
			BloomBlurTempTexture = NvrhiDevice->createTexture(Desc);

			Desc.width = CurrentFBInfo.width;
			Desc.height = CurrentFBInfo.height;
			Desc.debugName = "BloomOutput";
			BloomTexture = NvrhiDevice->createTexture(Desc);

			BindingCache.Clear();
		}

		// =====================================================================
		// Compute Light View-Proj matrix (using scaled bounding box)
		// =====================================================================
		float	  localSceneRadius = glm::length(glm::vec3(BBoxMax.x - BBoxMin.x, BBoxMax.y - BBoxMin.y, BBoxMax.z - BBoxMin.z)) * 0.5f;
		float	  worldSceneRadius = localSceneRadius * 2.0f;
		glm::vec3 WorldSceneCenter = SceneCenter * 2.0f;
		glm::vec3 LightDirVec(0.577f, 0.577f, 0.577f);
		glm::vec3 LightPos = WorldSceneCenter + glm::normalize(LightDirVec) * worldSceneRadius * 2.0f;
		glm::mat4 LightView = glm::lookAtLH(LightPos, WorldSceneCenter, glm::vec3(0, 1, 0));
		float	  orthoSize = worldSceneRadius * 1.5f;
		glm::mat4 LightProj = glm::orthoLH_ZO(-orthoSize, orthoSize, -orthoSize, orthoSize, 0.1f, worldSceneRadius * 4.0f);
		LightViewProj = LightProj * LightView;

		// =====================================================================
		// Shadow Pass
		// =====================================================================
		nvrhi::CommandListParameters CmdListParams;
		CmdListParams.enableImmediateExecution = false;
		nvrhi::CommandListHandle CmdList = NvrhiDevice->createCommandList(CmdListParams);
		CmdList->open();

		TVector<FShadowMapPass::FMeshDrawItem> ShadowMeshItems;
		ShadowMeshItems.reserve(AllMeshDrawData.size());
		for (const auto& DrawData : AllMeshDrawData)
		{
			FShadowMapPass::FMeshDrawItem Item;
			Item.VertexBuffer = DrawData.VertexBuffer;
			Item.IndexBuffer = DrawData.IndexBuffer;
			Item.IndexCount = DrawData.IndexCount;
			ShadowMeshItems.push_back(Item);
		}

		FShadowMapPass::FRenderDesc ShadowRenderDesc;
		ShadowRenderDesc.LightViewProj = LightViewProj;
		ShadowRenderDesc.ModelMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(2.0f));
		ShadowRenderDesc.MeshDrawItems = ShadowMeshItems.data();
		ShadowRenderDesc.MeshDrawItemCount = static_cast<uint32_t>(ShadowMeshItems.size());

		ShadowPass.Render(CmdList, ShadowRenderDesc);

		// =====================================================================
		// GBuffer Pass
		// =====================================================================

		// Camera orbit around scene center
		// NOTE: Position adjusted from original (radius=20, camY=8, angle=pi) to
		// (radius=35, camY=20, angle=0) to show exterior surfaces. The original
		// interior view showed only back-faces relative to the light, making all
		// pixels shadowed and preventing shadow validation. This exterior view
		// shows both lit and shadowed surfaces for meaningful shadow testing.
		float angle = static_cast<float>(FrameCount) * 0.01f; // Start on lit side
		float radius = 35.0f;									  // Further out to see exterior surfaces
		float camX = SceneCenter.x + sinf(angle) * radius;
		float camZ = SceneCenter.z + cosf(angle) * radius;
		float camY = 20.0f; // Higher to see roof and upper walls

		// Log camera position for first few frames
		if (FrameCount < 2)
		{
			HLVM_LOG(LogTest, info, TXT("Camera: pos=({:.2f}, {:.2f}, {:.2f}), target=({:.2f}, {:.2f}, {:.2f})"),
				camX, camY, camZ, SceneCenter.x, SceneCenter.y, SceneCenter.z);
		}

		// View matrix
		glm::mat4 view = glm::lookAtLH(glm::vec3(camX, camY, camZ), SceneCenter, glm::vec3(0.f, 1.f, 0.f));
		float	  aspectRatio = float(CurrentFBInfo.width) / float(CurrentFBInfo.height);
		glm::mat4 proj = glm::perspectiveLH_ZO(glm::radians(90.0f), aspectRatio, 0.1f, 1000.0f);

		// DIAGNOSTIC: Check first mesh vertex NDC
		if (FrameCount < 2 && !AllMeshDrawData.empty())
		{
			const auto& DrawData = AllMeshDrawData[0];
			if (DrawData.Mesh && !DrawData.Mesh->GetVertices().empty())
			{
				auto&	  Vertices = DrawData.Mesh->GetVertices();
				glm::vec3 FirstPos(Vertices[0].Position.x, Vertices[0].Position.y, Vertices[0].Position.z);
				glm::mat4 ModelMat(2.f, 0.f, 0.f, 0.f, 0.f, 2.f, 0.f, 0.f, 0.f, 0.f, 2.f, 0.f, 0.f, 0.f, 0.f, 1.f);
				glm::vec4 WorldPos = ModelMat * glm::vec4(FirstPos, 1.0f);
				glm::vec4 ViewPos = view * WorldPos;
				glm::vec4 ClipPos = proj * ViewPos;
				glm::vec3 NDC = ClipPos.w != 0.0f ? glm::vec3(ClipPos) / ClipPos.w : glm::vec3(0);
				HLVM_LOG(LogTest, info, TXT("FirstVert[0]: world=({:.2f}, {:.2f}, {:.2f}), NDC=({:.4f}, {:.4f}, {:.4f})"),
					WorldPos.x, WorldPos.y, WorldPos.z, NDC.x, NDC.y, NDC.z);
			}
		}

		// Build mesh draw items with materials
		TVector<FGBufferFillPass::FMeshDrawItem> GBufferMeshItems;
		GBufferMeshItems.reserve(AllMeshDrawData.size());
		for (const auto& DrawData : AllMeshDrawData)
		{
			FGBufferFillPass::FMeshDrawItem Item;
			Item.VertexBuffer = DrawData.VertexBuffer;
			Item.IndexBuffer = DrawData.IndexBuffer;
			Item.IndexCount = DrawData.IndexCount;

			// Material lookup (same logic as inline code)
			nvrhi::TextureHandle DiffuseTex = PlaceholderTexture;
			nvrhi::TextureHandle NormalTex = NormalPlaceholderTexture;
			FGBufferFillPass::FMaterialConstants MatConst;
			memset(&MatConst, 0, sizeof(MatConst));
			MatConst.AlbedoTint[0] = 1.0f;
			MatConst.AlbedoTint[1] = 1.0f;
			MatConst.AlbedoTint[2] = 1.0f;
			MatConst.AlbedoTint[3] = 1.0f;
			MatConst.Roughness = 1.0f;

			if (DrawData.Mesh && Scene)
			{
				auto it = Scene->MeshMultiMaterialMap.find(DrawData.Mesh);
				if (it != Scene->MeshMultiMaterialMap.end() && !it->second.empty())
				{
					if (auto PBRMat = std::dynamic_pointer_cast<FPBRMaterial>(it->second[0]))
					{
						if (PBRMat->HasGPUTexture(IMaterial::ETextureType::Albedo))
							DiffuseTex = PBRMat->GetGPUTexture(IMaterial::ETextureType::Albedo).GetTextureSRV();
						if (PBRMat->HasGPUTexture(IMaterial::ETextureType::Normal))
							NormalTex = PBRMat->GetGPUTexture(IMaterial::ETextureType::Normal).GetTextureSRV();
						FVec3 albedo = PBRMat->GetAlbedoColor();
						MatConst.AlbedoTint[0] = albedo.x;
						MatConst.AlbedoTint[1] = albedo.y;
						MatConst.AlbedoTint[2] = albedo.z;
						MatConst.Metallic = PBRMat->GetMetallic();
						MatConst.Roughness = PBRMat->GetRoughness();
					}
				}
			}

			Item.Material.DiffuseTexture = DiffuseTex;
			Item.Material.NormalTexture = NormalTex;
			Item.Material.Constants = MatConst;
			GBufferMeshItems.push_back(Item);
		}

		// Fill view constants
		FGBufferFillPass::FViewConstants ViewConstants;
		memset(&ViewConstants, 0, sizeof(ViewConstants));
		memcpy(ViewConstants.ModelMatrix, glm::value_ptr(glm::scale(glm::mat4(1.0f), glm::vec3(2.0f))), 64);
		memcpy(ViewConstants.ViewMatrix, glm::value_ptr(view), 64);
		memcpy(ViewConstants.ProjMatrix, glm::value_ptr(proj), 64);
		ViewConstants.CameraPos[0] = camX;
		ViewConstants.CameraPos[1] = camY;
		ViewConstants.CameraPos[2] = camZ;
		ViewConstants.CameraPos[3] = 1.0f;

		FGBufferFillPass::FRenderDesc GBufferRenderDesc;
		GBufferRenderDesc.ViewConstants = ViewConstants;
		GBufferRenderDesc.MeshDrawItems = GBufferMeshItems.data();
		GBufferRenderDesc.MeshDrawItemCount = static_cast<uint32_t>(GBufferMeshItems.size());

		GBufferPass.Render(CmdList, GBufferRenderDesc);

		// =====================================================================
		// HBAO Pass
		// =====================================================================
		{
			SSao::FHBAOConstants HBAOConstants;
			memset(&HBAOConstants, 0, sizeof(HBAOConstants));

			// Matrices (column-major)
			glm::mat4 invProj = glm::inverse(proj);
			memcpy(HBAOConstants.ProjMatrix, glm::value_ptr(proj), 64);
			memcpy(HBAOConstants.InvProjMatrix, glm::value_ptr(invProj), 64);
			memcpy(HBAOConstants.ViewMatrix, glm::value_ptr(view), 64);

			HBAOConstants.ScreenSize[0] = float(CurrentFBInfo.width);
			HBAOConstants.ScreenSize[1] = float(CurrentFBInfo.height);
			HBAOConstants.InvScreenSize[0] = 1.0f / float(CurrentFBInfo.width);
			HBAOConstants.InvScreenSize[1] = 1.0f / float(CurrentFBInfo.height);

			// Scene-scale radius for Sponza (~20 unit scene)
			HBAOConstants.SampleRadius = 2.0f;
			HBAOConstants.AngleBias = 0.2f; // ~11 degrees
			HBAOConstants.MaxRadiusPixels = 50.0f;
			HBAOConstants.AttenuationScale = 1.0f;
			HBAOConstants.MinAO = 0.3f;
			HBAOConstants.DirectionCount = 4;
			HBAOConstants.StepCount = 6;

			SSao::FSSAOPass::FDesc HBAODesc;
			HBAODesc.DepthTexture = GBufferPass.GetDepthTexture();
			HBAODesc.NormalTexture = GBufferPass.GetNormalsTexture();
			HBAODesc.OutputTexture = SSAOTexture;
			HBAODesc.OutputWidth = CurrentFBInfo.width;
			HBAODesc.OutputHeight = CurrentFBInfo.height;

			// Transition SSAO texture to UAV before dispatch
			CmdList->setTextureState(SSAOTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);

			HBAOPass.Dispatch(CmdList, HBAODesc, HBAOConstants);
		}

		// Transition SSAO texture to SRV for blur
		CmdList->setTextureState(SSAOTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
		// Transition SSAO blur texture to UAV
		CmdList->setTextureState(SSAOBlurTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);

		// Dispatch bilateral blur pass
		FJointBilateralUpsamplePass::FDesc BilateralDesc;
		BilateralDesc.InputTexture = SSAOTexture;
		BilateralDesc.DepthTexture = GBufferPass.GetDepthTexture();
		BilateralDesc.OutputTexture = SSAOBlurTexture;
		BilateralDesc.OutputWidth = CurrentFBInfo.width;
		BilateralDesc.OutputHeight = CurrentFBInfo.height;
		BilateralDesc.DepthSigma = 0.01f;
		BilateralBlurPass.Dispatch(CmdList, BilateralDesc);

		// Transition SSAO blur texture to SRV for lighting
		CmdList->setTextureState(SSAOBlurTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);

		// =====================================================================
		// Lighting Pass
		// =====================================================================
		glm::mat4 invViewProj = glm::inverse(proj * view);

		// Transition HDR texture to UAV for compute write
		CmdList->setTextureState(HDRTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);

		FDeferredLightingPass::FDesc LightingDesc;
		LightingDesc.GBufferDiffuse = GBufferPass.GetDiffuseTexture();
		LightingDesc.GBufferMaterial = GBufferPass.GetSpecularTexture();
		LightingDesc.GBufferNormals = GBufferPass.GetNormalsTexture();
		LightingDesc.GBufferEmissive = GBufferPass.GetEmissiveTexture();
		LightingDesc.GBufferDepth = GBufferPass.GetDepthTexture();
		LightingDesc.ShadowMap = ShadowPass.GetShadowMapTexture();
		LightingDesc.SSAOTexture = SSAOBlurTexture;
		LightingDesc.HDROutputTexture = HDRTexture;
		LightingDesc.ShadowSampler = ShadowPass.GetShadowSampler();
		LightingDesc.Width = CurrentFBInfo.width;
		LightingDesc.Height = CurrentFBInfo.height;

		FDeferredLightingPass::FConstants LightingConstants;
		memset(&LightingConstants, 0, sizeof(LightingConstants));
		memcpy(LightingConstants.InvViewProj, glm::value_ptr(invViewProj), 64);
		LightingConstants.LightDir[0] = 0.577f;
		LightingConstants.LightDir[1] = 0.577f;
		LightingConstants.LightDir[2] = 0.577f;
		LightingConstants.LightIntensity = 0.8f;
		LightingConstants.CameraPos[0] = camX;
		LightingConstants.CameraPos[1] = camY;
		LightingConstants.CameraPos[2] = camZ;
		LightingConstants.ShadowHardness = 16.0f;
		LightingConstants.AmbientColor[0] = 0.03f;
		LightingConstants.AmbientColor[1] = 0.03f;
		LightingConstants.AmbientColor[2] = 0.03f;
		LightingConstants.MinAO = 0.3f;
		LightingConstants.ScreenSize[0] = float(CurrentFBInfo.width);
		LightingConstants.ScreenSize[1] = float(CurrentFBInfo.height);
		memcpy(LightingConstants.LightViewProj, glm::value_ptr(LightViewProj), 64);
		LightingConstants.ShadowMapSize = 2048.0f;
		LightingConstants.ShadowBias = 0.005f;

		LightingPass.Dispatch(CmdList, LightingDesc, LightingConstants);

		// Transition GBuffer textures back to RenderTarget for next frame
		CmdList->setTextureState(GBufferPass.GetDiffuseTexture(), nvrhi::AllSubresources, nvrhi::ResourceStates::RenderTarget);
		CmdList->setTextureState(GBufferPass.GetSpecularTexture(), nvrhi::AllSubresources, nvrhi::ResourceStates::RenderTarget);
		CmdList->setTextureState(GBufferPass.GetNormalsTexture(), nvrhi::AllSubresources, nvrhi::ResourceStates::RenderTarget);
		CmdList->setTextureState(GBufferPass.GetEmissiveTexture(), nvrhi::AllSubresources, nvrhi::ResourceStates::RenderTarget);
		CmdList->setTextureState(GBufferPass.GetDepthTexture(), nvrhi::AllSubresources, nvrhi::ResourceStates::DepthWrite);

		// =====================================================================
		// Bloom Pass
		// =====================================================================
		{
			FBloomPass::FDesc BloomDesc;
			BloomDesc.HDRTexture = HDRTexture;
			BloomDesc.OutputTexture = BloomTexture;
			BloomDesc.HalfResTexture = BloomHalfResTexture;
			BloomDesc.BlurTempTexture = BloomBlurTempTexture;
			BloomDesc.LinearSampler = GBufferPass.GetLinearSampler();
			BloomDesc.FullResWidth = CurrentFBInfo.width;
			BloomDesc.FullResHeight = CurrentFBInfo.height;
			BloomDesc.Threshold = 0.8f;
			BloomDesc.Intensity = 0.4f;
			BloomDesc.Sigma = 2.0f;
			BloomDesc.BlurIterations = 2;

			CmdList->setTextureState(HDRTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
			CmdList->setTextureState(BloomTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);

			BloomPass.Dispatch(CmdList, BloomDesc);

			CmdList->setTextureState(BloomTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
		}

		// =====================================================================
		// Tone Mapping Pass
		// =====================================================================
		CmdList->setTextureState(HDRTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
		CmdList->setTextureState(BloomTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
		CmdList->setTextureState(SDRTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);

		FToneMappingPass::FDesc ToneMapDesc;
		ToneMapDesc.HDRInputTexture = HDRTexture;
		ToneMapDesc.BloomTexture = BloomTexture;
		ToneMapDesc.SDROutputTexture = SDRTexture;
		ToneMapDesc.Width = CurrentFBInfo.width;
		ToneMapDesc.Height = CurrentFBInfo.height;

		FToneMappingPass::FConstants ToneMapConstants;
		memset(&ToneMapConstants, 0, sizeof(ToneMapConstants));
		ToneMapConstants.Exposure = 1.0f;
		ToneMapConstants.Gamma = 2.2f;
		ToneMapConstants.TonemapMode = 0; // ACES
		ToneMapConstants.BloomIntensity = 0.4f;
		ToneMapConstants.TextureSize[0] = float(CurrentFBInfo.width);
		ToneMapConstants.TextureSize[1] = float(CurrentFBInfo.height);

		ToneMapPass.Dispatch(CmdList, ToneMapDesc, ToneMapConstants);

		// =====================================================================
		// Frame dump Phase 1: Prepare copy on command list (BEFORE close)
		// =====================================================================
		if (FrameDumper.IsEnabled())
		{
			CmdList->setTextureState(SDRTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::CopySource);
			HLVM_LOG(LogTest, info, TXT("Frame dump: SDRTexture={}, width={}, height={}"),
				static_cast<void*>(SDRTexture.Get()), CurrentFBInfo.width, CurrentFBInfo.height);
			FrameDumper.BeginDump(NvrhiDevice, SDRTexture.Get(), CurrentFBInfo.width, CurrentFBInfo.height);
			FrameDumper.PrepareCopy(CmdList);
		}

		// =====================================================================
		// Blit SDR to screen
		// =====================================================================
		CmdList->setTextureState(SDRTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);

		FCommonRenderPasses::BlitParameters BlitParams;
		FCommonRenderPasses::BlitTexture(
			CmdList,
			Framebuffer,
			SDRTexture,
			&BindingCache,
			CurrentFBInfo.width,
			CurrentFBInfo.height,
			BlitParams);

		CmdList->close();
		NvrhiDevice->executeCommandList(CmdList);

		// Wait for GPU to finish before next frame and before readback
		NvrhiDevice->waitForIdle();

		// =====================================================================
		// Frame dump Phase 2: Readback and save (AFTER execute)
		// =====================================================================
		if (FrameDumper.IsEnabled())
		{
			int frameNum = FrameDumper.GetCurrentFrame();
			if (FrameDumper.ReadbackAndSave())
			{
				HLVM_LOG(LogTest, info, TXT("Dumped frame {}"), frameNum);
				// Compare against reference image if available
				FString refPath = FString::Format(
					TXT("{}/../../Test/TestSponzaDeferred_Data/Reference/frame_{:04d}.png"),
					*GExecutablePath, frameNum);
				FrameDumper.CompareAgainstReference(refPath, 0.01f);
			}
			if (FrameDumper.IsLastFrame())
			{
				return; // Skip presentation on last frame
			}
		}
	}

	virtual void BackBufferResizing() override
	{
		HDRTexture = nullptr;
		SDRTexture = nullptr;
		LoadedDiffuseTexture = nullptr;
		SSAOTexture = nullptr;
		SSAOBlurTexture = nullptr;
		BindingCache.Clear();
	}

private:
	nvrhi::IDevice*		   NvrhiDevice = nullptr;
	nvrhi::FramebufferInfoEx FBInfo;
	FString				   WindowTitle;

	std::shared_ptr<FScene3DNode> Scene;

	nvrhi::TextureHandle	 PlaceholderTexture;
	nvrhi::TextureHandle	 NormalPlaceholderTexture;
	nvrhi::TextureHandle	 LoadedDiffuseTexture;

	FGBufferFillPass			 GBufferPass;
	nvrhi::TextureHandle		 HDRTexture;
	FDeferredLightingPass		 LightingPass;

	nvrhi::TextureHandle		 SDRTexture;
	FToneMappingPass			 ToneMapPass;

	FShadowMapPass				 ShadowPass;
	glm::mat4					 LightViewProj = glm::mat4(1.0f);

	// SSAO resources
	nvrhi::TextureHandle		SSAOTexture;
	nvrhi::TextureHandle		SSAOBlurTexture;
	SSao::FSSAOPass				HBAOPass;
	FJointBilateralUpsamplePass BilateralBlurPass;

	// Bloom resources
	nvrhi::TextureHandle		BloomHalfResTexture;
	nvrhi::TextureHandle		BloomBlurTempTexture;
	nvrhi::TextureHandle		BloomTexture;
	FBloomPass					BloomPass;

	FRenderPassDumper FrameDumper;

	// Sponza geometry buffers (cached)
	nvrhi::BufferHandle			 SponzaVertexBuffer;
	nvrhi::BufferHandle			 SponzaIndexBuffer;
	std::shared_ptr<FStaticMesh> SponzaMesh;
	uint32_t					 SponzaIndexCount = 0;

	// Per-mesh data for multi-mesh rendering
	struct FMeshDrawData
	{
		nvrhi::BufferHandle			 VertexBuffer;
		nvrhi::BufferHandle			 IndexBuffer;
		std::shared_ptr<FStaticMesh> Mesh;
		uint32_t					 IndexCount;
	};
	TVector<FMeshDrawData> AllMeshDrawData;

	FBindingCache BindingCache;

	glm::vec3 SceneCenter = glm::vec3(0.f); // Calculated from scene bounding box
	glm::vec3 BBoxMin = glm::vec3(0.f);
	glm::vec3 BBoxMax = glm::vec3(0.f);

	uint32_t LastWidth = 0;
	uint32_t LastHeight = 0;
	uint32_t FrameCount = 0;
	float	 FPSUpdateTimer = 0.0f;
};

// =============================================================================
// TEST IMPLEMENTATION
// =============================================================================

RECORD_BOOL(test_SponzaDeferred)
{
	HLVM_LOG(LogTest, info, TXT("=== Starting Sponza Deferred Test ==="));

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
		DeviceParams.bEnableDebugRuntime = true;
		DeviceParams.bEnableNVRHIValidationLayer = true;
		DeviceParams.bEnableRayTracingExtensions = false;

		if (!DeviceManager->CreateWindowDeviceAndSwapChain(WindowProps))
		{
			throw std::runtime_error("Failed to create window, device and swap chain");
		}

		nvrhi::IDevice*		 NvrhiDevice = DeviceManager->GetDevice();
		nvrhi::IFramebuffer* FirstFB = DeviceManager->GetFramebuffer(0);

		HLVM_LOG(LogTest, info, TXT("Creating render pass..."));
		TSharedPtr<FSponzaDeferredPass> DeferredPass =
			std::make_shared<FSponzaDeferredPass>(DeviceManager.get());
		if (!DeferredPass->Initialize(NvrhiDevice, FirstFB, FString(TXT("Sponza Deferred"))))
		{
			throw std::runtime_error("Failed to initialize FSponzaDeferredPass");
		}

		DeviceManager->AddRenderPassToBack(DeferredPass);

		HLVM_LOG(LogTest, info, TXT("Starting render loop..."));

		std::thread([&]() {
			FTimer Timer;
			while (Timer.MarkSec() < 5.0)
			{
			}
			DeviceManager->StopMessageLoop();
		}).detach();

		DeviceManager->RunMessageLoop();

		DeferredPass->Shutdown();

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

RECORD_BOOL(test_SponzaDeferred)
{
	HLVM_LOG(LogTest, warning, TXT("Vulkan renderer not enabled - skipping test"));
	return true;
}

#endif // HLVM_VULKAN_RENDERER
