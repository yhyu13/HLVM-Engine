/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 *
 * Cube on Plane Rendering Test
 *
 * Demonstrates the HLVM Engine rendering pipeline by rendering a Phong-lit
 * cube on a procedurally-textured grid plane using a static perspective camera.
 *
 * RENDERING PIPELINE OVERVIEW:
 * --------------------------
 * 1. DeviceManager creates Vulkan device, swapchain, and framebuffers
 * 2. SPIR-V shaders are loaded (pre-compiled, not runtime compilation)
 * 3. Vertex/index buffers created for cube and plane geometry
 * 4. Binding layout declares 3 constant buffer slots:
 *    - Slot 0: MVP matrix (FMVPUniform)
 *    - Slot 1: Light parameters (FLightUniform)
 *    - Slot 2: Object color (FObjectUniform)
 * 5. Graphics pipelines created for cube (with Phong shading) and plane (with grid)
 * 6. Per-frame: create fresh binding sets, update uniform buffers, draw
 *
 * NVRHI KEY PATTERNS:
 * -----------------
 * - BindingLayoutDesc: declares what resources will be bound (slots 0,1,2)
 * - BindingSetDesc: describes what resources are actually bound at runtime
 * - BufferDesc: must set isConstantBuffer=true, keepInitialState=true
 * - VulkanBindingOffsets: maps NVRHI slots to Vulkan binding points
 *   (default offsets are 256 for constant buffers, must set to 0 to match GLSL)
 * - Depth stencil: disable if framebuffer has no depth attachment
 *
 * SHADER BINDINGS (GLSL):
 * -----------------------
 * - Cube.vert: layout(binding=0) uniform UniformBuffer { mat4 uMVP; }
 * - Cube.frag: layout(binding=1) uniform LightBuffer { ... }, binding=2 ObjectBuffer
 * - Plane.vert: layout(binding=0) uniform UniformBuffer { mat4 uMVP; }
 * - Plane.frag: procedural grid pattern, no bindings needed
 */

#include "Test.h"
#include "Renderer/DeviceManager.h"
#include "Renderer/Window/WindowDefinition.h"
#include "Renderer/SceneGraph/PerspectiveCameraNode.h"
#include "Renderer/RHI/Object/Buffer.h"
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

using namespace std;

// =============================================================================
// CONFIGURATION
// =============================================================================

const uint32_t	   WIDTH = 800;
const uint32_t	   HEIGHT = 600;
static const char* WINDOW_TITLE = "Cube on Plane Test";

// =============================================================================
// TEST STRUCTURE
// =============================================================================

struct FCubeOnPlaneTestContext
{
	// DeviceManager
	TUniquePtr<FDeviceManager> DeviceManager;
	nvrhi::IDevice*			   NvrhiDevice = nullptr;

	// Command list
	nvrhi::CommandListHandle NvrhiCommandList;

	// Shaders
	nvrhi::ShaderHandle CubeVS;
	nvrhi::ShaderHandle CubeFS;
	nvrhi::ShaderHandle PlaneVS;
	nvrhi::ShaderHandle PlaneFS;

	// Buffers
	TUniquePtr<FStaticVertexBuffer> CubeVertexBuffer;
	TUniquePtr<FStaticIndexBuffer>	CubeIndexBuffer;
	TUniquePtr<FStaticVertexBuffer> PlaneVertexBuffer;
	TUniquePtr<FStaticIndexBuffer>	PlaneIndexBuffer;

	// Pipeline
	nvrhi::InputLayoutHandle	  CubeInputLayout;
	nvrhi::InputLayoutHandle	  PlaneInputLayout;
	nvrhi::BindingLayoutHandle	  BindingLayout;
	nvrhi::GraphicsPipelineHandle CubePipeline;
	nvrhi::GraphicsPipelineHandle PlanePipeline;

	// Camera
	TUniquePtr<FPerspectiveCameraNode> Camera;
};

