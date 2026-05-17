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
		// Load GBuffer shaders
		// =====================================================================
		HLVM_LOG(LogTest, info, TXT("Loading GBuffer shaders..."));

		auto GBufferVSBlob = ReadBinaryFile(
			FPath::Combine(ShaderDataDir, TXT("GBufferSponzaVS.sblob")).string());
		const void* GBufferVSBinary = nullptr;
		size_t		GBufferVSBinarySize = 0;
		if (!ShaderMake::FindPermutationInBlob(GBufferVSBlob.data(), GBufferVSBlob.size(), nullptr, 0, &GBufferVSBinary, &GBufferVSBinarySize))
		{
			HLVM_LOG(LogTest, err, TXT("Failed to extract GBufferVS from blob"));
			return false;
		}

		auto GBufferPSBlob = ReadBinaryFile(
			FPath::Combine(ShaderDataDir, TXT("GBufferSponzaPS.sblob")).string());
		const void* GBufferPSBinary = nullptr;
		size_t		GBufferPSBinarySize = 0;
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
		// Load lighting compute shader
		// =====================================================================
		HLVM_LOG(LogTest, info, TXT("Loading lighting shader..."));

		auto LightingCSBlob = ReadBinaryFile(
			FPath::Combine(ShaderDataDir, TXT("SponzaDeferredLighting_cs.sblob")).string());
		const void* LightingCSBinary = nullptr;
		size_t		LightingCSBinarySize = 0;
		if (!ShaderMake::FindPermutationInBlob(LightingCSBlob.data(), LightingCSBlob.size(), nullptr, 0, &LightingCSBinary, &LightingCSBinarySize))
		{
			HLVM_LOG(LogTest, err, TXT("Failed to extract LightingCS from blob"));
			return false;
		}

		nvrhi::ShaderDesc CSDesc;
		CSDesc.setShaderType(nvrhi::ShaderType::Compute);
		LightingCS = NvrhiDevice->createShader(CSDesc, LightingCSBinary, LightingCSBinarySize);

		if (!LightingCS)
		{
			HLVM_LOG(LogTest, err, TXT("Failed to create lighting shader"));
			return false;
		}
		HLVM_LOG(LogTest, info, TXT("Lighting shader loaded successfully"));

		// =====================================================================
		// Load tone mapping compute shader
		// =====================================================================
		HLVM_LOG(LogTest, info, TXT("Loading tone mapping shader..."));

		auto ToneMapCSBlob = ReadBinaryFile(
			FPath::Combine(ShaderDataDir, TXT("TonemapSponza_cs.sblob")).string());
		const void* ToneMapCSBinary = nullptr;
		size_t		ToneMapCSBinarySize = 0;
		if (!ShaderMake::FindPermutationInBlob(ToneMapCSBlob.data(), ToneMapCSBlob.size(), nullptr, 0, &ToneMapCSBinary, &ToneMapCSBinarySize))
		{
			HLVM_LOG(LogTest, err, TXT("Failed to extract ToneMapCS from blob"));
			return false;
		}

		ToneMapCS = NvrhiDevice->createShader(CSDesc, ToneMapCSBinary, ToneMapCSBinarySize);
		if (!ToneMapCS)
		{
			HLVM_LOG(LogTest, err, TXT("Failed to create tone mapping shader"));
			return false;
		}
		HLVM_LOG(LogTest, info, TXT("Tone mapping shader loaded successfully"));

		// =====================================================================
		// Load shadow vertex shader
		// =====================================================================
		HLVM_LOG(LogTest, info, TXT("Loading shadow shader..."));

		auto ShadowVSBlob = ReadBinaryFile(
			FPath::Combine(ShaderDataDir, TXT("ShadowVS.sblob")).string());
		const void* ShadowVSBinary = nullptr;
		size_t		ShadowVSBinarySize = 0;
		if (!ShaderMake::FindPermutationInBlob(ShadowVSBlob.data(), ShadowVSBlob.size(), nullptr, 0, &ShadowVSBinary, &ShadowVSBinarySize))
		{
			HLVM_LOG(LogTest, err, TXT("Failed to extract ShadowVS from blob"));
			return false;
		}

		nvrhi::ShaderDesc ShadowVSDesc;
		ShadowVSDesc.setShaderType(nvrhi::ShaderType::Vertex);
		ShadowVS = NvrhiDevice->createShader(ShadowVSDesc, ShadowVSBinary, ShadowVSBinarySize);

		if (!ShadowVS)
		{
			HLVM_LOG(LogTest, err, TXT("Failed to create shadow shader"));
			return false;
		}
		HLVM_LOG(LogTest, info, TXT("Shadow shader loaded successfully"));

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
		const FPath	  ScenePath = FPath(FString::Format(
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
					if (!LoadedDiffuseTexture) {
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
		// Create GBuffer textures and framebuffer
		// =====================================================================
		HLVM_LOG(LogTest, info, TXT("Creating GBuffer textures..."));

		uint32_t GBufferWidth = Framebuffer->getFramebufferInfo().width;
		uint32_t GBufferHeight = Framebuffer->getFramebufferInfo().height;

		// MRT1: Diffuse (RGBA8)
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

		// MRT2: Specular (RGBA16F)
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

		// MRT3: Normals (RGBA16F)
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

		// MRT4: Emissive (RGBA16F)
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
		HLVM_LOG(LogTest, info, TXT("GBuffer framebuffer created"));

		// =====================================================================
		// Create GBuffer input layout (matches FVertex format)
		// =====================================================================
		{
			nvrhi::VertexAttributeDesc Attrs[4];
			Attrs[0].setName("POSITION").setFormat(nvrhi::Format::RGB32_FLOAT).setOffset(offsetof(FVertex, Position)).setElementStride(sizeof(FVertex));
			Attrs[1].setName("NORMAL").setFormat(nvrhi::Format::RGB32_FLOAT).setOffset(offsetof(FVertex, Normal)).setElementStride(sizeof(FVertex));
			Attrs[2].setName("TEXCOORD0").setFormat(nvrhi::Format::RG32_FLOAT).setOffset(offsetof(FVertex, UV)).setElementStride(sizeof(FVertex));
			Attrs[3].setName("TANGENT").setFormat(nvrhi::Format::RGB32_FLOAT).setOffset(offsetof(FVertex, Tangent)).setElementStride(sizeof(FVertex));

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

			// b0 -> 256 (ViewConstants)
			// b1 -> 257 (MaterialConstants)
			// t0 -> 0 (DiffuseTexture)
			// t1 -> 1 (NormalTexture)
			// s0 -> 128 (LinearSampler)
			LayoutDesc.bindings = {
				nvrhi::BindingLayoutItem::ConstantBuffer(256), // ViewConstants b0 -> 256
				nvrhi::BindingLayoutItem::ConstantBuffer(257), // MaterialConstants b1 -> 257
				nvrhi::BindingLayoutItem::Texture_SRV(0),	   // DiffuseTexture t0
				nvrhi::BindingLayoutItem::Texture_SRV(1),	   // NormalTexture t1
				nvrhi::BindingLayoutItem::Sampler(128)		   // LinearSampler s0 -> 128
			};

			GBufferBindingLayout = NvrhiDevice->createBindingLayout(LayoutDesc);
		}

		// =====================================================================
		// Create GBuffer constant buffer
		// =====================================================================
		{
			nvrhi::BufferDesc CBDesc;
			CBDesc.byteSize = sizeof(float) * 16 * 4 + sizeof(float) * 4; // Model(4) + View(4) + Proj(4) + CameraPos(1)
			CBDesc.isConstantBuffer = true;
			CBDesc.isVolatile = false;
			CBDesc.keepInitialState = true;
			CBDesc.initialState = nvrhi::ResourceStates::ConstantBuffer;
			CBDesc.debugName = "ViewConstants";
			ViewConstantsBuffer = NvrhiDevice->createBuffer(CBDesc);
		}

		// Create Material constant buffer
		// =====================================================================
		{
			nvrhi::BufferDesc MatCBDesc;
			MatCBDesc.byteSize = sizeof(float) * 8; // AlbedoColor(4) + Specular/Roughness/Pad(3)
			MatCBDesc.isConstantBuffer = true;
			MatCBDesc.isVolatile = false;
			MatCBDesc.keepInitialState = true;
			MatCBDesc.initialState = nvrhi::ResourceStates::ConstantBuffer;
			MatCBDesc.debugName = "MaterialConstants";
			MaterialConstantBuffer = NvrhiDevice->createBuffer(MatCBDesc);
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
		// Create Lighting binding layout
		// =====================================================================
		{
			nvrhi::BindingLayoutDesc LayoutDesc;
			LayoutDesc.visibility = nvrhi::ShaderType::Compute;

			nvrhi::VulkanBindingOffsets offsets;
			offsets.setConstantBufferOffset(0).setShaderResourceOffset(0).setSamplerOffset(0).setUnorderedAccessViewOffset(0);
			LayoutDesc.setBindingOffsets(offsets);

			// b0 -> 256 (LightingConstants)
			// t0 -> 0 (GBufferDiffuse - Diffuse RGB)
			// t1 -> 1 (GBufferMaterial - Metallic(R) + Roughness(G))
			// t2 -> 2 (GBufferNormals - Normal XYZ)
			// t3 -> 3 (GBufferEmissive - Emissive RGB)
			// t4 -> 4 (GBufferDepth - Direct depth SRV for world pos reconstruction)
			// t5 -> 5 (ShadowMap)
			// t6 -> 6 (SSAO blur texture)
			// u0 -> 384 (HDR output)
			LayoutDesc.bindings = {
				nvrhi::BindingLayoutItem::ConstantBuffer(256),
				nvrhi::BindingLayoutItem::Texture_SRV(0),  // t0: Diffuse RGB
				nvrhi::BindingLayoutItem::Texture_SRV(1),  // t1: Metallic/Roughness
				nvrhi::BindingLayoutItem::Texture_SRV(2),  // t2: Normals
				nvrhi::BindingLayoutItem::Texture_SRV(3),  // t3: Emissive
				nvrhi::BindingLayoutItem::Texture_SRV(4),  // t4: Depth copy
				nvrhi::BindingLayoutItem::Texture_SRV(5),  // t5: ShadowMap
				nvrhi::BindingLayoutItem::Texture_SRV(6),  // t6: SSAO blur
				nvrhi::BindingLayoutItem::Sampler(129),    // s1: ShadowSampler
				nvrhi::BindingLayoutItem::Texture_UAV(384) // u0: HDR output
			};

			LightingBindingLayout = NvrhiDevice->createBindingLayout(LayoutDesc);
		}

		// =====================================================================
		// Create Lighting pipeline
		// =====================================================================
		{
			nvrhi::ComputePipelineDesc PipelineDesc;
			PipelineDesc.setComputeShader(LightingCS);
			PipelineDesc.addBindingLayout(LightingBindingLayout);

			LightingPipeline = NvrhiDevice->createComputePipeline(PipelineDesc);
			if (!LightingPipeline)
			{
				HLVM_LOG(LogTest, err, TXT("Failed to create lighting pipeline"));
				return false;
			}
		}
		HLVM_LOG(LogTest, info, TXT("Lighting pipeline created"));

		// =====================================================================
		// Create Tone Map binding layout
		// =====================================================================
		{
			nvrhi::BindingLayoutDesc LayoutDesc;
			LayoutDesc.visibility = nvrhi::ShaderType::Compute;

			nvrhi::VulkanBindingOffsets offsets;
			offsets.setConstantBufferOffset(0).setShaderResourceOffset(0).setSamplerOffset(0).setUnorderedAccessViewOffset(0);
			LayoutDesc.setBindingOffsets(offsets);

			// b0 -> 256 (TonemapConstants)
			// t0 -> 0 (HDR input)
			// u0 -> 384 (SDR output)
			LayoutDesc.bindings = {
				nvrhi::BindingLayoutItem::ConstantBuffer(256),
				nvrhi::BindingLayoutItem::Texture_SRV(0),  // t0: HDR input
				nvrhi::BindingLayoutItem::Texture_UAV(384) // u0: SDR output
			};

			ToneMapBindingLayout = NvrhiDevice->createBindingLayout(LayoutDesc);
		}

		// =====================================================================
		// Create Tone Map pipeline
		// =====================================================================
		{
			nvrhi::ComputePipelineDesc PipelineDesc;
			PipelineDesc.setComputeShader(ToneMapCS);
			PipelineDesc.addBindingLayout(ToneMapBindingLayout);

			ToneMapPipeline = NvrhiDevice->createComputePipeline(PipelineDesc);
			if (!ToneMapPipeline)
			{
				HLVM_LOG(LogTest, err, TXT("Failed to create tone mapping pipeline"));
				return false;
			}
		}
		HLVM_LOG(LogTest, info, TXT("Tone mapping pipeline created"));

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
		// Create lighting constant buffer
		// =====================================================================
		{
			nvrhi::BufferDesc BufferDesc;
			BufferDesc.byteSize = sizeof(float) * 52; // 13 float4s = 208 bytes
			BufferDesc.isConstantBuffer = true;
			BufferDesc.isVolatile = false;
			BufferDesc.keepInitialState = true;
			BufferDesc.initialState = nvrhi::ResourceStates::ConstantBuffer;
			BufferDesc.debugName = "LightingConstants";
			LightingConstantBuffer = NvrhiDevice->createBuffer(BufferDesc);
		}

		// =====================================================================
		// Create tone mapping constant buffer
		// =====================================================================
		{
			nvrhi::BufferDesc BufferDesc;
			BufferDesc.byteSize = sizeof(float) * 8; // 2 float4s = 32 bytes
			BufferDesc.isConstantBuffer = true;
			BufferDesc.isVolatile = false;
			BufferDesc.keepInitialState = true;
			BufferDesc.initialState = nvrhi::ResourceStates::ConstantBuffer;
			BufferDesc.debugName = "TonemapConstants";
			ToneMapConstantBuffer = NvrhiDevice->createBuffer(BufferDesc);
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
		// Create shadow map texture and framebuffer
		// =====================================================================
		HLVM_LOG(LogTest, info, TXT("Creating shadow map resources..."));

		// Shadow map (2048x2048, D32)
		{
			nvrhi::TextureDesc Desc;
			Desc.dimension = nvrhi::TextureDimension::Texture2D;
			Desc.width = 2048;
			Desc.height = 2048;
			Desc.format = nvrhi::Format::D32;
			Desc.isRenderTarget = true;
			Desc.isUAV = false;
			Desc.isTypeless = true;
			Desc.initialState = nvrhi::ResourceStates::DepthWrite;
			Desc.keepInitialState = true;
			Desc.debugName = "ShadowMap";
			ShadowMapTexture = NvrhiDevice->createTexture(Desc);
		}

		// Shadow map framebuffer (depth-only)
		{
			nvrhi::FramebufferDesc FBDesc;
			nvrhi::FramebufferAttachment DepthAttach;
			DepthAttach.setTexture(ShadowMapTexture);
			FBDesc.setDepthAttachment(DepthAttach);
			ShadowMapFramebuffer = NvrhiDevice->createFramebuffer(FBDesc);
		}

		// Shadow constants buffer (128 bytes: ModelMatrix + LightViewProj)
		{
			nvrhi::BufferDesc CBDesc;
			CBDesc.byteSize = sizeof(float) * 16 * 2; // 2 float4x4 matrices
			CBDesc.isConstantBuffer = true;
			CBDesc.isVolatile = false;
			CBDesc.keepInitialState = true;
			CBDesc.initialState = nvrhi::ResourceStates::ConstantBuffer;
			CBDesc.debugName = "ShadowConstants";
			ShadowConstantsBuffer = NvrhiDevice->createBuffer(CBDesc);
		}

		// Shadow sampler (clamp border white)
		{
			nvrhi::SamplerDesc SamplerDesc;
			SamplerDesc.setAddressU(nvrhi::SamplerAddressMode::ClampToBorder)
				.setAddressV(nvrhi::SamplerAddressMode::ClampToBorder)
				.setAddressW(nvrhi::SamplerAddressMode::ClampToBorder)
				.setBorderColor(nvrhi::Color(1.0f, 1.0f, 1.0f, 1.0f))
				.setMinFilter(true)
				.setMagFilter(true)
				.setMipFilter(false);
			ShadowSampler = NvrhiDevice->createSampler(SamplerDesc);
		}

		// Shadow binding layout
		{
			nvrhi::BindingLayoutDesc LayoutDesc;
			LayoutDesc.visibility = nvrhi::ShaderType::Vertex;

			nvrhi::VulkanBindingOffsets offsets;
			offsets.setConstantBufferOffset(0).setShaderResourceOffset(0).setSamplerOffset(0).setUnorderedAccessViewOffset(0);
			LayoutDesc.setBindingOffsets(offsets);

			LayoutDesc.bindings = {
				nvrhi::BindingLayoutItem::ConstantBuffer(256) // ShadowConstants b0 -> 256
			};

			ShadowBindingLayout = NvrhiDevice->createBindingLayout(LayoutDesc);
		}

		// Shadow pipeline (vertex-only, cull back, depth less)
		{
			nvrhi::GraphicsPipelineDesc PipelineDesc;
			PipelineDesc.setVertexShader(ShadowVS);
			PipelineDesc.setInputLayout(GBufferInputLayout);
			PipelineDesc.addBindingLayout(ShadowBindingLayout);
			PipelineDesc.setPrimType(nvrhi::PrimitiveType::TriangleList);
			PipelineDesc.renderState.setRasterState(nvrhi::RasterState().setCullBack());
			PipelineDesc.renderState.depthStencilState
				.setDepthTestEnable(true)
				.setDepthWriteEnable(true)
				.setDepthFunc(nvrhi::ComparisonFunc::Less);

			ShadowPipeline = NvrhiDevice->createGraphicsPipeline(PipelineDesc, ShadowMapFramebuffer->getFramebufferInfo());
			if (!ShadowPipeline)
			{
				HLVM_LOG(LogTest, err, TXT("Failed to create shadow pipeline"));
				return false;
			}
		}
		HLVM_LOG(LogTest, info, TXT("Shadow map resources created"));

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
		}

		HLVM_LOG(LogTest, info, TXT("SSAO textures created"));

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

		LightingPipeline = nullptr;
		LightingBindingLayout = nullptr;
		LightingCS = nullptr;
		HDRTexture = nullptr;
		LightingConstantBuffer = nullptr;
		PlaceholderTexture = nullptr;
		NormalPlaceholderTexture = nullptr;
		LoadedDiffuseTexture = nullptr;

		ToneMapPipeline = nullptr;
		ToneMapBindingLayout = nullptr;
		ToneMapCS = nullptr;
		SDRTexture = nullptr;
		ToneMapConstantBuffer = nullptr;

		ShadowVS = nullptr;
		ShadowPipeline = nullptr;
		ShadowMapTexture = nullptr;
		ShadowMapFramebuffer = nullptr;
		ShadowConstantsBuffer = nullptr;
		ShadowSampler = nullptr;
		ShadowBindingLayout = nullptr;

		SSAOTexture = nullptr;
		SSAOBlurTexture = nullptr;
		HBAOPass.Shutdown();
		BilateralBlurPass.Shutdown();

		GBufferVS = nullptr;
		GBufferPS = nullptr;
		GBufferDiffuseTexture = nullptr;
		GBufferSpecularTexture = nullptr;
		GBufferNormalsTexture = nullptr;
		GBufferEmissiveTexture = nullptr;
		GBufferDepthTexture = nullptr;
		GBufferFramebuffer = nullptr;
		GBufferInputLayout = nullptr;
		GBufferBindingLayout = nullptr;
		ViewConstantsBuffer = nullptr;
		MaterialConstantBuffer = nullptr;
		GBufferPipeline = nullptr;
		GBufferLinearSampler = nullptr;
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
		if (!GBufferNormalsTexture || CurrentFBInfo.width != LastWidth || CurrentFBInfo.height != LastHeight)
		{
			HLVM_LOG(LogTest, info, TXT("Resizing - width: {}, height: {}"), CurrentFBInfo.width, CurrentFBInfo.height);
			LastWidth = CurrentFBInfo.width;
			LastHeight = CurrentFBInfo.height;

			// Recreate GBuffer textures
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

			Desc.format = nvrhi::Format::RGBA16_FLOAT;
			Desc.debugName = "GBufferDiffuse";
			GBufferDiffuseTexture = NvrhiDevice->createTexture(Desc);

			Desc.debugName = "GBufferSpecular";
			GBufferSpecularTexture = NvrhiDevice->createTexture(Desc);

			Desc.debugName = "GBufferNormals";
			GBufferNormalsTexture = NvrhiDevice->createTexture(Desc);

			Desc.debugName = "GBufferEmissive";
			GBufferEmissiveTexture = NvrhiDevice->createTexture(Desc);

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

			BindingCache.Clear();
		}

		// =====================================================================
		// Compute Light View-Proj matrix (using scaled bounding box)
		// =====================================================================
		float localSceneRadius = glm::length(glm::vec3(BBoxMax.x - BBoxMin.x, BBoxMax.y - BBoxMin.y, BBoxMax.z - BBoxMin.z)) * 0.5f;
		float worldSceneRadius = localSceneRadius * 2.0f;
		glm::vec3 WorldSceneCenter = SceneCenter * 2.0f;
		glm::vec3 LightDirVec(0.577f, 0.577f, 0.577f);
		glm::vec3 LightPos = WorldSceneCenter + glm::normalize(LightDirVec) * worldSceneRadius * 2.0f;
		glm::mat4 LightView = glm::lookAtLH(LightPos, WorldSceneCenter, glm::vec3(0, 1, 0));
		float orthoSize = worldSceneRadius * 1.5f;
		glm::mat4 LightProj = glm::orthoLH_ZO(-orthoSize, orthoSize, -orthoSize, orthoSize, 0.1f, worldSceneRadius * 4.0f);
		LightViewProj = LightProj * LightView;

		// =====================================================================
		// Shadow Pass
		// =====================================================================
		nvrhi::CommandListParameters CmdListParams;
		CmdListParams.enableImmediateExecution = false;
		nvrhi::CommandListHandle CmdList = NvrhiDevice->createCommandList(CmdListParams);
		CmdList->open();

		// Clear shadow map
		CmdList->clearDepthStencilTexture(ShadowMapTexture, nvrhi::AllSubresources, true, 1.0f, false, 0);

		// Draw all meshes to shadow map
		for (size_t MeshIdx = 0; MeshIdx < AllMeshDrawData.size(); ++MeshIdx)
		{
			const auto& DrawData = AllMeshDrawData[MeshIdx];

			// Upload shadow constants: ModelMatrix (2x scale) + LightViewProj
			float ShadowConstantsData[16 * 2];
			memcpy(&ShadowConstantsData[0], glm::value_ptr(glm::scale(glm::mat4(1.0f), glm::vec3(2.0f))), 64);
			memcpy(&ShadowConstantsData[16], glm::value_ptr(LightViewProj), 64);
			CmdList->writeBuffer(ShadowConstantsBuffer, ShadowConstantsData, sizeof(ShadowConstantsData));

			nvrhi::BindingSetDesc ShadowBindingSetDesc;
			ShadowBindingSetDesc.bindings = {
				nvrhi::BindingSetItem::ConstantBuffer(256, ShadowConstantsBuffer)
			};
			nvrhi::BindingSetHandle ShadowBindingSet = BindingCache.GetOrCreateBindingSet(ShadowBindingSetDesc, ShadowBindingLayout);

			nvrhi::GraphicsState ShadowState;
			ShadowState.setPipeline(ShadowPipeline);
			ShadowState.setFramebuffer(ShadowMapFramebuffer);
			ShadowState.addBindingSet(ShadowBindingSet);

			nvrhi::VertexBufferBinding VBBinding;
			VBBinding.setBuffer(DrawData.VertexBuffer);
			VBBinding.setSlot(0);
			VBBinding.setOffset(0);
			ShadowState.addVertexBuffer(VBBinding);

			nvrhi::IndexBufferBinding IBBinding;
			IBBinding.setBuffer(DrawData.IndexBuffer);
			IBBinding.setOffset(0);
			IBBinding.setFormat(nvrhi::Format::R32_UINT);
			ShadowState.setIndexBuffer(IBBinding);

			nvrhi::Viewport shadowViewport(0.f, 2048.f, 0.f, 2048.f, 0.0f, 1.0f);
			ShadowState.viewport.addViewportAndScissorRect(shadowViewport);

			CmdList->setGraphicsState(ShadowState);

			nvrhi::DrawArguments DrawArgs;
			DrawArgs.vertexCount = DrawData.IndexCount;
			CmdList->drawIndexed(DrawArgs);
		}

		// Transition shadow map to ShaderResource for lighting pass
		CmdList->setTextureState(ShadowMapTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);

		// =====================================================================
		// GBuffer Pass
		// =====================================================================

		// Camera orbit around scene center
		// NOTE: Position adjusted from original (radius=20, camY=8, angle=pi) to
		// (radius=35, camY=20, angle=0) to show exterior surfaces. The original
		// interior view showed only back-faces relative to the light, making all
		// pixels shadowed and preventing shadow validation. This exterior view
		// shows both lit and shadowed surfaces for meaningful shadow testing.
		float angle = static_cast<float>(FrameCount) * 0.01f;  // Start on lit side
		float radius = 35.0f;  // Further out to see exterior surfaces
		float camX = SceneCenter.x + sinf(angle) * radius;
		float camZ = SceneCenter.z + cosf(angle) * radius;
		float camY = 20.0f;  // Higher to see roof and upper walls

		// Log camera position for first few frames
		if (FrameCount < 2)
		{
			HLVM_LOG(LogTest, info, TXT("Camera: pos=({:.2f}, {:.2f}, {:.2f}), target=({:.2f}, {:.2f}, {:.2f})"),
				camX, camY, camZ, SceneCenter.x, SceneCenter.y, SceneCenter.z);
		}

		// View matrix
		glm::mat4 view = glm::lookAtLH(glm::vec3(camX, camY, camZ), SceneCenter, glm::vec3(0.f, 1.f, 0.f));
		float aspectRatio = float(CurrentFBInfo.width) / float(CurrentFBInfo.height);
		glm::mat4 proj = glm::perspectiveLH_ZO(glm::radians(90.0f), aspectRatio, 0.1f, 1000.0f);

		// DIAGNOSTIC: Check first mesh vertex NDC
		if (FrameCount < 2 && !AllMeshDrawData.empty())
		{
			const auto& DrawData = AllMeshDrawData[0];
			if (DrawData.Mesh && !DrawData.Mesh->GetVertices().empty())
			{
				auto& Vertices = DrawData.Mesh->GetVertices();
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

		// Write ViewConstants: Model(2x scale) + View + Proj + CameraPos
		// NOTE: Slangc ignores #pragma pack_matrix, so HLSL uses default column-major.
		// We must upload matrices in column-major order (matching GLM storage).
		float ViewConstantsData[16 * 4];
		// ModelMatrix (2x scale, symmetric so row/col order doesn't matter)
		memcpy(&ViewConstantsData[0], glm::value_ptr(glm::scale(glm::mat4(1.0f), glm::vec3(2.0f))), 64);
		// ViewMatrix (column-major from GLM)
		memcpy(&ViewConstantsData[16], glm::value_ptr(view), 64);
		// ProjMatrix (column-major from GLM)
		memcpy(&ViewConstantsData[32], glm::value_ptr(proj), 64);
		// CameraPos
		ViewConstantsData[48] = camX;
		ViewConstantsData[49] = camY;
		ViewConstantsData[50] = camZ;
		ViewConstantsData[51] = 1.0f;
		CmdList->writeBuffer(ViewConstantsBuffer, ViewConstantsData, sizeof(ViewConstantsData));

		// Use cached Sponza buffers
		if (AllMeshDrawData.empty())
		{
			HLVM_LOG(LogTest, warn, TXT("Render: No meshes available"));
			CmdList->close();
			return;
		}

		// Clear MRTs
		nvrhi::Color clearBlack(0.f, 0.f, 0.f, 0.f);
		nvrhi::Color clearBlue(0.f, 0.f, 1.f, 1.f);
		// Normal clear: (0.5, 1.0, 0.5) decodes to (0, 1, 0) - proper up normal
		nvrhi::Color clearNormalUp(0.5f, 1.0f, 0.5f, 1.f);
		nvrhi::utils::ClearColorAttachment(CmdList, GBufferFramebuffer, 0, clearBlue);
		nvrhi::utils::ClearColorAttachment(CmdList, GBufferFramebuffer, 1, clearBlack);
		nvrhi::utils::ClearColorAttachment(CmdList, GBufferFramebuffer, 2, clearNormalUp);
		nvrhi::utils::ClearColorAttachment(CmdList, GBufferFramebuffer, 3, clearBlack);
		nvrhi::utils::ClearDepthStencilAttachment(CmdList, GBufferFramebuffer, 1.0f, 0u);

		// Draw all meshes with per-mesh material binding
		for (size_t MeshIdx = 0; MeshIdx < AllMeshDrawData.size(); ++MeshIdx)
		{
			const auto& DrawData = AllMeshDrawData[MeshIdx];

			// Look up material for this mesh
			nvrhi::TextureHandle DiffuseTex = PlaceholderTexture;
			nvrhi::TextureHandle NormalTex = NormalPlaceholderTexture;
			float MatData[8] = {
				1.0f, 1.0f, 1.0f, 1.0f,   // AlbedoTint
				0.0f, 1.0f, 0.0f, 0.0f    // Metallic, Roughness, EmissiveStrength, Pad
			};
			if (DrawData.Mesh && Scene)
			{
				auto it = Scene->MeshMultiMaterialMap.find(DrawData.Mesh);
				if (it != Scene->MeshMultiMaterialMap.end())
				{
					auto& materials = it->second;
					if (!materials.empty())
					{
						if (auto PBRMat = std::dynamic_pointer_cast<FPBRMaterial>(materials[0]))
						{
							if (PBRMat->HasGPUTexture(IMaterial::ETextureType::Albedo))
							{
								DiffuseTex = PBRMat->GetGPUTexture(IMaterial::ETextureType::Albedo).GetTextureSRV();
								if (!DiffuseTex)
								{
									DiffuseTex = PlaceholderTexture;
								}
							}
							if (PBRMat->HasGPUTexture(IMaterial::ETextureType::Normal))
							{
								NormalTex = PBRMat->GetGPUTexture(IMaterial::ETextureType::Normal).GetTextureSRV();
								if (!NormalTex)
								{
									NormalTex = NormalPlaceholderTexture;
								}
							}
							FVec3 albedo = PBRMat->GetAlbedoColor();
							MatData[0] = albedo.x;
							MatData[1] = albedo.y;
							MatData[2] = albedo.z;
							MatData[4] = PBRMat->GetMetallic();
							MatData[5] = PBRMat->GetRoughness();
						}
					}
				}
			}
			CmdList->writeBuffer(MaterialConstantBuffer, MatData, sizeof(MatData));

			// Create binding set for this mesh's textures
			nvrhi::BindingSetDesc GBufferBindingSetDesc;
			GBufferBindingSetDesc.bindings = {
				nvrhi::BindingSetItem::ConstantBuffer(256, ViewConstantsBuffer),
				nvrhi::BindingSetItem::ConstantBuffer(257, MaterialConstantBuffer),
				nvrhi::BindingSetItem::Texture_SRV(0, DiffuseTex),
				nvrhi::BindingSetItem::Texture_SRV(1, NormalTex),
				nvrhi::BindingSetItem::Sampler(128, GBufferLinearSampler)
			};
			nvrhi::BindingSetHandle GBufferBindingSet = BindingCache.GetOrCreateBindingSet(GBufferBindingSetDesc, GBufferBindingLayout);

			// Build graphics state
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
			for (size_t i = 0; i < AllMeshDrawData.size() && i < 3; ++i)
			{
				HLVM_LOG(LogTest, info, TXT("Mesh[{}]: IndexCount={}, VB={}, IB={}"),
					i, AllMeshDrawData[i].IndexCount,
					static_cast<void*>(AllMeshDrawData[i].VertexBuffer.Get()),
					static_cast<void*>(AllMeshDrawData[i].IndexBuffer.Get()));
			}
		}

		// Transition GBuffer color textures to ShaderResource
		CmdList->setTextureState(GBufferDiffuseTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
		CmdList->setTextureState(GBufferSpecularTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
		CmdList->setTextureState(GBufferNormalsTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
		CmdList->setTextureState(GBufferEmissiveTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);

		// Transition depth to ShaderResource for compute SRV (D32 can be sampled directly)
		CmdList->setTextureState(GBufferDepthTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);

		// Precompute dispatch dimensions for compute passes
		uint32_t DispatchX = (CurrentFBInfo.width + 7) / 8;
		uint32_t DispatchY = (CurrentFBInfo.height + 7) / 8;

		// =====================================================================
		// HBAO Pass
		// =====================================================================
		{
			SSao::FHBAOConstants HBAOConstants;
			memset(&HBAOConstants, 0, sizeof(HBAOConstants));

			// Matrices (column-major)
			glm::mat4 invProj = glm::inverse(proj);
			memcpy(HBAOConstants.ProjMatrix,    glm::value_ptr(proj),      64);
			memcpy(HBAOConstants.InvProjMatrix, glm::value_ptr(invProj),   64);
			memcpy(HBAOConstants.ViewMatrix,    glm::value_ptr(view),      64);

			HBAOConstants.ScreenSize[0]    = float(CurrentFBInfo.width);
			HBAOConstants.ScreenSize[1]    = float(CurrentFBInfo.height);
			HBAOConstants.InvScreenSize[0] = 1.0f / float(CurrentFBInfo.width);
			HBAOConstants.InvScreenSize[1] = 1.0f / float(CurrentFBInfo.height);

			// Scene-scale radius for Sponza (~20 unit scene)
			HBAOConstants.SampleRadius     = 2.0f;
			HBAOConstants.AngleBias        = 0.2f;      // ~11 degrees
			HBAOConstants.MaxRadiusPixels  = 50.0f;
			HBAOConstants.AttenuationScale = 1.0f;
			HBAOConstants.MinAO            = 0.3f;
			HBAOConstants.DirectionCount   = 4;
			HBAOConstants.StepCount        = 6;

			SSao::FSSAOPass::FDesc HBAODesc;
			HBAODesc.DepthTexture  = GBufferDepthTexture;
			HBAODesc.NormalTexture = GBufferNormalsTexture;
			HBAODesc.OutputTexture = SSAOTexture;
			HBAODesc.OutputWidth   = CurrentFBInfo.width;
			HBAODesc.OutputHeight  = CurrentFBInfo.height;

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
		BilateralDesc.DepthTexture = GBufferDepthTexture;
		BilateralDesc.OutputTexture = SSAOBlurTexture;
		BilateralDesc.OutputWidth = CurrentFBInfo.width;
		BilateralDesc.OutputHeight = CurrentFBInfo.height;
		BilateralDesc.DepthSigma = 0.01f;
		BilateralBlurPass.Dispatch(CmdList, BilateralDesc);

		// Transition SSAO blur texture to SRV for lighting
		CmdList->setTextureState(SSAOBlurTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);

		// =====================================================================
		// Lighting Pass (same command list)
		// =====================================================================

		// Write lighting constants: InvViewProj (4 registers) + light data (4 registers) + shadow data (5 registers)
		float LightingData[52];
		glm::mat4 invViewProj = glm::inverse(proj * view);
		// Register 0-3: InvViewProj (column-major)
		memcpy(&LightingData[0], glm::value_ptr(invViewProj), 64);
		// Register 4: LightDir + LightIntensity
		LightingData[16] = 0.577f;
		LightingData[17] = 0.577f;
		LightingData[18] = 0.577f;
		LightingData[19] = 0.8f;
		// Register 5: CameraPos + ShadowHardness
		LightingData[20] = camX;
		LightingData[21] = camY;
		LightingData[22] = camZ;
		LightingData[23] = 16.0f;
		// Register 6: AmbientColor + MinAO
		LightingData[24] = 0.03f;
		LightingData[25] = 0.03f;
		LightingData[26] = 0.03f;
		LightingData[27] = 0.3f; // MinAO
		// Register 7: ScreenSize + Pad1
		LightingData[28] = float(CurrentFBInfo.width);
		LightingData[29] = float(CurrentFBInfo.height);
		LightingData[30] = 0.0f;
		LightingData[31] = 0.0f;
		// Register 8-11: LightViewProj (column-major)
		memcpy(&LightingData[32], glm::value_ptr(LightViewProj), 64);
		// Register 12: ShadowMapSize + ShadowBias + Pad2
		LightingData[48] = 2048.0f;
		LightingData[49] = 0.005f;
		LightingData[50] = 0.0f;
		LightingData[51] = 0.0f;
		CmdList->writeBuffer(LightingConstantBuffer, LightingData, sizeof(LightingData));

		// Transition HDR texture to UAV for compute write
		CmdList->setTextureState(HDRTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);

		// Create binding set
		nvrhi::BindingSetDesc LightingBindingSetDesc;
		LightingBindingSetDesc.bindings = {
			nvrhi::BindingSetItem::ConstantBuffer(256, LightingConstantBuffer),
			nvrhi::BindingSetItem::Texture_SRV(0, GBufferDiffuseTexture),
			nvrhi::BindingSetItem::Texture_SRV(1, GBufferSpecularTexture),
			nvrhi::BindingSetItem::Texture_SRV(2, GBufferNormalsTexture),
			nvrhi::BindingSetItem::Texture_SRV(3, GBufferEmissiveTexture),
			nvrhi::BindingSetItem::Texture_SRV(4, GBufferDepthTexture),
			nvrhi::BindingSetItem::Texture_SRV(5, ShadowMapTexture),
			nvrhi::BindingSetItem::Texture_SRV(6, SSAOBlurTexture),
			nvrhi::BindingSetItem::Sampler(129, ShadowSampler),
			nvrhi::BindingSetItem::Texture_UAV(384, HDRTexture)
		};
		nvrhi::BindingSetHandle LightingBindingSet = BindingCache.GetOrCreateBindingSet(LightingBindingSetDesc, LightingBindingLayout);

		// Dispatch
		nvrhi::ComputeState ComputeState;
		ComputeState.setPipeline(LightingPipeline);
		ComputeState.addBindingSet(LightingBindingSet);
		CmdList->setComputeState(ComputeState);
		CmdList->dispatch(DispatchX, DispatchY, 1);

		// Transition GBuffer textures back to RenderTarget for next frame
		CmdList->setTextureState(GBufferDiffuseTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::RenderTarget);
		CmdList->setTextureState(GBufferSpecularTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::RenderTarget);
		CmdList->setTextureState(GBufferNormalsTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::RenderTarget);
		CmdList->setTextureState(GBufferEmissiveTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::RenderTarget);
		CmdList->setTextureState(GBufferDepthTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::DepthWrite);

		// =====================================================================
		// Tone Mapping Pass
		// =====================================================================
		float TonemapData[8] = {
			1.0f, 2.2f, 0.0f, 0.0f,                           // Exposure, Gamma, Mode=ACES, Pad
			float(CurrentFBInfo.width), float(CurrentFBInfo.height), 0.0f, 0.0f  // TextureSize
		};
		CmdList->writeBuffer(ToneMapConstantBuffer, TonemapData, sizeof(TonemapData));

		CmdList->setTextureState(HDRTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
		CmdList->setTextureState(SDRTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);

		nvrhi::BindingSetDesc ToneMapBindingSetDesc;
		ToneMapBindingSetDesc.bindings = {
			nvrhi::BindingSetItem::ConstantBuffer(256, ToneMapConstantBuffer),
			nvrhi::BindingSetItem::Texture_SRV(0, HDRTexture),
			nvrhi::BindingSetItem::Texture_UAV(384, SDRTexture)
		};
		nvrhi::BindingSetHandle ToneMapBindingSet = BindingCache.GetOrCreateBindingSet(ToneMapBindingSetDesc, ToneMapBindingLayout);

		nvrhi::ComputeState ToneMapComputeState;
		ToneMapComputeState.setPipeline(ToneMapPipeline);
		ToneMapComputeState.addBindingSet(ToneMapBindingSet);
		CmdList->setComputeState(ToneMapComputeState);
		CmdList->dispatch(DispatchX, DispatchY, 1);

		// =====================================================================
		// Frame dump Phase 1: Prepare copy on command list (BEFORE close)
		// =====================================================================
		if (FrameDumper.IsEnabled()) {
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
		if (FrameDumper.IsEnabled()) {
			int frameNum = FrameDumper.GetCurrentFrame();
			if (FrameDumper.ReadbackAndSave()) {
				HLVM_LOG(LogTest, info, TXT("Dumped frame {}"), frameNum);
				// Compare against reference image if available
				FString refPath = FString::Format(
					TXT("{}/../../Test/TestSponzaDeferred_Data/Reference/frame_{:04d}.png"),
					*GExecutablePath, frameNum);
				FrameDumper.CompareAgainstReference(refPath, 0.01f);
			}
			if (FrameDumper.IsLastFrame()) {
				return;  // Skip presentation on last frame
			}
		}
	}

	virtual void BackBufferResizing() override
	{
		HDRTexture = nullptr;
		SDRTexture = nullptr;
		GBufferFramebuffer = nullptr;
		GBufferDiffuseTexture = nullptr;
		GBufferSpecularTexture = nullptr;
		GBufferNormalsTexture = nullptr;
		GBufferEmissiveTexture = nullptr;
		GBufferDepthTexture = nullptr;
		LoadedDiffuseTexture = nullptr;
		ShadowMapFramebuffer = nullptr;
		SSAOTexture = nullptr;
		SSAOBlurTexture = nullptr;
		BindingCache.Clear();
	}

private:
	nvrhi::IDevice*		   NvrhiDevice = nullptr;
	nvrhi::FramebufferInfo FBInfo;
	FString				   WindowTitle;

	std::shared_ptr<FScene3DNode> Scene;

	nvrhi::ShaderHandle GBufferVS;
	nvrhi::ShaderHandle GBufferPS;
	nvrhi::ShaderHandle LightingCS;
	nvrhi::ShaderHandle ToneMapCS;

	nvrhi::TextureHandle	 GBufferDiffuseTexture;
	nvrhi::TextureHandle	 GBufferSpecularTexture;
	nvrhi::TextureHandle	 GBufferNormalsTexture;
	nvrhi::TextureHandle	 GBufferEmissiveTexture;
	nvrhi::TextureHandle	 GBufferDepthTexture;
	nvrhi::FramebufferHandle GBufferFramebuffer;
	nvrhi::TextureHandle	 PlaceholderTexture;
	nvrhi::TextureHandle	 NormalPlaceholderTexture;
	nvrhi::TextureHandle	 LoadedDiffuseTexture;

	nvrhi::InputLayoutHandle	  GBufferInputLayout;
	nvrhi::BindingLayoutHandle	  GBufferBindingLayout;
	nvrhi::BufferHandle			  ViewConstantsBuffer;
	nvrhi::BufferHandle			  MaterialConstantBuffer;
	nvrhi::GraphicsPipelineHandle GBufferPipeline;
	nvrhi::SamplerHandle		  GBufferLinearSampler;

	nvrhi::BindingLayoutHandle	 LightingBindingLayout;
	nvrhi::ComputePipelineHandle LightingPipeline;
	nvrhi::BufferHandle			 LightingConstantBuffer;
	nvrhi::TextureHandle		 HDRTexture;

	nvrhi::BindingLayoutHandle	 ToneMapBindingLayout;
	nvrhi::ComputePipelineHandle ToneMapPipeline;
	nvrhi::BufferHandle			 ToneMapConstantBuffer;
	nvrhi::TextureHandle		 SDRTexture;

	nvrhi::ShaderHandle			 ShadowVS;
	nvrhi::GraphicsPipelineHandle ShadowPipeline;
	nvrhi::TextureHandle		 ShadowMapTexture;
	nvrhi::FramebufferHandle	 ShadowMapFramebuffer;
	nvrhi::BufferHandle			 ShadowConstantsBuffer;
	nvrhi::SamplerHandle		 ShadowSampler;
	nvrhi::BindingLayoutHandle	 ShadowBindingLayout;
	glm::mat4					 LightViewProj = glm::mat4(1.0f);

	// SSAO resources
	nvrhi::TextureHandle		 SSAOTexture;
	nvrhi::TextureHandle		 SSAOBlurTexture;
	SSao::FSSAOPass				 HBAOPass;
	FJointBilateralUpsamplePass BilateralBlurPass;

	FRenderPassDumper			 FrameDumper;

	// Sponza geometry buffers (cached)
	nvrhi::BufferHandle			 SponzaVertexBuffer;
	nvrhi::BufferHandle			 SponzaIndexBuffer;
	std::shared_ptr<FStaticMesh> SponzaMesh;
	uint32_t					 SponzaIndexCount = 0;

	// Per-mesh data for multi-mesh rendering
	struct FMeshDrawData {
		nvrhi::BufferHandle VertexBuffer;
		nvrhi::BufferHandle IndexBuffer;
		std::shared_ptr<FStaticMesh> Mesh;
		uint32_t IndexCount;
	};
	TVector<FMeshDrawData> AllMeshDrawData;

	FBindingCache BindingCache;

	glm::vec3 SceneCenter = glm::vec3(0.f);  // Calculated from scene bounding box
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
