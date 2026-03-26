/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 *
 * DeviceManagerVk Integration Test
 *
 * This test demonstrates using the DeviceManagerVk class to initialize Vulkan
 * and render a triangle using NVRHI abstractions. Unlike raw Vulkan-HPP tests,
 * this test uses our DeviceManager abstraction for cleaner, more maintainable code.
 */

#include "Test.h"
#include "Renderer/DeviceManager.h"
#include "Renderer/Window/WindowDefinition.h"
#include <nvrhi/utils.h>

DECLARE_LOG_CATEGORY(LogTest)

#if HLVM_VULKAN_RENDERER
	#include "Renderer/Window/GLFW3/GLFW3VulkanWindow.h"
	#include "Renderer/RHI/Vulkan/VulkanDefinition.h"
	#include "Renderer/RHI/RHICommon.h"
	#include "Renderer/RHI/Object/Texture.h"
	#include "Renderer/RHI/Object/Frambuffer.h"
	#include "Renderer/RHI/Object/Buffer.h"

	#if 1 // Test DeviceManagerVk with NVRHI triangle rendering
		#pragma clang diagnostic push
		#pragma clang diagnostic ignored "-Wdocumentation"
		#pragma clang diagnostic ignored "-Wzero-as-null-pointer-constant"
		#pragma clang diagnostic ignored "-Wold-style-cast"
		#pragma clang diagnostic ignored "-Wextra-semi-stmt"
		#pragma clang diagnostic ignored "-Wmissing-noreturn"
		#pragma clang diagnostic ignored "-Wcast-function-type-strict"
		#pragma clang diagnostic ignored "-Wunused-parameter"
		#pragma clang diagnostic ignored "-Wshadow"
		#pragma clang diagnostic ignored "-Wmissing-braces"
		#pragma clang diagnostic ignored "-Wsign-conversion"
		#pragma clang diagnostic ignored "-Wunreachable-code"

using namespace std;

// =============================================================================
// CONFIGURATION
// =============================================================================

	#define USING_INDEX 0

const uint32_t	   WIDTH = 800;
const uint32_t	   HEIGHT = 600;
static const char* WINDOW_TITLE = "DeviceManagerVk Integration Test";

// =============================================================================
// TEST STRUCTURE
// =============================================================================

struct FDeviceManagerVkTestContext
{
	// DeviceManager
	TUniquePtr<FDeviceManager> DeviceManager;

	// NVRHI device (obtained from DeviceManager)
	nvrhi::IDevice* NvrhiDevice = nullptr;

	// Command list
	nvrhi::CommandListHandle NvrhiCommandList;

	// RHI Objects
	TUniquePtr<FTexture>	  ColorTexture;
	TUniquePtr<FTexture>	  DepthTexture;
	TUniquePtr<FFramebuffer>  Framebuffer;
	TUniquePtr<FVertexBuffer> VertexBuffer;
	TUniquePtr<FIndexBuffer>  IndexBuffer;

	// Pipeline
	nvrhi::ShaderHandle			  VertexShader;
	nvrhi::ShaderHandle			  FragmentShader;
	nvrhi::InputLayoutHandle	  InputLayout;
	nvrhi::BindingLayoutHandle	  BindingLayout;
	nvrhi::BindingSetHandle		  BindingSet;
	nvrhi::GraphicsPipelineHandle Pipeline;

	// Framebuffers (one per swapchain image)
	vector<nvrhi::FramebufferHandle> Framebuffers;
};

static FDeviceManagerVkTestContext GTestContext;

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