static void CleanupResources(FCubeOnPlaneTestContext& Context)
{
	HLVM_LOG(LogTest, info, TXT("Cleaning up resources..."));

	// Release NVRHI resources
	// Note: Framebuffers are owned by DeviceManager, don't clear them
	Context.CubePipeline = nullptr;
	Context.PlanePipeline = nullptr;
	Context.BindingLayout = nullptr;
	Context.PlaneInputLayout = nullptr;
	Context.CubeInputLayout = nullptr;

	Context.PlaneIndexBuffer = nullptr;
	Context.CubeIndexBuffer = nullptr;
	Context.CubeVertexBuffer = nullptr;
	Context.PlaneVertexBuffer = nullptr;

	Context.CubeVS = nullptr;
	Context.PlaneVS = nullptr;
	Context.CubeFS = nullptr;
	Context.PlaneFS = nullptr;

	if (Context.NvrhiCommandList)
	{
		Context.NvrhiCommandList.Reset();
	}

	// Shutdown DeviceManager (releases all Vulkan resources)
	Context.DeviceManager = nullptr;
	Context.NvrhiDevice = nullptr;

	Context.Camera = nullptr;
}

// =============================================================================
// GEOMETRY DATA
// =============================================================================

// Cube vertices: position (3) + normal (3) + UV (2) = 32 bytes
struct FCubeVertex
{
	float Position[3];
	float Normal[3];
	float UV[2];
};

static const FCubeVertex CubeVertices[24] = {
	// Front face
	{ { -0.5f, -0.5f, 0.5f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f } },
	{ { 0.5f, -0.5f, 0.5f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f } },
	{ { 0.5f, 0.5f, 0.5f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f } },
	{ { -0.5f, 0.5f, 0.5f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f } },
	// Back face
	{ { -0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f } },
	{ { 0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f } },
	{ { 0.5f, 0.5f, -0.5f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 1.0f } },
	{ { -0.5f, 0.5f, -0.5f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 1.0f } },
	// Top face
	{ { -0.5f, 0.5f, -0.5f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f } },
	{ { 0.5f, 0.5f, -0.5f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f } },
	{ { 0.5f, 0.5f, 0.5f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 1.0f } },
	{ { -0.5f, 0.5f, 0.5f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f } },
	// Bottom face
	{ { -0.5f, -0.5f, -0.5f }, { 0.0f, -1.0f, 0.0f }, { 0.0f, 0.0f } },
	{ { 0.5f, -0.5f, -0.5f }, { 0.0f, -1.0f, 0.0f }, { 1.0f, 0.0f } },
	{ { 0.5f, -0.5f, 0.5f }, { 0.0f, -1.0f, 0.0f }, { 1.0f, 1.0f } },
	{ { -0.5f, -0.5f, 0.5f }, { 0.0f, -1.0f, 0.0f }, { 0.0f, 1.0f } },
	// Right face
	{ { 0.5f, -0.5f, -0.5f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f } },
	{ { 0.5f, 0.5f, -0.5f }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f } },
	{ { 0.5f, 0.5f, 0.5f }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f } },
	{ { 0.5f, -0.5f, 0.5f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f } },
	// Left face
	{ { -0.5f, -0.5f, -0.5f }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f } },
	{ { -0.5f, 0.5f, -0.5f }, { -1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f } },
	{ { -0.5f, 0.5f, 0.5f }, { -1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f } },
	{ { -0.5f, -0.5f, 0.5f }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f } },
};

static const uint32_t CubeIndices[36] = {
	// Front face (z=+0.5) - CCW from outside
	0,
	1,
	2,
	2,
	3,
	0,
	// Back face (z=-0.5) - CCW from outside
	4,
	5,
	6,
	6,
	7,
	4,
	// Top face (y=+0.5) - CCW from outside
	8,
	9,
	10,
	10,
	11,
	8,
	// Bottom face (y=-0.5) - CCW from outside
	12,
	13,
	14,
	14,
	15,
	12,
	// Right face (x=+0.5) - CCW from outside
	16,
	17,
	18,
	18,
	19,
	16,
	// Left face (x=-0.5) - CCW from outside
	20,
	21,
	22,
	22,
	23,
	20,
};

// Plane vertices: position (3) + UV (2) = 20 bytes
struct FPlaneVertex
{
	float Position[3];
	float UV[2];
};

static const FPlaneVertex PlaneVertices[4] = {
	{ { -5.0f, -5.0f, 0.0f }, { 0.0f, 0.0f } },
	{ { 5.0f, -5.0f, 0.0f }, { 1.0f, 0.0f } },
	{ { 5.0f, 5.0f, 0.0f }, { 1.0f, 1.0f } },
	{ { -5.0f, 5.0f, 0.0f }, { 0.0f, 1.0f } },
};

