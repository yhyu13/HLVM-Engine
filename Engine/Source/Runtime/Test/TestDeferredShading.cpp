/**
 * HLVM-Engine Deferred Shading Test - Simplified Version
 *
 * This is a simplified version that validates basic rendering infrastructure.
 * Based on TestRenderCube patterns.
 *
 * RENDERING PIPELINE:
 * 1. DeviceManager creates Vulkan device, swapchain, and framebuffers
 * 2. SPIR-V shaders loaded (pre-compiled)
 * 3. Vertex buffer created with 8 vertices (position + color)
 * 4. Index buffer created with 36 indices for cube faces
 * 5. Per-frame: update uniforms, draw indexed cube
 *
 * This test validates that the basic rendering infrastructure works
 * before attempting complex deferred shading.
 */

#include "Test.h"
#include "Renderer/DeviceManager.h"
#include "Renderer/Window/WindowDefinition.h"
#include "Renderer/SceneGraph/PerspectiveCameraNode.h"
#include "Renderer/RHI/Object/Buffer.h"
#include "Renderer/Deferred/FGBufferTextures.h"
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

	#pragma clang diagnostic push
	#pragma clang diagnostic ignored "-Wunreachable-code"

using namespace std;

// =============================================================================
// CONFIGURATION
// =============================================================================

const uint32_t	   WIDTH = 800;
const uint32_t	   HEIGHT = 600;
static const char* WINDOW_TITLE = "Deferred Shading Test (Simplified)";

// =============================================================================
// TEST STRUCTURE
// =============================================================================

struct FDeferredShadingTestContext
{
	// DeviceManager
	TUniquePtr<FDeviceManager> DeviceManager;
	nvrhi::IDevice*			   NvrhiDevice = nullptr;

	// Command list
	nvrhi::CommandListHandle NvrhiCommandList;

	// Shaders
	nvrhi::ShaderHandle VS;
	nvrhi::ShaderHandle FS;

	// Buffers
	TUniquePtr<FStaticVertexBuffer> VertexBuffer;
	TUniquePtr<FStaticIndexBuffer>	IndexBuffer;

	// Pipeline
	nvrhi::InputLayoutHandle	  InputLayout;
	nvrhi::BindingLayoutHandle	  BindingLayout;
	nvrhi::GraphicsPipelineHandle Pipeline;

	// GBuffer (for validation - not used in rendering)
	FGBufferTextures GBuffer;

	// Camera
	TUniquePtr<FPerspectiveCameraNode> Camera;
};

// =============================================================================
// GEOMETRY DATA (Vulkan-Cube Style)
// =============================================================================

// Vertex: position (3) + color (3) = 24 bytes
struct FSimpleVertex
{
	float Position[3]; // vec3
	float Color[3];	   // vec3
};

// Cube vertices - 8 vertices with per-face colors
static const FSimpleVertex Vertices[8] = {
	// -Z face (back)
	{ { -0.5f, -0.5f, -0.5f }, { 1.0f, 1.0f, 1.0f } }, // 0
	{ { 0.5f, -0.5f, -0.5f }, { 0.0f, 1.0f, 1.0f } },  // 1
	{ { 0.5f, 0.5f, -0.5f }, { 0.0f, 0.0f, 1.0f } },   // 2
	{ { -0.5f, 0.5f, -0.5f }, { 1.0f, 0.0f, 1.0f } },  // 3

	// +Z face (front)
	{ { -0.5f, -0.5f, 0.5f }, { 1.0f, 1.0f, 0.0f } }, // 4
	{ { 0.5f, -0.5f, 0.5f }, { 0.0f, 1.0f, 0.0f } },  // 5
	{ { 0.5f, 0.5f, 0.5f }, { 0.0f, 0.0f, 0.0f } },	  // 6
	{ { -0.5f, 0.5f, 0.5f }, { 1.0f, 0.0f, 0.0f } },  // 7
};

