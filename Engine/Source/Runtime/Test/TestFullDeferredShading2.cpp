/**
 * HLVM-Engine TestFullDeferredShading2 - Full Deferred Shading Pipeline
 *
 * BRAND NEW test file implementing GBuffer → Lighting → Blit architecture.
 * Uses FCommonRenderPasses::BlitTexture (NO FDeferredBlitPass).
 *
 * GBuff PASS (3 MRTs):
 *   - MRT0: Position (world space)
 *   - MRT1: UV (texture coordinates)
 *   - MRT2: Normal (world space normal)
 *
 * LIGHTING PASS (Compute shader):
 *   - Reads: t0=Position, t1=Normal
 *   - Writes: u0=HDR output texture
 *
 * BLIT PASS:
 *   - Uses FCommonRenderPasses::BlitTexture (loads pre-compiled from TestRayTracedTriangle_Data/)
 *
 * RENDERING PIPELINE:
 *   1. GBuffer Pass: Render cube to 3 MRTs (Position, UV, Normal)
 *   2. Lighting Pass: Compute shader reads GBuffer, writes HDR
 *   3. Blit Pass: Copy HDR to swapchain via BlitTexture
 */

#include "Test.h"
#include "Renderer/DeviceManager.h"
#include "Renderer/Window/WindowDefinition.h"
#include "Renderer/SceneGraph/PerspectiveCameraNode.h"
#include "Renderer/Common/FCommonRenderPasses.h"
#include "Renderer/Common/FBindingCache.h"
#include "Renderer/RHI/Object/Buffer.h"
#include "Renderer/ShaderMake/ShaderBlob.h"
#include <nvrhi/utils.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <vector>
#include <memory>

DECLARE_LOG_CATEGORY(LogTest)

#if HLVM_VULKAN_RENDERER
	#include "Renderer/Window/GLFW3/GLFW3VulkanWindow.h"
	#include "Renderer/RHI/Vulkan/VulkanDefinition.h"
	#include "Renderer/RHI/RHICommon.h"

	#pragma clang diagnostic push
	#pragma clang diagnostic ignored "-Wunreachable-code"
	#pragma clang diagnostic ignored "-Wcovered-switch-default"

using namespace std;

// =============================================================================
// CONFIGURATION
// =============================================================================

const uint32_t	   WIDTH = 800;
const uint32_t	   HEIGHT = 600;
static const char* WINDOW_TITLE = "Full Deferred Shading 2";

// =============================================================================
// TEST STRUCTURE
// =============================================================================

struct FDeferredShading2Context
{
	// DeviceManager
	TUniquePtr<FDeviceManager> DeviceManager;
	nvrhi::IDevice*			   NvrhiDevice = nullptr;

	// Command list
	nvrhi::CommandListHandle NvrhiCommandList;

	// GBuffer shaders
	nvrhi::ShaderHandle GBufferVS;
	nvrhi::ShaderHandle GBufferPS;

	// Lighting compute shader
	nvrhi::ShaderHandle LightingCS;

	// Buffers
	TUniquePtr<FStaticVertexBuffer> VertexBuffer;
	TUniquePtr<FStaticIndexBuffer>	IndexBuffer;

	// GBuffer pipeline
	nvrhi::InputLayoutHandle	  GBufferInputLayout;
	nvrhi::BindingLayoutHandle	  GBufferBindingLayout;
	nvrhi::GraphicsPipelineHandle GBufferPipeline;
	nvrhi::FramebufferHandle	  GBufferFramebuffer;

	// Lighting pipeline
	nvrhi::BindingLayoutHandle	 LightingBindingLayout;
	nvrhi::ComputePipelineHandle LightingPipeline;

	// GBuffer textures (5 MRTs)
	nvrhi::TextureHandle PositionTexture;  // MRT0: World Position
	nvrhi::TextureHandle AlbedoTexture;    // MRT1: Albedo RGB + Specular (A)
	nvrhi::TextureHandle NormalTexture;    // MRT2: World Normal + Roughness (A)
	nvrhi::TextureHandle DepthTexture;     // MRT3: Depth
	nvrhi::TextureHandle EmissiveTexture;  // MRT4: Emissive RGB

	// HDR output texture (UAV)
	nvrhi::TextureHandle HDRTexture;

	// Binding cache for BlitTexture
	FBindingCache BindingCache;

	// Camera
	TUniquePtr<FPerspectiveCameraNode> Camera;

	// Dispatch groups
	uint32_t DispatchX = (WIDTH + 7) / 8;
	uint32_t DispatchY = (HEIGHT + 7) / 8;
};

// Display mode cycling for GBuffer visualization
enum class EGBufferDisplayMode
{
	Lighting,  // Full deferred lighting result
	Albedo,    // GBuffer MRT1: Albedo RGB + Specular (A)
	Normal,    // GBuffer MRT2: World Normal + Roughness (A)
	Depth,     // GBuffer MRT3: Depth
	Specular,  // GBuffer MRT1 Alpha: Specular
	Emissive   // GBuffer MRT4: Emissive RGB
};

static EGBufferDisplayMode g_DisplayMode = EGBufferDisplayMode::Lighting;
static float			   g_DisplayModeTimer = 0.0f;
static const float		   g_DisplayModeInterval = 0.5f; // Cycle every 0.5 seconds

// =============================================================================
// GEOMETRY DATA (Cube with Position, UV, Normal)
// =============================================================================

// Vertex: Position(3) + UV(2) + Normal(3) = 32 bytes
struct FDeferredVertex
{
	float Position[3]; // vec3
	float UV[2];	   // vec2
	float Normal[3];   // vec3
};