static const uint32_t PlaneIndices[6] = {
	0,
	1,
	2,
	2,
	3,
	0,
};

// Uniform buffer structures
struct alignas(16) FMVPUniform
{
	float MVP[4][4];
};

struct alignas(16) FLightUniform
{
	float LightPosition[3];
	float _pad0;
	float LightColor[3];
	float _pad1;
};

struct alignas(16) FObjectUniform
{
	float ObjectColor[3];
	float _pad;
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

// =============================================================================
// TEST IMPLEMENTATION
// =============================================================================

RECORD_BOOL(test_RenderCubeOnPlane)
{
	HLVM_LOG(LogTest, info, TXT("Starting Cube on Plane Test..."));

	FCubeOnPlaneTestContext Context;

	try
	{
		// 1. Create window properties
		HLVM_LOG(LogTest, info, TXT("Creating window..."));
		IWindow::Properties WindowProps;
		WindowProps.Title = WINDOW_TITLE;
		WindowProps.Extent = { WIDTH, HEIGHT };
		WindowProps.Resizable = true;
		WindowProps.VSync = IWindow::EVsync::Off;

		// 2. Create DeviceManager for Vulkan
		HLVM_LOG(LogTest, info, TXT("Creating DeviceManager..."));
		Context.DeviceManager = FDeviceManager::Create(nvrhi::GraphicsAPI::VULKAN);
		if (!Context.DeviceManager)
		{
			throw runtime_error("Failed to create DeviceManager");
		}

		// Configure device creation parameters
		FDeviceCreationParameters& DeviceParams = const_cast<FDeviceCreationParameters&>(Context.DeviceManager->GetDeviceParams());
		DeviceParams.BackBufferWidth = WIDTH;
		DeviceParams.BackBufferHeight = HEIGHT;
		DeviceParams.SwapChainBufferCount = 2;
		DeviceParams.VSyncMode = 0;
		DeviceParams.bEnableDebugRuntime = HLVM_BUILD_DEBUG;
		DeviceParams.bEnableNVRHIValidationLayer = HLVM_BUILD_DEBUG;
		DeviceParams.bEnableRayTracingExtensions = false;

		// Create window, device, and swapchain
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

		// 3. Create command list
		HLVM_LOG(LogTest, info, TXT("Creating command list..."));
		nvrhi::CommandListParameters Params = {};
		Params.enableImmediateExecution = false;
		Context.NvrhiCommandList = Context.NvrhiDevice->createCommandList(Params);
		Context.NvrhiCommandList->open();

		// 4. Load shaders from SPIR-V files
		HLVM_LOG(LogTest, info, TXT("Loading shaders..."));
		const auto DataDir = FString::Format(TXT("{}/../../Test/{}_Data"), *GExecutablePath, *GExecutableName);

		auto CubeVertCode = ReadBinaryFile(FPath::Combine(DataDir, TXT("Cube.vert.spv")).string());
		auto CubeFragCode = ReadBinaryFile(FPath::Combine(DataDir, TXT("Cube.frag.spv")).string());
		auto PlaneVertCode = ReadBinaryFile(FPath::Combine(DataDir, TXT("Plane.vert.spv")).string());
		auto PlaneFragCode = ReadBinaryFile(FPath::Combine(DataDir, TXT("Plane.frag.spv")).string());

		nvrhi::ShaderDesc CubeVSDesc;
		CubeVSDesc.setShaderType(nvrhi::ShaderType::Vertex);
		Context.CubeVS = Context.NvrhiDevice->createShader(CubeVSDesc, CubeVertCode.data(), CubeVertCode.size());

		nvrhi::ShaderDesc CubeFSDesc;
		CubeFSDesc.setShaderType(nvrhi::ShaderType::Pixel);
		Context.CubeFS = Context.NvrhiDevice->createShader(CubeFSDesc, CubeFragCode.data(), CubeFragCode.size());

		nvrhi::ShaderDesc PlaneVSDesc;
		PlaneVSDesc.setShaderType(nvrhi::ShaderType::Vertex);
		Context.PlaneVS = Context.NvrhiDevice->createShader(PlaneVSDesc, PlaneVertCode.data(), PlaneVertCode.size());

		nvrhi::ShaderDesc PlaneFSDesc;
		PlaneFSDesc.setShaderType(nvrhi::ShaderType::Pixel);
		Context.PlaneFS = Context.NvrhiDevice->createShader(PlaneFSDesc, PlaneFragCode.data(), PlaneFragCode.size());

		// 5. Create vertex/index buffers for cube
		HLVM_LOG(LogTest, info, TXT("Creating cube buffers..."));
		Context.CubeVertexBuffer = TUniquePtr<FStaticVertexBuffer>(new FStaticVertexBuffer());
		Context.CubeVertexBuffer->Initialize(Context.NvrhiCommandList.Get(), Context.NvrhiDevice, CubeVertices, sizeof(CubeVertices));

		Context.CubeIndexBuffer = TUniquePtr<FStaticIndexBuffer>(new FStaticIndexBuffer());
		Context.CubeIndexBuffer->Initialize(Context.NvrhiCommandList.Get(), Context.NvrhiDevice, CubeIndices, sizeof(CubeIndices), nvrhi::Format::R32_UINT);

		// 6. Create vertex/index buffers for plane
		HLVM_LOG(LogTest, info, TXT("Creating plane buffers..."));
		Context.PlaneVertexBuffer = TUniquePtr<FStaticVertexBuffer>(new FStaticVertexBuffer());
		Context.PlaneVertexBuffer->Initialize(Context.NvrhiCommandList.Get(), Context.NvrhiDevice, PlaneVertices, sizeof(PlaneVertices));

		Context.PlaneIndexBuffer = TUniquePtr<FStaticIndexBuffer>(new FStaticIndexBuffer());
		Context.PlaneIndexBuffer->Initialize(Context.NvrhiCommandList.Get(), Context.NvrhiDevice, PlaneIndices, sizeof(PlaneIndices), nvrhi::Format::R32_UINT);

		Context.NvrhiCommandList->close();
		Context.NvrhiDevice->executeCommandList(Context.NvrhiCommandList);

		// 7. Create input layouts
		HLVM_LOG(LogTest, info, TXT("Creating input layouts..."));
		nvrhi::VertexAttributeDesc CubeAttrs[3];
		CubeAttrs[0].setName("POSITION").setFormat(nvrhi::Format::RGB32_FLOAT).setOffset(0).setElementStride(sizeof(FCubeVertex));
		CubeAttrs[1].setName("NORMAL").setFormat(nvrhi::Format::RGB32_FLOAT).setOffset(12).setElementStride(sizeof(FCubeVertex));
		CubeAttrs[2].setName("TEXCOORD").setFormat(nvrhi::Format::RG32_FLOAT).setOffset(24).setElementStride(sizeof(FCubeVertex));

		Context.CubeInputLayout = Context.NvrhiDevice->createInputLayout(CubeAttrs, 3, Context.CubeVS);

		nvrhi::VertexAttributeDesc PlaneAttrs[2];
		PlaneAttrs[0].setName("POSITION").setFormat(nvrhi::Format::RGB32_FLOAT).setOffset(0).setElementStride(sizeof(FPlaneVertex));
		PlaneAttrs[1].setName("TEXCOORD").setFormat(nvrhi::Format::RG32_FLOAT).setOffset(12).setElementStride(sizeof(FPlaneVertex));

		Context.PlaneInputLayout = Context.NvrhiDevice->createInputLayout(PlaneAttrs, 2, Context.PlaneVS);

		// 8. Create binding layout for uniform buffers
		HLVM_LOG(LogTest, info, TXT("Creating binding layout..."));
		nvrhi::BindingLayoutDesc LayoutDesc;
		LayoutDesc.setVisibility(nvrhi::ShaderType::All);
		// Set binding offsets to 0 so slot numbers match GLSL binding numbers (0, 1, 2)
		nvrhi::VulkanBindingOffsets offsets;
		offsets.setConstantBufferOffset(0).setShaderResourceOffset(0).setSamplerOffset(0).setUnorderedAccessViewOffset(0);
		LayoutDesc.setBindingOffsets(offsets);
		// Declare 3 constant buffer bindings: slot 0=MVP, slot 1=Light, slot 2=ObjectColor
		LayoutDesc.addItem(nvrhi::BindingLayoutItem::ConstantBuffer(0));
		LayoutDesc.addItem(nvrhi::BindingLayoutItem::ConstantBuffer(1));
		LayoutDesc.addItem(nvrhi::BindingLayoutItem::ConstantBuffer(2));

		Context.BindingLayout = Context.NvrhiDevice->createBindingLayout(LayoutDesc);

		// Note: Binding sets are created per-frame in the render loop, not here

		// 10. Get framebuffer info for pipeline creation
		nvrhi::IFramebuffer* FirstFB = Context.DeviceManager->GetFramebuffer(0);
		if (!FirstFB)
		{
			throw runtime_error("Failed to get framebuffer 0 from DeviceManager");
		}
		nvrhi::FramebufferInfo FBInfo = FirstFB->getFramebufferInfo();

		// 11. Create graphics pipelines
		HLVM_LOG(LogTest, info, TXT("Creating graphics pipelines..."));

		// Cube pipeline
		{
			// YuHang : pipeline desc need to locally scoped in order to release deps for handles
			// Otherwise it will cause a segfault due to we clean up resources before pipeline is released
			nvrhi::GraphicsPipelineDesc PipelineDesc;
			PipelineDesc.setVertexShader(Context.CubeVS);
			PipelineDesc.setPixelShader(Context.CubeFS);
			PipelineDesc.setInputLayout(Context.CubeInputLayout);
			PipelineDesc.addBindingLayout(Context.BindingLayout);
			PipelineDesc.setPrimType(nvrhi::PrimitiveType::TriangleList);
			PipelineDesc.renderState.setRasterState(nvrhi::RasterState().setCullBack());
			PipelineDesc.renderState.depthStencilState.setDepthTestEnable(true).setDepthWriteEnable(true).setDepthFunc(nvrhi::ComparisonFunc::Less);

			Context.CubePipeline = Context.NvrhiDevice->createGraphicsPipeline(PipelineDesc, FBInfo);
		}

		// Plane pipeline
		{
			// YuHang : pipeline desc need to locally scoped in order to release deps for handles
			// Otherwise it will cause a segfault due to we clean up resources before pipeline is released
			nvrhi::GraphicsPipelineDesc PipelineDesc;
			PipelineDesc.setVertexShader(Context.PlaneVS);
			PipelineDesc.setPixelShader(Context.PlaneFS);
			PipelineDesc.setInputLayout(Context.PlaneInputLayout);
			PipelineDesc.addBindingLayout(Context.BindingLayout);
			PipelineDesc.setPrimType(nvrhi::PrimitiveType::TriangleList);
			PipelineDesc.renderState.setRasterState(nvrhi::RasterState().setCullNone());
			PipelineDesc.renderState.depthStencilState.setDepthTestEnable(true).setDepthWriteEnable(true).setDepthFunc(nvrhi::ComparisonFunc::Less);

			Context.PlanePipeline = Context.NvrhiDevice->createGraphicsPipeline(PipelineDesc, FBInfo);
		}

		// 12. Create camera
		HLVM_LOG(LogTest, info, TXT("Creating camera..."));
		Context.Camera = TUniquePtr<FPerspectiveCameraNode>(new FPerspectiveCameraNode());
		Context.Camera->SetPosition(FVec3(2.0f, 2.0f, 2.0f));								   // Camera position: above and in front
		Context.Camera->SetRotation(FVec3(glm::radians(-45.0f), glm::radians(45.0f), 0.0f)); // Look at origin
		Context.Camera->SetFovY(glm::radians(45.0f));										   // 45 degrees - standard perspective FOV
		Context.Camera->SetAspectRatio(static_cast<float>(WIDTH) / static_cast<float>(HEIGHT));
		Context.Camera->SetNearPlane(0.1f);
		Context.Camera->SetFarPlane(10000.0f);
		Context.Camera->UpdateWorldTransform();

		// 13. Render loop
		HLVM_LOG(LogTest, info, TXT("Starting render loop..."));
		FTimer		  Timer;
		int			  TestFrameCount = 0;
		constexpr int MaxFrames = 600;
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

			// Update MVP matrices
			FMat4 View = Context.Camera->GetViewMatrix();
			FMat4 Projection = Context.Camera->GetProjectionMatrix();
			// Cube rotation angle based on time (360 degrees per 10 seconds)
			float RotationAngle = static_cast<float>(Timer.MarkSec()) * glm::radians(36.0f);
			FMat4 CubeModel = glm::rotate(FMat4(1.0f), RotationAngle, FVec3(0.0f, 1.0f, 0.0f));
			FMat4 CubeMVP = Projection * View * CubeModel;

			// Plane MVP (identity model)
			FMat4 PlaneMVP = Projection * View;

			// Create command list for this frame
			Context.NvrhiCommandList = Context.NvrhiDevice->createCommandList();
			Context.NvrhiCommandList->open();

			// Prepare uniform data
			FMVPUniform CubeMVPUniform;
			memcpy(CubeMVPUniform.MVP, glm::value_ptr(CubeMVP), sizeof(float) * 16);

			FMVPUniform PlaneMVPUniform;
			memcpy(PlaneMVPUniform.MVP, glm::value_ptr(PlaneMVP), sizeof(float) * 16);

			FLightUniform LightUniform;
			LightUniform.LightPosition[0] = 5.0f;
			LightUniform.LightPosition[1] = 5.0f;
			LightUniform.LightPosition[2] = -5.0f;
			LightUniform.LightColor[0] = 1.0f;
			LightUniform.LightColor[1] = 1.0f;
			LightUniform.LightColor[2] = 1.0f;

			FObjectUniform CubeObjectUniform;
			// Blue cube
			CubeObjectUniform.ObjectColor[0] = 0.2f;
			CubeObjectUniform.ObjectColor[1] = 0.2f;
			CubeObjectUniform.ObjectColor[2] = 1.0f;

			FObjectUniform PlaneObjectUniform;
			// White plane
			PlaneObjectUniform.ObjectColor[0] = 1.0f;
			PlaneObjectUniform.ObjectColor[1] = 1.0f;
			PlaneObjectUniform.ObjectColor[2] = 1.0f;

			// Create constant buffers
			nvrhi::BufferHandle CubeMVPBuffer = Context.NvrhiDevice->createBuffer(
				nvrhi::BufferDesc().setByteSize(sizeof(FMVPUniform)).setIsConstantBuffer(true).setInitialState(nvrhi::ResourceStates::ConstantBuffer).setKeepInitialState(true));
			Context.NvrhiCommandList->writeBuffer(CubeMVPBuffer, &CubeMVPUniform, sizeof(FMVPUniform), 0);

			nvrhi::BufferHandle PlaneMVPBuffer = Context.NvrhiDevice->createBuffer(
				nvrhi::BufferDesc().setByteSize(sizeof(FMVPUniform)).setIsConstantBuffer(true).setInitialState(nvrhi::ResourceStates::ConstantBuffer).setKeepInitialState(true));
			Context.NvrhiCommandList->writeBuffer(PlaneMVPBuffer, &PlaneMVPUniform, sizeof(FMVPUniform), 0);

			nvrhi::BufferHandle LightBuffer = Context.NvrhiDevice->createBuffer(
				nvrhi::BufferDesc().setByteSize(sizeof(FLightUniform)).setIsConstantBuffer(true).setInitialState(nvrhi::ResourceStates::ConstantBuffer).setKeepInitialState(true));
			Context.NvrhiCommandList->writeBuffer(LightBuffer, &LightUniform, sizeof(FLightUniform), 0);

			nvrhi::BufferHandle CubeObjectBuffer = Context.NvrhiDevice->createBuffer(
				nvrhi::BufferDesc().setByteSize(sizeof(FObjectUniform)).setIsConstantBuffer(true).setInitialState(nvrhi::ResourceStates::ConstantBuffer).setKeepInitialState(true));
			Context.NvrhiCommandList->writeBuffer(CubeObjectBuffer, &CubeObjectUniform, sizeof(FObjectUniform), 0);

			nvrhi::BufferHandle PlaneObjectBuffer = Context.NvrhiDevice->createBuffer(
				nvrhi::BufferDesc().setByteSize(sizeof(FObjectUniform)).setIsConstantBuffer(true).setInitialState(nvrhi::ResourceStates::ConstantBuffer).setKeepInitialState(true));
			Context.NvrhiCommandList->writeBuffer(PlaneObjectBuffer, &PlaneObjectUniform, sizeof(FObjectUniform), 0);

			// Create binding sets
			nvrhi::BindingSetDesc CubeSetDesc;
			CubeSetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, CubeMVPBuffer));
			CubeSetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(1, LightBuffer));
			CubeSetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(2, CubeObjectBuffer));
			nvrhi::BindingSetHandle CubeBindingSet = Context.NvrhiDevice->createBindingSet(CubeSetDesc, Context.BindingLayout);

			nvrhi::BindingSetDesc PlaneSetDesc;
			PlaneSetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, PlaneMVPBuffer));
			PlaneSetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(1, LightBuffer));
			PlaneSetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(2, PlaneObjectBuffer));
			nvrhi::BindingSetHandle PlaneBindingSet = Context.NvrhiDevice->createBindingSet(PlaneSetDesc, Context.BindingLayout);

			// Clear color attachment
			// Pink background
			nvrhi::Color ClearColor(0.1f, 1.0f, 0.1f, 1.0f);
			nvrhi::utils::ClearColorAttachment(Context.NvrhiCommandList, Framebuffer, 0, ClearColor);
			// Clear depth to 1.0 (infinity) so everything else renders in front
			nvrhi::utils::ClearDepthStencilAttachment(Context.NvrhiCommandList, Framebuffer, 1.0f, 0u);

			// Draw plane FIRST (will be covered by cube)
			{
				nvrhi::GraphicsState State;
				State.setPipeline(Context.PlanePipeline);
				State.setFramebuffer(Framebuffer);
				State.addBindingSet(PlaneBindingSet);

				nvrhi::VertexBufferBinding PlaneVBBinding;
				PlaneVBBinding.setBuffer(Context.PlaneVertexBuffer->GetBufferHandle().Get());
				PlaneVBBinding.setSlot(0);
				PlaneVBBinding.setOffset(0);
				State.addVertexBuffer(PlaneVBBinding);

				nvrhi::IndexBufferBinding PlaneIndexBinding;
				PlaneIndexBinding.setBuffer(Context.PlaneIndexBuffer->GetBufferHandle().Get());
				PlaneIndexBinding.setOffset(0);
				PlaneIndexBinding.setFormat(nvrhi::Format::R32_UINT);
				State.setIndexBuffer(PlaneIndexBinding);

				nvrhi::Viewport Viewport(0, float(WIDTH), 0, float(HEIGHT), 0.0f, 1.0f);
				State.viewport.addViewportAndScissorRect(Viewport);

				Context.NvrhiCommandList->setGraphicsState(State);

				nvrhi::DrawArguments PlaneDrawArgs;
				PlaneDrawArgs.setVertexCount(6);
				Context.NvrhiCommandList->drawIndexed(PlaneDrawArgs);
			}

			// Draw cube SECOND (on top of plane - without depth test, order matters!
			{
				nvrhi::GraphicsState State;
				State.setPipeline(Context.CubePipeline);
				State.setFramebuffer(Framebuffer);
				State.addBindingSet(CubeBindingSet);

				nvrhi::VertexBufferBinding CubeVBBinding;
				CubeVBBinding.setBuffer(Context.CubeVertexBuffer->GetBufferHandle().Get());
				CubeVBBinding.setSlot(0);
				CubeVBBinding.setOffset(0);
				State.addVertexBuffer(CubeVBBinding);

				nvrhi::IndexBufferBinding CubeIndexBinding;
				CubeIndexBinding.setBuffer(Context.CubeIndexBuffer->GetBufferHandle().Get());
				CubeIndexBinding.setOffset(0);
				CubeIndexBinding.setFormat(nvrhi::Format::R32_UINT);
				State.setIndexBuffer(CubeIndexBinding);

				nvrhi::Viewport Viewport(0, float(WIDTH), 0, float(HEIGHT), 0.0f, 1.0f);
				State.viewport.addViewportAndScissorRect(Viewport);

				Context.NvrhiCommandList->setGraphicsState(State);

				nvrhi::DrawArguments CubeDrawArgs;
				CubeDrawArgs.setVertexCount(36);
				Context.NvrhiCommandList->drawIndexed(CubeDrawArgs);
			}

			Context.NvrhiCommandList->close();
			Context.NvrhiDevice->executeCommandList(Context.NvrhiCommandList);

			// End frame and present
			Context.DeviceManager->EndFrame();
			Context.DeviceManager->Present();

			// Wait for GPU to finish
			Context.NvrhiDevice->waitForIdle();

			TestFrameCount++;

			// Auto-close after 10 seconds
			if (Timer.MarkSec() > 1.0)
			{
				break;
			}
		}

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

#endif	   // HLVM_VULKAN_RENDERER