static void CreateDeviceManager(FDeviceManagerVkTestContext& Context)
{
	HLVM_LOG(LogTest, info, TXT("Creating DeviceManager..."));

	// Create window properties
	IWindow::Properties WindowProps;
	WindowProps.Title = WINDOW_TITLE;
	WindowProps.Extent = { WIDTH, HEIGHT };
	WindowProps.Resizable = true;
	WindowProps.VSync = IWindow::EVsync::Off;

	// Create DeviceManager for Vulkan
	Context.DeviceManager = FDeviceManager::Create(nvrhi::GraphicsAPI::VULKAN);
	if (!Context.DeviceManager)
	{
		throw runtime_error("Failed to create DeviceManager");
	}

	// Configure device creation parameters
	FDeviceCreationParameters& DeviceParams = const_cast<FDeviceCreationParameters&>(Context.DeviceManager->GetDeviceParams());
	DeviceParams.BackBufferWidth = WIDTH;
	DeviceParams.BackBufferHeight = HEIGHT;
	DeviceParams.SwapChainBufferCount = 2; // Double buffering
	DeviceParams.VSyncMode = 0;			   // VSync off
	DeviceParams.bEnableDebugRuntime = HLVM_BUILD_DEBUG;
	DeviceParams.bEnableNVRHIValidationLayer = HLVM_BUILD_DEBUG;
	DeviceParams.bEnableRayTracingExtensions = true;

	// Create window, device, and swapchain
	if (!Context.DeviceManager->CreateWindowDeviceAndSwapChain(WindowProps))
	{
		throw runtime_error("Failed to create device and swapchain");
	}

	// Get NVRHI device
	Context.NvrhiDevice = Context.DeviceManager->GetDevice();
	if (!Context.NvrhiDevice)
	{
		throw runtime_error("Failed to get NVRHI device");
	}

	HLVM_LOG(LogTest, info, TXT("DeviceManager created successfully. GPU: {}"), FString(Context.DeviceManager->GetRendererString()));
}

static void CreateNVRHIResources(FDeviceManagerVkTestContext& Context)
{
	HLVM_LOG(LogTest, info, TXT("Creating NVRHI resources..."));

	// Create command list
	nvrhi::CommandListParameters Params = {};
	Params.enableImmediateExecution = false;
	{
		// SRS - set upload buffer size to avoid Vulkan staging buffer fragmentation
		size_t MaxBufferSize = (size_t)(1 * 1024 * 1024);
		Params.setUploadChunkSize(MaxBufferSize);
	}
	Context.NvrhiCommandList = Context.NvrhiDevice->createCommandList(Params);
	if (!Context.NvrhiCommandList)
	{
		throw runtime_error("Failed to create NVRHI command list");
	}
	Context.NvrhiCommandList->open();

	// Note: We don't create our own textures/framebuffers
	// DeviceManager already manages swapchain images and framebuffers
	// Just create vertex and index buffers

	// Create vertex buffer
	struct FVertex
	{
		float Position[3];
		float Color[3];
	};

	FVertex Vertices[] = {
		{ { 0.0f, 0.8f, 0.0f }, { 1.0f, 0.0f, 0.0f } },	  // Top - Red
		{ { -0.8f, -0.8f, 0.0f }, { 0.0f, 1.0f, 0.0f } }, // Bottom Left - Green
		{ { 0.8f, -0.8f, 0.0f }, { 0.0f, 0.0f, 1.0f } }	  // Bottom Right - Blue
	};

	Context.VertexBuffer = TUniquePtr<FDynamicVertexBuffer>(new FDynamicVertexBuffer());
	static_cast<FDynamicVertexBuffer*>(Context.VertexBuffer.get())
		->Initialize(Context.NvrhiDevice, sizeof(Vertices));
	static_cast<FDynamicVertexBuffer*>(Context.VertexBuffer.get())
		->Update(Context.NvrhiCommandList, Vertices, sizeof(Vertices));
	Context.VertexBuffer->SetDebugName(TXT("DynamicTriangleVertexBuffer"));

	#if USING_INDEX
	// Create Index buffer
	uint32_t Indices[] = { 0, 1, 2 };
	Context.IndexBuffer = TUniquePtr<FDynamicIndexBuffer>(new FDynamicIndexBuffer());
	static_cast<FDynamicIndexBuffer*>(Context.IndexBuffer.get())
		->Initialize(Context.NvrhiDevice, sizeof(Indices), nvrhi::Format::R32_UINT);
	static_cast<FDynamicIndexBuffer*>(Context.IndexBuffer.get())
		->Update(Context.NvrhiCommandList, Indices, sizeof(Indices));
	Context.IndexBuffer->SetDebugName(TXT("DynamicTriangleIndexBuffer"));
	#endif

	Context.NvrhiCommandList->close();
	Context.NvrhiDevice->executeCommandList(Context.NvrhiCommandList);

	HLVM_LOG(LogTest, info, TXT("NVRHI resources (buffers) created successfully"));
}