// Cube indices - 36 indices for 12 triangles (6 faces)
static const uint32_t Indices[36] = {
	// -Z face
	0,
	1,
	3,
	3,
	1,
	2,
	// +X face
	1,
	5,
	2,
	2,
	5,
	6,
	// +Y face
	5,
	4,
	6,
	6,
	4,
	7,
	// -X face
	4,
	0,
	7,
	7,
	0,
	3,
	// +Z face
	3,
	2,
	7,
	7,
	2,
	6,
	// -Y face
	4,
	5,
	0,
	0,
	5,
	1,
};

// =============================================================================
// UNIFORM BUFFER
// =============================================================================

// Matches shader: layout(binding = 0) uniform UniformBufferObject { mat4 model; mat4 view; mat4 proj; }
struct FUniformBufferObject
{
	alignas(16) float Model[4][4];
	alignas(16) float View[4][4];
	alignas(16) float Proj[4][4];
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

static void CleanupResources(FDeferredShadingTestContext& Context)
{
	HLVM_LOG(LogTest, info, TXT("Cleaning up resources..."));

	// Release NVRHI resources
	Context.Pipeline = nullptr;
	Context.InputLayout = nullptr;
	Context.BindingLayout = nullptr;

	Context.IndexBuffer = nullptr;
	Context.VertexBuffer = nullptr;

	Context.VS = nullptr;
	Context.FS = nullptr;

	if (Context.NvrhiCommandList)
	{
		Context.NvrhiCommandList.Reset();
	}
	// Shutdown DeviceManager (releases all Vulkan resources)
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

RECORD_BOOL(TestDeferredShading)
{
	HLVM_LOG(LogTest, info, TXT("Starting Deferred Shading Test (Simplified)..."));

	FDeferredShadingTestContext Context;

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

		// 4. Load shaders from ShaderMake blob files
		HLVM_LOG(LogTest, info, TXT("Loading shaders from blobs..."));
		const auto DataDir = FString::Format(TXT("{}/../../Test/{}_Data"), *GExecutablePath, *GExecutableName);

		// Load vertex shader blob
		HLVM_LOG(LogTest, info, TXT("Loading vertex shader blob..."));
		auto		VSBlobData = ReadBinaryFile(FPath::Combine(DataDir, TXT("blit_vs.sblob")).string());
		const void* VSBinary = nullptr;
		size_t		VSBinarySize = 0;
		if (!ShaderMake::FindPermutationInBlob(VSBlobData.data(), VSBlobData.size(), nullptr, 0, &VSBinary, &VSBinarySize))
		{
			throw runtime_error("Failed to extract vertex shader from blob");
		}
		HLVM_LOG(LogTest, info, TXT("Vertex shader loaded successfully ({} bytes)"), VSBinarySize);

		// Load fragment shader blob
		HLVM_LOG(LogTest, info, TXT("Loading fragment shader blob..."));
		auto		FSBlobData = ReadBinaryFile(FPath::Combine(DataDir, TXT("blit_ps.sblob")).string());
		const void* FSBinary = nullptr;
		size_t		FSBinarySize = 0;
		if (!ShaderMake::FindPermutationInBlob(FSBlobData.data(), FSBlobData.size(), nullptr, 0, &FSBinary, &FSBinarySize))
		{
			throw runtime_error("Failed to extract fragment shader from blob");
		}
		HLVM_LOG(LogTest, info, TXT("Fragment shader loaded successfully ({} bytes)"), FSBinarySize);

		nvrhi::ShaderDesc VSDesc;
		VSDesc.setShaderType(nvrhi::ShaderType::Vertex);
		Context.VS = Context.NvrhiDevice->createShader(VSDesc, VSBinary, VSBinarySize);

		nvrhi::ShaderDesc FSDesc;
		FSDesc.setShaderType(nvrhi::ShaderType::Pixel);
		Context.FS = Context.NvrhiDevice->createShader(FSDesc, FSBinary, FSBinarySize);

		// 5. Create vertex buffer
		HLVM_LOG(LogTest, info, TXT("Creating vertex buffer..."));
		Context.VertexBuffer = TUniquePtr<FStaticVertexBuffer>(new FStaticVertexBuffer());
		Context.VertexBuffer->Initialize(Context.NvrhiCommandList.Get(), Context.NvrhiDevice, Vertices, sizeof(Vertices));

		// 6. Create index buffer
		HLVM_LOG(LogTest, info, TXT("Creating index buffer..."));
		Context.IndexBuffer = TUniquePtr<FStaticIndexBuffer>(new FStaticIndexBuffer());
		Context.IndexBuffer->Initialize(Context.NvrhiCommandList.Get(), Context.NvrhiDevice, Indices, sizeof(Indices), nvrhi::Format::R32_UINT);

		Context.NvrhiCommandList->close();
		Context.NvrhiDevice->executeCommandList(Context.NvrhiCommandList);

		// 7. Create input layout
		HLVM_LOG(LogTest, info, TXT("Creating input layout..."));
		nvrhi::VertexAttributeDesc Attrs[2];
		// Location 0: position (vec3)
		Attrs[0].setName("inPosition").setFormat(nvrhi::Format::RGB32_FLOAT).setOffset(0).setElementStride(sizeof(FSimpleVertex));
		// Location 1: color (vec3)
		Attrs[1].setName("inColor").setFormat(nvrhi::Format::RGB32_FLOAT).setOffset(12).setElementStride(sizeof(FSimpleVertex));

		Context.InputLayout = Context.NvrhiDevice->createInputLayout(Attrs, 2, Context.VS);

		// 8. Create binding layout
		HLVM_LOG(LogTest, info, TXT("Creating binding layout..."));
		nvrhi::BindingLayoutDesc LayoutDesc;
		LayoutDesc.setVisibility(nvrhi::ShaderType::All);

		// Set binding offsets to 0 to match GLSL binding 0
		nvrhi::VulkanBindingOffsets offsets;
		offsets.setConstantBufferOffset(0).setShaderResourceOffset(0).setSamplerOffset(0).setUnorderedAccessViewOffset(0);
		LayoutDesc.setBindingOffsets(offsets);

		// One constant buffer at slot 0 (matches SPIR-V binding 256 from bRegShift 256)
		LayoutDesc.addItem(nvrhi::BindingLayoutItem::ConstantBuffer(256));

		Context.BindingLayout = Context.NvrhiDevice->createBindingLayout(LayoutDesc);

		// 9. Get framebuffer info
		nvrhi::IFramebuffer* FirstFB = Context.DeviceManager->GetFramebuffer(0);
		if (!FirstFB)
		{
			throw runtime_error("Failed to get framebuffer 0 from DeviceManager");
		}
		nvrhi::FramebufferInfo FBInfo = FirstFB->getFramebufferInfo();

		// 10. Create graphics pipeline
		HLVM_LOG(LogTest, info, TXT("Creating graphics pipeline..."));
		{
			nvrhi::GraphicsPipelineDesc PipelineDesc;
			PipelineDesc.setVertexShader(Context.VS);
			PipelineDesc.setPixelShader(Context.FS);
			PipelineDesc.setInputLayout(Context.InputLayout);
			PipelineDesc.addBindingLayout(Context.BindingLayout);
			PipelineDesc.setPrimType(nvrhi::PrimitiveType::TriangleList);
			PipelineDesc.renderState.setRasterState(nvrhi::RasterState().setCullBack());
			PipelineDesc.renderState.depthStencilState.setDepthTestEnable(true).setDepthWriteEnable(true).setDepthFunc(nvrhi::ComparisonFunc::Less);

			Context.Pipeline = Context.NvrhiDevice->createGraphicsPipeline(PipelineDesc, FBInfo);
		}

		// NOTE: GBuffer validation requires separate command list to avoid validation errors
		// Skipping for simplicity - GBuffer is tested in other tests

		// 12. Create camera
		HLVM_LOG(LogTest, info, TXT("Creating camera..."));
		Context.Camera = TUniquePtr<FPerspectiveCameraNode>(new FPerspectiveCameraNode());
		Context.Camera->SetPosition(FVec3(2.0f, 2.0f, 2.0f));
		Context.Camera->SetRotation(FVec3(0.0f, 0.0f, 0.0f));
		Context.Camera->SetFovY(glm::radians(45.0f));
		Context.Camera->SetAspectRatio(static_cast<float>(WIDTH) / static_cast<float>(HEIGHT));
		Context.Camera->SetNearPlane(0.1f);
		Context.Camera->SetFarPlane(100.0f); // Increased from 10.0f for safety
		Context.Camera->UpdateWorldTransform();
		HLVM_LOG(LogTest, info, TXT("Camera created at (2,2,2) looking at origin, far plane=100.0f"));

		// 13. Render loop
		constexpr int MaxFrames = 600;
		HLVM_LOG(LogTest, info, TXT("Starting render loop (max {} frames)..."), MaxFrames);
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
			FMat4 Model = glm::rotate(FMat4(1.0f), static_cast<float>(Timer.MarkSec()) * glm::radians(90.0f), FVec3(0.0f, 0.0f, 1.0f));
			FMat4 View = Context.Camera->GetViewMatrix();
			FMat4 Projection = Context.Camera->GetProjectionMatrix();

			// Create command list for this frame
			Context.NvrhiCommandList = Context.NvrhiDevice->createCommandList();
			Context.NvrhiCommandList->open();

			// Prepare uniform data
			FUniformBufferObject UBO;
			memcpy(UBO.Model, glm::value_ptr(Model), sizeof(float) * 16);
			memcpy(UBO.View, glm::value_ptr(View), sizeof(float) * 16);
			memcpy(UBO.Proj, glm::value_ptr(Projection), sizeof(float) * 16);

			// Create constant buffer
			nvrhi::BufferHandle UniformBuffer = Context.NvrhiDevice->createBuffer(
				nvrhi::BufferDesc()
					.setByteSize(sizeof(FUniformBufferObject))
					.setIsConstantBuffer(true)
					.setInitialState(nvrhi::ResourceStates::ConstantBuffer)
					.setKeepInitialState(true));
			Context.NvrhiCommandList->writeBuffer(UniformBuffer, &UBO, sizeof(FUniformBufferObject), 0);

			// Create binding set
			nvrhi::BindingSetDesc SetDesc;
			SetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(256, UniformBuffer));
			nvrhi::BindingSetHandle BindingSet = Context.NvrhiDevice->createBindingSet(SetDesc, Context.BindingLayout);

			// Clear color (dark gray background)
			nvrhi::Color ClearColor(0.1f, 0.1f, 0.1f, 1.0f);
			nvrhi::utils::ClearColorAttachment(Context.NvrhiCommandList, Framebuffer, 0, ClearColor);
			// Clear depth to 1.0 (infinity)
			nvrhi::utils::ClearDepthStencilAttachment(Context.NvrhiCommandList, Framebuffer, 1.0f, 0u);

			// Draw cube
			{
				nvrhi::GraphicsState State;
				State.setPipeline(Context.Pipeline);
				State.setFramebuffer(Framebuffer);
				State.addBindingSet(BindingSet);

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
				DrawArgs.setVertexCount(36); // 36 indices for cube
				Context.NvrhiCommandList->drawIndexed(DrawArgs);
			}

			Context.NvrhiCommandList->close();
			Context.NvrhiDevice->executeCommandList(Context.NvrhiCommandList);

			// End frame and present
			Context.DeviceManager->EndFrame();
			Context.DeviceManager->Present();

			// Wait for GPU to finish
			Context.NvrhiDevice->waitForIdle();

			TestFrameCount++;

			// Log progress every 100 frames
			if (TestFrameCount % 100 == 0)
			{
				HLVM_LOG(LogTest, info, TXT("Rendered {} frames in {:.2f}s"), TestFrameCount, Timer.MarkSec());
			}

			// Auto-close after 1 second or max frames
			if (Timer.MarkSec() > 1.0 || TestFrameCount >= MaxFrames)
			{
				break;
			} // close if
		} // close while loop

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