// Cube vertices with full vertex data
static const FDeferredVertex Vertices[24] = {
	// Front face (+Z)
	{ { -0.5f, -0.5f, 0.5f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
	{ { 0.5f, -0.5f, 0.5f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
	{ { 0.5f, 0.5f, 0.5f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } },
	{ { -0.5f, 0.5f, 0.5f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } },
	// Back face (-Z)
	{ { 0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f } },
	{ { -0.5f, -0.5f, -0.5f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, -1.0f } },
	{ { -0.5f, 0.5f, -0.5f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } },
	{ { 0.5f, 0.5f, -0.5f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } },
	// Top face (+Y)
	{ { -0.5f, 0.5f, 0.5f }, { 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } },
	{ { 0.5f, 0.5f, 0.5f }, { 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } },
	{ { 0.5f, 0.5f, -0.5f }, { 1.0f, 1.0f }, { 0.0f, 1.0f, 0.0f } },
	{ { -0.5f, 0.5f, -0.5f }, { 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f } },
	// Bottom face (-Y)
	{ { -0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f }, { 0.0f, -1.0f, 0.0f } },
	{ { 0.5f, -0.5f, -0.5f }, { 1.0f, 0.0f }, { 0.0f, -1.0f, 0.0f } },
	{ { 0.5f, -0.5f, 0.5f }, { 1.0f, 1.0f }, { 0.0f, -1.0f, 0.0f } },
	{ { -0.5f, -0.5f, 0.5f }, { 0.0f, 1.0f }, { 0.0f, -1.0f, 0.0f } },
	// Right face (+X)
	{ { 0.5f, -0.5f, 0.5f }, { 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
	{ { 0.5f, -0.5f, -0.5f }, { 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
	{ { 0.5f, 0.5f, -0.5f }, { 1.0f, 1.0f }, { 1.0f, 0.0f, 0.0f } },
	{ { 0.5f, 0.5f, 0.5f }, { 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f } },
	// Left face (-X)
	{ { -0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f }, { -1.0f, 0.0f, 0.0f } },
	{ { -0.5f, -0.5f, 0.5f }, { 1.0f, 0.0f }, { -1.0f, 0.0f, 0.0f } },
	{ { -0.5f, 0.5f, 0.5f }, { 1.0f, 1.0f }, { -1.0f, 0.0f, 0.0f } },
	{ { -0.5f, 0.5f, -0.5f }, { 0.0f, 1.0f }, { -1.0f, 0.0f, 0.0f } },
};

// Cube indices - 36 indices for 12 triangles (6 faces)
static const uint32_t Indices[36] = {
	// Front
	0,
	1,
	2,
	0,
	2,
	3,
	// Back
	4,
	5,
	6,
	4,
	6,
	7,
	// Top
	8,
	9,
	10,
	8,
	10,
	11,
	// Bottom
	12,
	13,
	14,
	12,
	14,
	15,
	// Right
	16,
	17,
	18,
	16,
	18,
	19,
	// Left
	20,
	21,
	22,
	20,
	22,
	23,
};

// =============================================================================
// UNIFORM BUFFERS
// =============================================================================

// GBuffer vertex shader uniforms
struct FGBufferViewConstants
{
	alignas(16) float ViewProj[4][4];
	alignas(16) float Model[4][4];
};

// GBuffer fragment shader uniforms (material)
struct FGBufferMaterialConstants
{
	alignas(16) float AlbedoColor[4];
	float SpecularIntensity;
	float Roughness;
	float Pad1;
	float Pad2;
};

// Lighting compute shader uniforms
struct FLightingConstants
{
	alignas(16) float LightDir[3];
	float LightIntensity;
	alignas(16) float CameraPos[3];
	float ShadowHardness;
	alignas(16) float AmbientColor[3];
	float Pad;
};

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

static vector<char> ReadBinaryFile(const std::string& Filename)
{
	ifstream file(Filename, ios::ate | ios::binary);
	if (!file.is_open())
	{
		throw runtime_error("Failed to open file: " + Filename);
	}

	size_t		 fileSize = static_cast<size_t>(file.tellg());
	vector<char> buffer(fileSize);

	file.seekg(0);
	file.read(buffer.data(), static_cast<streamsize>(fileSize));
	file.close();

	return buffer;
}

static void CleanupResources(FDeferredShading2Context& Context)
{
	HLVM_LOG(LogTest, info, TXT("Cleaning up resources..."));

	// Wait for GPU
	if (Context.NvrhiDevice)
	{
		Context.NvrhiDevice->waitForIdle();
	}

	// Release NVRHI resources
	Context.LightingPipeline = nullptr;
	Context.LightingBindingLayout = nullptr;
	Context.GBufferPipeline = nullptr;
	Context.GBufferBindingLayout = nullptr;
	Context.GBufferFramebuffer = nullptr;
	Context.GBufferInputLayout = nullptr;

	Context.HDRTexture = nullptr;
	Context.PositionTexture = nullptr;
	Context.AlbedoTexture = nullptr;
	Context.NormalTexture = nullptr;
	Context.DepthTexture = nullptr;
	Context.EmissiveTexture = nullptr;

	Context.LightingCS = nullptr;
	Context.GBufferPS = nullptr;
	Context.GBufferVS = nullptr;

	Context.IndexBuffer = nullptr;
	Context.VertexBuffer = nullptr;

	Context.NvrhiCommandList = nullptr;

	// Shutdown Blit resources
	FCommonRenderPasses::Shutdown();

	// Shutdown BindingCache
	Context.BindingCache.Shutdown();

	// Shutdown DeviceManager
	if (Context.DeviceManager)
	{
		Context.DeviceManager->Shutdown();
		Context.DeviceManager.reset();
	}
	Context.NvrhiDevice = nullptr;

	Context.Camera = nullptr;
}

// =============================================================================
// TEST IMPLEMENTATION
// =============================================================================

RECORD_BOOL(TestFullDeferredShading2)
{
	HLVM_LOG(LogTest, info, TXT("Starting Full Deferred Shading 2 Test..."));

	FDeferredShading2Context Context;

	try
	{
		// =====================================================================
		// 1. Create window properties
		// =====================================================================
		HLVM_LOG(LogTest, info, TXT("Creating window..."));
		IWindow::Properties WindowProps;
		WindowProps.Title = WINDOW_TITLE;
		WindowProps.Extent = { WIDTH, HEIGHT };
		WindowProps.Resizable = true;
		WindowProps.VSync = IWindow::EVsync::Off;

		// =====================================================================
		// 2. Create DeviceManager for Vulkan
		// =====================================================================
		HLVM_LOG(LogTest, info, TXT("Creating DeviceManager..."));
		Context.DeviceManager = FDeviceManager::Create(nvrhi::GraphicsAPI::VULKAN);
		if (!Context.DeviceManager)
		{
			throw runtime_error("Failed to create DeviceManager");
		}

		FDeviceCreationParameters& DeviceParams = const_cast<FDeviceCreationParameters&>(Context.DeviceManager->GetDeviceParams());
		DeviceParams.BackBufferWidth = WIDTH;
		DeviceParams.BackBufferHeight = HEIGHT;
		DeviceParams.SwapChainBufferCount = 2;
		DeviceParams.VSyncMode = 0;
		DeviceParams.bEnableDebugRuntime = HLVM_BUILD_DEBUG;
		DeviceParams.bEnableNVRHIValidationLayer = HLVM_BUILD_DEBUG;
		DeviceParams.bEnableRayTracingExtensions = false;

		if (!Context.DeviceManager->CreateWindowDeviceAndSwapChain(WindowProps))
		{
			throw runtime_error("Failed to create device and swapchain");
		}

		Context.NvrhiDevice = Context.DeviceManager->GetDevice();
		if (!Context.NvrhiDevice)
		{
			throw runtime_error("Failed to get NVRHI device");
		}

		HLVM_LOG(LogTest, info, TXT("DeviceManager created successfully. GPU: {}"), FString(Context.DeviceManager->GetRendererString()));

		// =====================================================================
		// 3. Create command list
		// =====================================================================
		HLVM_LOG(LogTest, info, TXT("Creating command list..."));
		nvrhi::CommandListParameters Params;
		Params.enableImmediateExecution = false;
		Context.NvrhiCommandList = Context.NvrhiDevice->createCommandList(Params);
		Context.NvrhiCommandList->open();

		// =====================================================================
		// 4. Load shaders from ShaderMake blob files
		// =====================================================================
		HLVM_LOG(LogTest, info, TXT("Loading shaders from blobs..."));
		const auto DataDir = FString::Format(TXT("{}/../../Test/{}_Data"), *GExecutablePath, *GExecutableName);

		// Load GBuffer vertex shader blob
		auto		GBufferVSBlob = ReadBinaryFile(FPath::Combine(DataDir, TXT("gbuffer_vs.sblob")).string());
		const void* GBufferVSBinary = nullptr;
		size_t		GBufferVSBinarySize = 0;
		if (!ShaderMake::FindPermutationInBlob(GBufferVSBlob.data(), GBufferVSBlob.size(), nullptr, 0, &GBufferVSBinary, &GBufferVSBinarySize))
		{
			throw runtime_error("Failed to extract GBufferVS from blob");
		}
		HLVM_LOG(LogTest, info, TXT("GBufferVS loaded ({} bytes)"), GBufferVSBinarySize);

		// Load GBuffer fragment shader blob
		auto		GBufferPSBlob = ReadBinaryFile(FPath::Combine(DataDir, TXT("gbuffer_ps.sblob")).string());
		const void* GBufferPSBinary = nullptr;
		size_t		GBufferPSBinarySize = 0;
		if (!ShaderMake::FindPermutationInBlob(GBufferPSBlob.data(), GBufferPSBlob.size(), nullptr, 0, &GBufferPSBinary, &GBufferPSBinarySize))
		{
			throw runtime_error("Failed to extract GBufferPS from blob");
		}
		HLVM_LOG(LogTest, info, TXT("GBufferPS loaded ({} bytes)"), GBufferPSBinarySize);

		// Load lighting compute shader blob
		auto		LightingCSBlob = ReadBinaryFile(FPath::Combine(DataDir, TXT("deferred_lighting_cs.sblob")).string());
		const void* LightingCSBinary = nullptr;
		size_t		LightingCSBinarySize = 0;
		if (!ShaderMake::FindPermutationInBlob(LightingCSBlob.data(), LightingCSBlob.size(), nullptr, 0, &LightingCSBinary, &LightingCSBinarySize))
		{
			throw runtime_error("Failed to extract LightingCS from blob");
		}
		HLVM_LOG(LogTest, info, TXT("LightingCS loaded ({} bytes)"), LightingCSBinarySize);

		// Create shader handles
		nvrhi::ShaderDesc VSDesc;
		VSDesc.setShaderType(nvrhi::ShaderType::Vertex);
		Context.GBufferVS = Context.NvrhiDevice->createShader(VSDesc, GBufferVSBinary, GBufferVSBinarySize);

		nvrhi::ShaderDesc PSDesc;
		PSDesc.setShaderType(nvrhi::ShaderType::Pixel);
		Context.GBufferPS = Context.NvrhiDevice->createShader(PSDesc, GBufferPSBinary, GBufferPSBinarySize);

		nvrhi::ShaderDesc CSDesc;
		CSDesc.setShaderType(nvrhi::ShaderType::Compute);
		Context.LightingCS = Context.NvrhiDevice->createShader(CSDesc, LightingCSBinary, LightingCSBinarySize);

		// =====================================================================
		// 5. Create vertex buffer
		// =====================================================================
		HLVM_LOG(LogTest, info, TXT("Creating vertex buffer..."));
		Context.VertexBuffer = TUniquePtr<FStaticVertexBuffer>(new FStaticVertexBuffer());
		Context.VertexBuffer->Initialize(Context.NvrhiCommandList.Get(), Context.NvrhiDevice, Vertices, sizeof(Vertices));

		// =====================================================================
		// 6. Create index buffer
		// =====================================================================
		HLVM_LOG(LogTest, info, TXT("Creating index buffer..."));
		Context.IndexBuffer = TUniquePtr<FStaticIndexBuffer>(new FStaticIndexBuffer());
		Context.IndexBuffer->Initialize(Context.NvrhiCommandList.Get(), Context.NvrhiDevice, Indices, sizeof(Indices), nvrhi::Format::R32_UINT);

		Context.NvrhiCommandList->close();
		Context.NvrhiDevice->executeCommandList(Context.NvrhiCommandList);

		// =====================================================================
		// 7. Create GBuffer textures (5 MRTs)
		// =====================================================================
		HLVM_LOG(LogTest, info, TXT("Creating GBuffer textures..."));

		// MRT0: Position (RGBA16_FLOAT - world position)
		{
			nvrhi::TextureDesc Desc;
			Desc.dimension = nvrhi::TextureDimension::Texture2D;
			Desc.width = WIDTH;
			Desc.height = HEIGHT;
			Desc.mipLevels = 1;
			Desc.arraySize = 1;
			Desc.format = nvrhi::Format::RGBA16_FLOAT;
			Desc.isUAV = false;
			Desc.isRenderTarget = true;
			Desc.isTypeless = false;
			Desc.initialState = nvrhi::ResourceStates::RenderTarget;
			Desc.keepInitialState = true;
			Desc.debugName = "PositionTexture";
			Context.PositionTexture = Context.NvrhiDevice->createTexture(Desc);
		}

		// MRT1: Albedo (RGBA16_FLOAT - RGB albedo + A specular)
		{
			nvrhi::TextureDesc Desc;
			Desc.dimension = nvrhi::TextureDimension::Texture2D;
			Desc.width = WIDTH;
			Desc.height = HEIGHT;
			Desc.mipLevels = 1;
			Desc.arraySize = 1;
			Desc.format = nvrhi::Format::RGBA16_FLOAT;
			Desc.isUAV = false;
			Desc.isRenderTarget = true;
			Desc.isTypeless = false;
			Desc.initialState = nvrhi::ResourceStates::RenderTarget;
			Desc.keepInitialState = true;
			Desc.debugName = "AlbedoTexture";
			Context.AlbedoTexture = Context.NvrhiDevice->createTexture(Desc);
		}

		// MRT2: Normal (RGBA16_FLOAT - RGB normal + A roughness)
		{
			nvrhi::TextureDesc Desc;
			Desc.dimension = nvrhi::TextureDimension::Texture2D;
			Desc.width = WIDTH;
			Desc.height = HEIGHT;
			Desc.mipLevels = 1;
			Desc.arraySize = 1;
			Desc.format = nvrhi::Format::RGBA16_FLOAT;
			Desc.isUAV = false;
			Desc.isRenderTarget = true;
			Desc.isTypeless = false;
			Desc.initialState = nvrhi::ResourceStates::RenderTarget;
			Desc.keepInitialState = true;
			Desc.debugName = "NormalTexture";
			Context.NormalTexture = Context.NvrhiDevice->createTexture(Desc);
		}

		// MRT3: Emissive (RGBA16_FLOAT - RGB emissive + depth in alpha)
		// Note: Depth is packed into alpha channel, separate D32 texture used for depth attachment
		{
			nvrhi::TextureDesc Desc;
			Desc.dimension = nvrhi::TextureDimension::Texture2D;
			Desc.width = WIDTH;
			Desc.height = HEIGHT;
			Desc.mipLevels = 1;
			Desc.arraySize = 1;
			Desc.format = nvrhi::Format::RGBA16_FLOAT;
			Desc.isUAV = false;
			Desc.isRenderTarget = true;
			Desc.isTypeless = false;
			Desc.initialState = nvrhi::ResourceStates::RenderTarget;
			Desc.keepInitialState = true;
			Desc.debugName = "EmissiveTexture";
			Context.EmissiveTexture = Context.NvrhiDevice->createTexture(Desc);
		}

		// =====================================================================
		// 8. Create HDR output texture (UAV)
		// =====================================================================
		HLVM_LOG(LogTest, info, TXT("Creating HDR output texture..."));
		{
			nvrhi::TextureDesc Desc;
			Desc.dimension = nvrhi::TextureDimension::Texture2D;
			Desc.width = WIDTH;
			Desc.height = HEIGHT;
			Desc.mipLevels = 1;
			Desc.arraySize = 1;
			Desc.format = nvrhi::Format::RGBA16_FLOAT;
			Desc.isUAV = true;
			Desc.isRenderTarget = false;
			Desc.isTypeless = false;
			Desc.initialState = nvrhi::ResourceStates::UnorderedAccess;
			Desc.keepInitialState = true;
			Desc.debugName = "HDRTexture";
			Context.HDRTexture = Context.NvrhiDevice->createTexture(Desc);
		}

		// =====================================================================
		// 9. Create depth texture
		// =====================================================================
		HLVM_LOG(LogTest, info, TXT("Creating depth texture..."));
		{
			nvrhi::TextureDesc Desc;
			Desc.dimension = nvrhi::TextureDimension::Texture2D;
			Desc.width = WIDTH;
			Desc.height = HEIGHT;
			Desc.mipLevels = 1;
			Desc.arraySize = 1;
			Desc.format = nvrhi::Format::D32;
			Desc.isUAV = false;
			Desc.isRenderTarget = true; // MUST be true for depth attachment
			Desc.isTypeless = true;
			Desc.initialState = nvrhi::ResourceStates::DepthWrite;
			Desc.keepInitialState = true;
			Desc.debugName = "DepthTexture";
			Context.DepthTexture = Context.NvrhiDevice->createTexture(Desc);
		}

		// =====================================================================
		// 10. Create GBuffer framebuffer (4 color MRTs + depth attachment)
		// =====================================================================
		HLVM_LOG(LogTest, info, TXT("Creating GBuffer framebuffer..."));
		{
			nvrhi::FramebufferDesc FBDesc;

			// MRT0: Position
			nvrhi::FramebufferAttachment PosAttach;
			PosAttach.setTexture(Context.PositionTexture);
			FBDesc.addColorAttachment(PosAttach);

			// MRT1: Albedo + Specular
			nvrhi::FramebufferAttachment AlbedoAttach;
			AlbedoAttach.setTexture(Context.AlbedoTexture);
			FBDesc.addColorAttachment(AlbedoAttach);

			// MRT2: Normal + Roughness
			nvrhi::FramebufferAttachment NormalAttach;
			NormalAttach.setTexture(Context.NormalTexture);
			FBDesc.addColorAttachment(NormalAttach);

			// MRT3: Emissive RGB + Depth (packed in alpha)
			nvrhi::FramebufferAttachment EmissiveAttach;
			EmissiveAttach.setTexture(Context.EmissiveTexture);
			FBDesc.addColorAttachment(EmissiveAttach);

			// Separate depth attachment (D32)
			nvrhi::FramebufferAttachment DepthAttach;
			DepthAttach.setTexture(Context.DepthTexture);
			FBDesc.setDepthAttachment(DepthAttach);

			Context.GBufferFramebuffer = Context.NvrhiDevice->createFramebuffer(FBDesc);
		}

		// =====================================================================
		// 11. Create GBuffer input layout (3 vertex attributes)
		// =====================================================================
		HLVM_LOG(LogTest, info, TXT("Creating GBuffer input layout..."));
		{
			nvrhi::VertexAttributeDesc Attrs[3];
			// Location 0: Position (vec3)
			Attrs[0].setName("POSITION").setFormat(nvrhi::Format::RGB32_FLOAT).setOffset(0).setElementStride(sizeof(FDeferredVertex));
			// Location 1: UV (vec2)
			Attrs[1].setName("TEXCOORD0").setFormat(nvrhi::Format::RG32_FLOAT).setOffset(12).setElementStride(sizeof(FDeferredVertex));
			// Location 2: Normal (vec3)
			Attrs[2].setName("NORMAL").setFormat(nvrhi::Format::RGB32_FLOAT).setOffset(20).setElementStride(sizeof(FDeferredVertex));

			Context.GBufferInputLayout = Context.NvrhiDevice->createInputLayout(Attrs, 3, Context.GBufferVS);
		}

		// =====================================================================
		// 12. Create GBuffer binding layout
		// =====================================================================
		HLVM_LOG(LogTest, info, TXT("Creating GBuffer binding layout..."));
		{
			nvrhi::BindingLayoutDesc LayoutDesc;
			LayoutDesc.visibility = nvrhi::ShaderType::Vertex | nvrhi::ShaderType::Pixel;

			// Set binding offsets to 0 to match SPIR-V bindings
			nvrhi::VulkanBindingOffsets offsets;
			offsets.setConstantBufferOffset(0).setShaderResourceOffset(0).setSamplerOffset(0).setUnorderedAccessViewOffset(0);
			LayoutDesc.setBindingOffsets(offsets);

			// b2 → 258 (ViewConstants), b0 → 256 (MaterialConstants)
			LayoutDesc.addItem(nvrhi::BindingLayoutItem::ConstantBuffer(258)); // ViewConstants
			LayoutDesc.addItem(nvrhi::BindingLayoutItem::ConstantBuffer(256)); // MaterialConstants

			Context.GBufferBindingLayout = Context.NvrhiDevice->createBindingLayout(LayoutDesc);
		}

		// =====================================================================
		// 13. Create GBuffer pipeline
		// =====================================================================
		HLVM_LOG(LogTest, info, TXT("Creating GBuffer pipeline..."));
		{
			nvrhi::GraphicsPipelineDesc PipelineDesc;
			PipelineDesc.setVertexShader(Context.GBufferVS);
			PipelineDesc.setPixelShader(Context.GBufferPS);
			PipelineDesc.setInputLayout(Context.GBufferInputLayout);
			PipelineDesc.addBindingLayout(Context.GBufferBindingLayout);
			PipelineDesc.setPrimType(nvrhi::PrimitiveType::TriangleList);
			PipelineDesc.renderState.setRasterState(nvrhi::RasterState().setCullBack());
			PipelineDesc.renderState.depthStencilState
				.setDepthTestEnable(true)
				.setDepthWriteEnable(true)
				.setDepthFunc(nvrhi::ComparisonFunc::Less);

			Context.GBufferPipeline = Context.NvrhiDevice->createGraphicsPipeline(PipelineDesc, Context.GBufferFramebuffer->getFramebufferInfo());
		}

		// =====================================================================
		// 14. Create Lighting binding layout
		// =====================================================================
		HLVM_LOG(LogTest, info, TXT("Creating Lighting binding layout..."));
		{
			nvrhi::BindingLayoutDesc LayoutDesc;
			LayoutDesc.visibility = nvrhi::ShaderType::Compute;

			// Set binding offsets to 0
			nvrhi::VulkanBindingOffsets offsets;
			offsets.setConstantBufferOffset(0).setShaderResourceOffset(0).setSamplerOffset(0).setUnorderedAccessViewOffset(0);
			LayoutDesc.setBindingOffsets(offsets);

			// b0 → 256 (LightingConstants), t0 → 0 (Albedo), t1 → 1 (Normal), t2 → 2 (Depth), t3 → 3 (Emissive), u0 → 384 (HDR output)
			LayoutDesc.addItem(nvrhi::BindingLayoutItem::ConstantBuffer(256)); // LightConstants
			LayoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(0));	    // Albedo texture
			LayoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(1));	    // Normal texture
			LayoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(2));	    // Depth texture
			LayoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(3));	    // Emissive texture
			LayoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_UAV(384));	// HDR output

			Context.LightingBindingLayout = Context.NvrhiDevice->createBindingLayout(LayoutDesc);
		}

		// =====================================================================
		// 15. Create Lighting compute pipeline
		// =====================================================================
		HLVM_LOG(LogTest, info, TXT("Creating Lighting compute pipeline..."));
		{
			nvrhi::ComputePipelineDesc PipelineDesc;
			PipelineDesc.setComputeShader(Context.LightingCS);
			PipelineDesc.addBindingLayout(Context.LightingBindingLayout);

			Context.LightingPipeline = Context.NvrhiDevice->createComputePipeline(PipelineDesc);
		}

		// =====================================================================
		// 16. Initialize BindingCache for BlitTexture
		// =====================================================================
		HLVM_LOG(LogTest, info, TXT("Initializing BindingCache..."));
		Context.BindingCache.SetDevice(Context.NvrhiDevice);

		// =====================================================================
		// 17. Create camera
		// =====================================================================
		HLVM_LOG(LogTest, info, TXT("Creating camera..."));
		Context.Camera = TUniquePtr<FPerspectiveCameraNode>(new FPerspectiveCameraNode());
		Context.Camera->SetPosition(FVec3(2.0f, 2.0f, 2.0f));
		Context.Camera->SetRotation(FVec3(0.0f, 0.0f, 0.0f));
		Context.Camera->SetFovY(glm::radians(45.0f));
		Context.Camera->SetAspectRatio(static_cast<float>(WIDTH) / static_cast<float>(HEIGHT));
		Context.Camera->SetNearPlane(0.1f);
		Context.Camera->SetFarPlane(100.0f);
		Context.Camera->UpdateWorldTransform();

		// =====================================================================
		// 18. Render loop
		// =====================================================================
		constexpr int MaxFrames = 600;
		HLVM_LOG(LogTest, info, TXT("Starting render loop (max {} frames, dispatch {}x{})..."), MaxFrames, Context.DispatchX, Context.DispatchY);
		FTimer Timer;
		int	   TestFrameCount = 0;

		while (TestFrameCount < MaxFrames)
		{
			// Begin frame
			HLVM_ENSURE(Context.DeviceManager->BeginFrame());

			// Get current framebuffer
			nvrhi::IFramebuffer* Framebuffer = Context.DeviceManager->GetCurrentFramebuffer();
			if (!Framebuffer)
			{
				HLVM_LOG(LogTest, err, TXT("Failed to get current framebuffer"));
				break;
			}

			// Update camera look-at
			Context.Camera->SetRotation(FVec3(glm::radians(-45.0f), glm::radians(45.0f), 0.0f));
			Context.Camera->UpdateWorldTransform();

			// Get matrices
			FMat4 Model = glm::rotate(FMat4(1.0f), static_cast<float>(Timer.MarkSec()) * glm::radians(45.0f), FVec3(0.0f, 1.0f, 0.0f));
			FMat4 ModelRot = glm::rotate(FMat4(1.0f), static_cast<float>(Timer.MarkSec()) * glm::radians(30.0f), FVec3(1.0f, 0.0f, 0.0f));
			Model = Model * ModelRot;
			FMat4 View = Context.Camera->GetViewMatrix();
			FMat4 Projection = Context.Camera->GetProjectionMatrix();
			FMat4 ViewProj = Projection * View;

			// Create new command list for this frame
			Context.NvrhiCommandList = Context.NvrhiDevice->createCommandList();
			Context.NvrhiCommandList->open();

			// =================================================================
			// PASS 1: GBuffer
			// =================================================================
			{
				// Prepare GBuffer uniforms
				FGBufferViewConstants ViewConstants;
				memcpy(ViewConstants.ViewProj, glm::value_ptr(ViewProj), sizeof(float) * 16);
				memcpy(ViewConstants.Model, glm::value_ptr(Model), sizeof(float) * 16);

				FGBufferMaterialConstants MaterialConstants;
				float					  albedo[4] = { 0.8f, 0.2f, 0.2f, 1.0f }; // Red cube
				memcpy(MaterialConstants.AlbedoColor, albedo, sizeof(float) * 4);
				MaterialConstants.SpecularIntensity = 0.5f;
				MaterialConstants.Roughness = 0.3f;

				// Create GBuffer constant buffers
				nvrhi::BufferHandle ViewCB = Context.NvrhiDevice->createBuffer(
					nvrhi::BufferDesc()
						.setByteSize(sizeof(FGBufferViewConstants))
						.setIsConstantBuffer(true)
						.setInitialState(nvrhi::ResourceStates::ConstantBuffer)
						.setKeepInitialState(true));
				Context.NvrhiCommandList->writeBuffer(ViewCB, &ViewConstants, sizeof(ViewConstants), 0);

				nvrhi::BufferHandle MaterialCB = Context.NvrhiDevice->createBuffer(
					nvrhi::BufferDesc()
						.setByteSize(sizeof(FGBufferMaterialConstants))
						.setIsConstantBuffer(true)
						.setInitialState(nvrhi::ResourceStates::ConstantBuffer)
						.setKeepInitialState(true));
				Context.NvrhiCommandList->writeBuffer(MaterialCB, &MaterialConstants, sizeof(MaterialConstants), 0);

				// Create GBuffer binding set
				nvrhi::BindingSetDesc SetDesc;
				SetDesc.bindings = {
					nvrhi::BindingSetItem::ConstantBuffer(258, ViewCB),
					nvrhi::BindingSetItem::ConstantBuffer(256, MaterialCB)
				};
				nvrhi::BindingSetHandle GBufferBindingSet = Context.NvrhiDevice->createBindingSet(SetDesc, Context.GBufferBindingLayout);

				// Clear GBuffer (4 color MRTs + depth)
				nvrhi::Color clearBlack(0.0f, 0.0f, 0.0f, 0.0f);
				nvrhi::utils::ClearColorAttachment(Context.NvrhiCommandList, Context.GBufferFramebuffer, 0, clearBlack);
				nvrhi::utils::ClearColorAttachment(Context.NvrhiCommandList, Context.GBufferFramebuffer, 1, clearBlack);
				nvrhi::utils::ClearColorAttachment(Context.NvrhiCommandList, Context.GBufferFramebuffer, 2, clearBlack);
				nvrhi::utils::ClearColorAttachment(Context.NvrhiCommandList, Context.GBufferFramebuffer, 3, clearBlack);
				nvrhi::utils::ClearDepthStencilAttachment(Context.NvrhiCommandList, Context.GBufferFramebuffer, 1.0f, 0u);

				// Draw cube to GBuffer
				{
					nvrhi::GraphicsState State;
					State.setPipeline(Context.GBufferPipeline);
					State.setFramebuffer(Context.GBufferFramebuffer);
					State.addBindingSet(GBufferBindingSet);

					nvrhi::VertexBufferBinding VBBinding;
					VBBinding.setBuffer(Context.VertexBuffer->GetBufferHandle().Get());
					VBBinding.setSlot(0);
					VBBinding.setOffset(0);
					State.addVertexBuffer(VBBinding);

					nvrhi::IndexBufferBinding IndexBinding;
					IndexBinding.setBuffer(Context.IndexBuffer->GetBufferHandle().Get());
					IndexBinding.setOffset(0);
					IndexBinding.setFormat(nvrhi::Format::R32_UINT);
					State.setIndexBuffer(IndexBinding);

					nvrhi::Viewport Viewport(0, float(WIDTH), 0, float(HEIGHT), 0.0f, 1.0f);
					State.viewport.addViewportAndScissorRect(Viewport);

					Context.NvrhiCommandList->setGraphicsState(State);

					nvrhi::DrawArguments DrawArgs;
					DrawArgs.setVertexCount(36);
					Context.NvrhiCommandList->drawIndexed(DrawArgs);
				}
			}

			// =================================================================
			// PASS 2: Lighting (Compute shader)
			// =================================================================
			{
				// Prepare lighting uniforms
				FLightingConstants LightConstants;
				// Light from camera direction (normalized: camera at 2,2,2 so direction is -2,-2,-2 normalized)
				float lightDir[3] = { -0.577f, -0.577f, -0.577f }; // Light pointing from camera towards scene
				memcpy(LightConstants.LightDir, lightDir, sizeof(float) * 3);
				LightConstants.LightIntensity = 1.5f; // Slightly brighter
				float cameraPos[3] = { 2.0f, 2.0f, 2.0f };
				memcpy(LightConstants.CameraPos, cameraPos, sizeof(float) * 3);
				LightConstants.ShadowHardness = 16.0f;
				float ambient[3] = { 0.15f, 0.15f, 0.15f }; // Increased ambient
				memcpy(LightConstants.AmbientColor, ambient, sizeof(float) * 3);

				// Transition GBuffer textures from RenderTarget to ShaderResource
				Context.NvrhiCommandList->setTextureState(Context.PositionTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
				Context.NvrhiCommandList->setTextureState(Context.AlbedoTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
				Context.NvrhiCommandList->setTextureState(Context.NormalTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
				Context.NvrhiCommandList->setTextureState(Context.EmissiveTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
				Context.NvrhiCommandList->setTextureState(Context.HDRTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);

				// Create lighting constant buffer
				nvrhi::BufferHandle LightCB = Context.NvrhiDevice->createBuffer(
					nvrhi::BufferDesc()
						.setByteSize(sizeof(FLightingConstants))
						.setIsConstantBuffer(true)
						.setInitialState(nvrhi::ResourceStates::ConstantBuffer)
						.setKeepInitialState(true));
				Context.NvrhiCommandList->writeBuffer(LightCB, &LightConstants, sizeof(LightConstants), 0);

				// Create lighting binding set
				nvrhi::BindingSetDesc SetDesc;
				SetDesc.bindings = {
					nvrhi::BindingSetItem::ConstantBuffer(256, LightCB),
					nvrhi::BindingSetItem::Texture_SRV(0, Context.PositionTexture),
					nvrhi::BindingSetItem::Texture_SRV(1, Context.AlbedoTexture),
					nvrhi::BindingSetItem::Texture_SRV(2, Context.NormalTexture),
					nvrhi::BindingSetItem::Texture_SRV(3, Context.EmissiveTexture),
					nvrhi::BindingSetItem::Texture_UAV(384, Context.HDRTexture)
				};
				nvrhi::BindingSetHandle LightingBindingSet = Context.NvrhiDevice->createBindingSet(SetDesc, Context.LightingBindingLayout);

				// Dispatch 8x8 thread groups
				HLVM_LOG(LogTest, debug, TXT("Dispatching lighting compute shader {}x{}"), Context.DispatchX, Context.DispatchY);
				nvrhi::ComputeState State;
				State.setPipeline(Context.LightingPipeline);
				State.addBindingSet(LightingBindingSet);
				Context.NvrhiCommandList->setComputeState(State);

				Context.NvrhiCommandList->dispatch(Context.DispatchX, Context.DispatchY, 1);

				// Transition GBuffer textures back to RenderTarget for next frame
				Context.NvrhiCommandList->setTextureState(Context.PositionTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::RenderTarget);
				Context.NvrhiCommandList->setTextureState(Context.AlbedoTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::RenderTarget);
				Context.NvrhiCommandList->setTextureState(Context.NormalTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::RenderTarget);
				Context.NvrhiCommandList->setTextureState(Context.EmissiveTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::RenderTarget);
				Context.NvrhiCommandList->setTextureState(Context.HDRTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
			}

			// =================================================================
			// PASS 3: Blit to swapchain (using FCommonRenderPasses::BlitTexture)
			// Cycles through: Lighting → Albedo → Normal → Depth → Specular → Emissive
			// =================================================================
			{
				// Update display mode timer and cycle modes
				g_DisplayModeTimer += 0.016f; // Approximate delta time (~60fps)
				if (g_DisplayModeTimer >= g_DisplayModeInterval)
				{
					g_DisplayModeTimer = 0.0f;
					switch (g_DisplayMode)
					{
						case EGBufferDisplayMode::Lighting:
							g_DisplayMode = EGBufferDisplayMode::Albedo;
							HLVM_LOG(LogTest, info, TXT("Display mode: Albedo"));
							break;
						case EGBufferDisplayMode::Albedo:
							g_DisplayMode = EGBufferDisplayMode::Normal;
							HLVM_LOG(LogTest, info, TXT("Display mode: Normal"));
							break;
						case EGBufferDisplayMode::Normal:
							g_DisplayMode = EGBufferDisplayMode::Depth;
							HLVM_LOG(LogTest, info, TXT("Display mode: Depth"));
							break;
						case EGBufferDisplayMode::Depth:
							g_DisplayMode = EGBufferDisplayMode::Specular;
							HLVM_LOG(LogTest, info, TXT("Display mode: Specular"));
							break;
						case EGBufferDisplayMode::Specular:
							g_DisplayMode = EGBufferDisplayMode::Emissive;
							HLVM_LOG(LogTest, info, TXT("Display mode: Emissive"));
							break;
						case EGBufferDisplayMode::Emissive:
							g_DisplayMode = EGBufferDisplayMode::Lighting;
							HLVM_LOG(LogTest, info, TXT("Display mode: Lighting (Full Deferred)"));
							break;
					}
				}

				// Clear swapchain framebuffer
				nvrhi::Color clearColor(0.05f, 0.05f, 0.1f, 1.0f);
				nvrhi::utils::ClearColorAttachment(Context.NvrhiCommandList, Framebuffer, 0, clearColor);

				// Select texture to blit based on display mode
				nvrhi::TextureHandle TextureToBlit;
				FCommonRenderPasses::BlitParameters BlitParams;
				BlitParams.Mode = FCommonRenderPasses::BlitParameters::EBlitMode::Normal;

				switch (g_DisplayMode)
				{
					case EGBufferDisplayMode::Lighting:
						TextureToBlit = Context.HDRTexture;
						break;
					case EGBufferDisplayMode::Albedo:
						TextureToBlit = Context.AlbedoTexture;
						break;
					case EGBufferDisplayMode::Normal:
						TextureToBlit = Context.NormalTexture;
						break;
					case EGBufferDisplayMode::Depth:
						// Use EmissiveTexture which contains packed depth in alpha
						// Note: D32 DepthTexture cannot be read as color
						TextureToBlit = Context.EmissiveTexture;
						BlitParams.Mode = FCommonRenderPasses::BlitParameters::EBlitMode::Depth;
						break;
					case EGBufferDisplayMode::Specular:
						// Show albedo texture - specular is in alpha channel
						TextureToBlit = Context.AlbedoTexture;
						break;
					case EGBufferDisplayMode::Emissive:
						TextureToBlit = Context.EmissiveTexture;
						break;
				}

				// Blit selected texture to swapchain
				FCommonRenderPasses::BlitTexture(
					Context.NvrhiCommandList,
					Framebuffer,
					TextureToBlit,
					&Context.BindingCache,
					WIDTH,
					HEIGHT,
					BlitParams);
			}

			Context.NvrhiCommandList->close();
			Context.NvrhiDevice->executeCommandList(Context.NvrhiCommandList);

			// End frame and present
			Context.DeviceManager->EndFrame();
			Context.DeviceManager->Present();

			// Wait for GPU
			Context.NvrhiDevice->waitForIdle();

			TestFrameCount++;

			// Log progress every 100 frames
			if (TestFrameCount % 100 == 0)
			{
				HLVM_LOG(LogTest, info, TXT("Rendered {} frames in {:.2f}s"), TestFrameCount, Timer.MarkSec());
			}

			// Auto-close after 1 second or max frames
			if (Timer.MarkSec() > 3.0 || TestFrameCount >= MaxFrames)
			{
				break;
			}
		}

		// Wait for GPU to finish ALL pending command lists before cleanup
		Context.NvrhiDevice->waitForIdle();

		CleanupResources(Context);
		HLVM_LOG(LogTest, info, TXT("Test completed successfully! Rendered {} frames in {:.2f} seconds"), TestFrameCount, Timer.MarkSec());
		return true;
	}
	catch (const exception& e)
	{
		CleanupResources(Context);
		HLVM_LOG(LogTest, critical, TXT("Test failed: {}"), TO_TCHAR_CSTR(e.what()));
		return false;
	}
	catch (...)
	{
		CleanupResources(Context);
		HLVM_LOG(LogTest, critical, TXT("Unknown fatal error occurred"));
		return false;
	}
}

	#pragma clang diagnostic pop
#endif // HLVM_VULKAN_RENDERER