/**
 * Read binary file (SPIR-V shaders)
 */
static vector<char> readFile(const string& filename)
{
	ifstream file(filename, std::ios::ate | std::ios::binary);
	if (!file.is_open())
	{
		throw std::runtime_error("Failed to open file: " + filename);
	}

	size_t		 fileSize = static_cast<size_t>(file.tellg());
	vector<char> buffer(fileSize);

	file.seekg(0);
	file.read(buffer.data(), fileSize);
	file.close();

	return buffer;
}

static void CreateRenderPipeline(FDeviceManagerVkTestContext& Context)
{
	HLVM_LOG(LogTest, info, TXT("Creating render pipeline..."));

	const auto DataDir = FString::Format(TXT("{}/../../Test/{}_Data"), *GExecutablePath, *GExecutableName);
	const bool bDataDirExist = FGenericPlatformFile::Get(EPlatformFileType::Disk)->Exists(DataDir);
	HLVM_ENSURE_F(bDataDirExist, TXT("Data directory not exist {}"), *DataDir);

	// Load SPIR-V shader bytecode
	auto vertShaderCode = readFile(FPath::Combine(DataDir, TXT("vert.spv")).string());
	auto fragShaderCode = readFile(FPath::Combine(DataDir, TXT("frag.spv")).string());

	// Create shaders
	nvrhi::ShaderDesc VertexShaderDesc;
	VertexShaderDesc.setShaderType(nvrhi::ShaderType::Vertex);
	Context.VertexShader = Context.NvrhiDevice->createShader(
		VertexShaderDesc,
		vertShaderCode.data(),
		vertShaderCode.size());
	if (!Context.VertexShader)
	{
		throw std::runtime_error("Failed to create vertex shader");
	}

	nvrhi::ShaderDesc FragmentShaderDesc;
	FragmentShaderDesc.setShaderType(nvrhi::ShaderType::Pixel);
	Context.FragmentShader = Context.NvrhiDevice->createShader(
		FragmentShaderDesc,
		fragShaderCode.data(),
		fragShaderCode.size());
	if (!Context.FragmentShader)
	{
		throw std::runtime_error("Failed to create fragment shader");
	}

	// Create input layout matching our vertex structure (position + color)
	nvrhi::VertexAttributeDesc Attributes[] = {
		nvrhi::VertexAttributeDesc()
			.setName("POSITION")
			.setFormat(nvrhi::Format::RGB32_FLOAT)
			.setOffset(0)
			.setElementStride(sizeof(float) * 6),
		nvrhi::VertexAttributeDesc()
			.setName("COLOR")
			.setFormat(nvrhi::Format::RGB32_FLOAT)
			.setOffset(sizeof(float) * 3)
			.setElementStride(sizeof(float) * 6)
	};

	Context.InputLayout = Context.NvrhiDevice->createInputLayout(
		Attributes, 2, Context.VertexShader);
	if (!Context.InputLayout)
	{
		throw std::runtime_error("Failed to create input layout");
	}

	// Create binding layout (empty for this demo)
	nvrhi::BindingLayoutDesc LayoutDesc;
	LayoutDesc.setVisibility(nvrhi::ShaderType::All);

	Context.BindingLayout = Context.NvrhiDevice->createBindingLayout(LayoutDesc);
	if (!Context.BindingLayout)
	{
		throw std::runtime_error("Failed to create binding layout");
	}

	// Create binding set (empty for this demo)
	nvrhi::BindingSetDesc SetDesc;
	Context.BindingSet = Context.NvrhiDevice->createBindingSet(SetDesc, Context.BindingLayout);

	// Use DeviceManager's framebuffers (already created with proper render pass)
	// No need to create our own - DeviceManager maintains them
	const uint32_t BackBufferCount = Context.DeviceManager->GetBackBufferCount();
	HLVM_LOG(LogTest, info, TXT("Using DeviceManager's {:d} framebuffers"), BackBufferCount);

	// Get framebuffer info from DeviceManager's first framebuffer
	nvrhi::IFramebuffer* FirstFB = Context.DeviceManager->GetFramebuffer(0);
	if (!FirstFB)
	{
		throw std::runtime_error("Failed to get framebuffer 0 from DeviceManager");
	}

	nvrhi::FramebufferInfo FBInfo = FirstFB->getFramebufferInfo();

	// Create graphics pipeline
	nvrhi::GraphicsPipelineDesc PipelineDesc;
	PipelineDesc.setInputLayout(Context.InputLayout)
		.setVertexShader(Context.VertexShader)
		.setPixelShader(Context.FragmentShader)
		.addBindingLayout(Context.BindingLayout);
	PipelineDesc.renderState.rasterState.setCullBack();
	// Caveat : YuHang must explicitly disable depth test/write if fb does not have such attacment
	// Disable depth stencil since framebuffer does not have depth stencil as well
	// Otherwise vk will complain 'The depth-stencil state indicates that depth or stencil operations are used, but the framebuffer info has no depth format.'
	PipelineDesc.renderState.depthStencilState.disableDepthTest().disableDepthWrite().disableStencil();

	Context.Pipeline = Context.NvrhiDevice->createGraphicsPipeline(PipelineDesc, FBInfo);
	if (!Context.Pipeline)
	{
		throw std::runtime_error("Failed to create graphics pipeline");
	}

	HLVM_LOG(LogTest, info, TXT("Render pipeline created successfully"));
}

static void CleanupResources(FDeviceManagerVkTestContext& Context)
{
	HLVM_LOG(LogTest, info, TXT("Cleaning up resources..."));

	// Release NVRHI resources
	// Note: Framebuffers are owned by DeviceManager, don't clear them
	Context.Pipeline = nullptr;
	Context.BindingSet = nullptr;
	Context.BindingLayout = nullptr;
	Context.InputLayout = nullptr;
	Context.FragmentShader = nullptr;
	Context.VertexShader = nullptr;
	Context.IndexBuffer.reset();
	Context.VertexBuffer.reset();

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
}

// =============================================================================
// TEST ENTRY POINT
// =============================================================================

RECORD_BOOL(test_DeviceManagerVk_Integration)
{
		#if (VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1)
	static vk::detail::DynamicLoader dl(VULKAN_LIB);
	PFN_vkGetInstanceProcAddr		 vkGetInstanceProcAddr = dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
	VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
		#endif

	FDeviceManagerVkTestContext& Ctx = GTestContext;

	try
	{
		// Phase 1: Create DeviceManager
		CreateDeviceManager(Ctx);

		// Phase 2: Create NVRHI resources
		CreateNVRHIResources(Ctx);

		// Phase 3: Create render pipeline
		CreateRenderPipeline(Ctx);

		// Phase 4: Render loop
		HLVM_LOG(LogTest, info, TXT("Starting render loop..."));

		FTimer	  Timer;
		int		  FrameCount = 0;
		const int MaxFrames = 120; // Render 100 frames for testing

		while (FrameCount < MaxFrames)
		{
			// Begin frame (acquire swapchain image)
			HLVM_ENSURE(Ctx.DeviceManager->BeginFrame());

			{
				// Get current framebuffer from DeviceManager (already configured with swapchain)
				nvrhi::IFramebuffer* Framebuffer = Ctx.DeviceManager->GetCurrentFramebuffer();
				if (!Framebuffer)
				{
					HLVM_LOG(LogTest, err, TXT("Failed to get current framebuffer"));
					break;
				}

				// Record commands
				Ctx.NvrhiCommandList = Ctx.NvrhiDevice->createCommandList();
				Ctx.NvrhiCommandList->open();

				// Caveat : YuHang NVRHI Clear color attachment for FB must happen
				// before we set GraphicsState that use this FB
				// Clear color attachment using NVRHI utils (this begins render pass internally)
				nvrhi::Color ClearColor(0.1f, 0.1f, 0.1f, 1.0f);
				nvrhi::utils::ClearColorAttachment(Ctx.NvrhiCommandList, Framebuffer, 0, ClearColor);

				// Set graphics state
				nvrhi::GraphicsState State;
				State.setPipeline(Ctx.Pipeline)
					.setFramebuffer(Framebuffer)
					.addBindingSet(Ctx.BindingSet);

				// Set vertex buffer binding
				nvrhi::VertexBufferBinding VBBinding;
				VBBinding.setBuffer(Ctx.VertexBuffer->GetBufferHandle().Get())
					.setSlot(0)
					.setOffset(0);
				State.addVertexBuffer(VBBinding);

	#if USING_INDEX
				// Set index buffer binding
				nvrhi::IndexBufferBinding IBBinding;
				IBBinding.setBuffer(Ctx.IndexBuffer->GetBufferHandle().Get())
					.setFormat(nvrhi::Format::R32_UINT)
					.setOffset(0);
				State.setIndexBuffer(IBBinding);
	#endif
				// Set viewport and scissor
				nvrhi::Viewport Viewport(0, float(WIDTH), 0, float(HEIGHT), 0.0f, 1.0f);
				State.viewport.addViewportAndScissorRect(Viewport);

				Ctx.NvrhiCommandList->setGraphicsState(State);

	#if USING_INDEX == 0
				// Draw (using vertices directly)
				nvrhi::DrawArguments DrawArgs;
				DrawArgs.setVertexCount(3);
				Ctx.NvrhiCommandList->draw(DrawArgs);
	#else
				// Draw (using indices)
				nvrhi::DrawArguments DrawArgs;
				DrawArgs.setVertexCount(3);
				Ctx.NvrhiCommandList->drawIndexed(DrawArgs);
	#endif

				// Execute command list and wait for completion
				// This ensures rendering completes before DeviceManager presents
				Ctx.NvrhiCommandList->close();
				Ctx.NvrhiDevice->executeCommandList(Ctx.NvrhiCommandList);
			}
			// End frame (signal semaphore)
			HLVM_ENSURE(Ctx.DeviceManager->EndFrame());

			// Present (swap backbuffer)
			HLVM_ENSURE(Ctx.DeviceManager->Present());

			// Wait for GPU to finish
			Ctx.NvrhiDevice->waitForIdle();

			FrameCount++;

			// Auto-close after rendering enough frames
			if (Timer.MarkSec() > 2.0)
			{
				break;
			}
		}

		// Wait for GPU to finish
		Ctx.NvrhiDevice->waitForIdle();

		HLVM_LOG(LogTest, info, TXT("Render loop completed. Frames rendered: %d"), FrameCount);

		// Cleanup
		CleanupResources(Ctx);

		HLVM_LOG(LogTest, info, TXT("DeviceManagerVk integration test completed successfully!"));
		return true;
	}
	catch (const exception& e)
	{
		HLVM_LOG(LogTest, critical, TXT("Fatal Error: {}"), FString(e.what()));

		// Cleanup on error
		CleanupResources(Ctx);
		return false;
	}
	catch (...)
	{
		HLVM_LOG(LogTest, critical, TXT("Unknown fatal error occurred"));
		CleanupResources(Ctx);
		return false;
	}
}

		#pragma clang diagnostic pop
	#endif // Test DeviceManagerVk with NVRHI

#endif // HLVM_WINDOW_USE_VULKAN
