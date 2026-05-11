/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 *
 * ImGui Vulkan NVRHI Cube Test
 *
 * Demonstrates ImGui integration with the HLVM Engine rendering pipeline:
 * 1. DeviceManager creates Vulkan device, swapchain, and framebuffers
 * 2. FCubeRenderPass renders a rotating cube
 * 3. FUIRenderer renders ImGui demo window
 * 4. Uses render pass system with RunMessageLoop
 */

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
#pragma clang diagnostic ignored "-Wmissing-braces"

#include "Test.h"
#include "Renderer/DeviceManager.h"
#include "Renderer/Window/WindowDefinition.h"
#include "Renderer/RHI/Object/Buffer.h"
#include "Renderer/ImGui/FImgui_Renderer.h"
#include "Renderer/FShaderFactory.h"
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
#endif

using namespace std;

constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;
static const char* WINDOW_TITLE = "ImGui Vulkan Test";

// =============================================================================
// GEOMETRY DATA
// =============================================================================

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
	// Front face
	0,
	1,
	2,
	2,
	3,
	0,
	// Back face
	4,
	5,
	6,
	6,
	7,
	4,
	// Top face
	8,
	9,
	10,
	10,
	11,
	8,
	// Bottom face
	12,
	13,
	14,
	14,
	15,
	12,
	// Right face
	16,
	17,
	18,
	18,
	19,
	16,
	// Left face
	20,
	21,
	22,
	22,
	23,
	20,
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
// FCubeRenderPass - Renders a rotating cube using existing shaders
// =============================================================================

class FCubeRenderPass : public IRenderPass
{
public:
	using IRenderPass::IRenderPass;

	bool		 Initialize(nvrhi::IDevice* device, nvrhi::IFramebuffer* framebuffer);
	virtual void Animate(float fElapsedTimeSeconds) override;
	virtual void Render(nvrhi::IFramebuffer* framebuffer) override;
	virtual void BackBufferResizing() override;

private:
	nvrhi::IDevice*					NvrhiDevice = nullptr;
	nvrhi::ShaderHandle				CubeVS;
	nvrhi::ShaderHandle				CubeFS;
	nvrhi::InputLayoutHandle		CubeInputLayout;
	nvrhi::BindingLayoutHandle		BindingLayout;
	nvrhi::GraphicsPipelineHandle	CubePipeline;
	TUniquePtr<FStaticVertexBuffer> CubeVertexBuffer;
	TUniquePtr<FStaticIndexBuffer>	CubeIndexBuffer;
	nvrhi::FramebufferInfo			FBInfo;

	float RotationAngle = 0.0f;
};

// =============================================================================
// FUIRenderer - Shows ImGui demo window
// =============================================================================

class FUIRenderer : public FImgui_Renderer
{
public:
	using FImgui_Renderer::FImgui_Renderer;
	virtual ~FUIRenderer() override = default;

protected:
	virtual void buildUI() override
	{
		ImGui::ShowDemoWindow();
	}
};

// =============================================================================
// FCubeRenderPass IMPLEMENTATION
// =============================================================================

bool FCubeRenderPass::Initialize(nvrhi::IDevice* device, nvrhi::IFramebuffer* framebuffer)
{
	NvrhiDevice = device;
	FBInfo = framebuffer->getFramebufferInfo();

	// Load shaders
	const auto DataDir = FString::Format(TXT("{}/../../Test/TestCubeOnPlane_Data"), *GExecutablePath);

	auto CubeVertCode = ReadBinaryFile(FPath::Combine(DataDir, TXT("Cube.vert.spv")).string());
	auto CubeFragCode = ReadBinaryFile(FPath::Combine(DataDir, TXT("Cube.frag.spv")).string());

	nvrhi::ShaderDesc CubeVSDesc;
	CubeVSDesc.setShaderType(nvrhi::ShaderType::Vertex);
	CubeVS = NvrhiDevice->createShader(CubeVSDesc, CubeVertCode.data(), CubeVertCode.size());

	nvrhi::ShaderDesc CubeFSDesc;
	CubeFSDesc.setShaderType(nvrhi::ShaderType::Pixel);
	CubeFS = NvrhiDevice->createShader(CubeFSDesc, CubeFragCode.data(), CubeFragCode.size());

	if (!CubeVS || !CubeFS)
	{
		HLVM_LOG(LogTest, err, TXT("Failed to load cube shaders"));
		return false;
	}

	HLVM_LOG(LogTest, info, TXT("Cube shaders loaded successfully"));

	// Create command list for initialization
	nvrhi::CommandListHandle InitCmdList = NvrhiDevice->createCommandList();
	InitCmdList->open();

	// Create vertex buffer
	CubeVertexBuffer = TUniquePtr<FStaticVertexBuffer>(new FStaticVertexBuffer());
	CubeVertexBuffer->Initialize(InitCmdList.Get(), NvrhiDevice, CubeVertices, sizeof(CubeVertices));

	// Create index buffer
	CubeIndexBuffer = TUniquePtr<FStaticIndexBuffer>(new FStaticIndexBuffer());
	CubeIndexBuffer->Initialize(InitCmdList.Get(), NvrhiDevice, CubeIndices, sizeof(CubeIndices), nvrhi::Format::R32_UINT);

	InitCmdList->close();
	NvrhiDevice->executeCommandList(InitCmdList);

	// Create input layout
	nvrhi::VertexAttributeDesc CubeAttrs[3];
	CubeAttrs[0].setName("POSITION").setFormat(nvrhi::Format::RGB32_FLOAT).setOffset(0).setElementStride(sizeof(FCubeVertex));
	CubeAttrs[1].setName("NORMAL").setFormat(nvrhi::Format::RGB32_FLOAT).setOffset(12).setElementStride(sizeof(FCubeVertex));
	CubeAttrs[2].setName("TEXCOORD").setFormat(nvrhi::Format::RG32_FLOAT).setOffset(24).setElementStride(sizeof(FCubeVertex));

	CubeInputLayout = NvrhiDevice->createInputLayout(CubeAttrs, 3, CubeVS);

	// Create binding layout
	nvrhi::BindingLayoutDesc LayoutDesc;
	LayoutDesc.setVisibility(nvrhi::ShaderType::All);
	nvrhi::VulkanBindingOffsets offsets;
	offsets.setConstantBufferOffset(0).setShaderResourceOffset(0).setSamplerOffset(0).setUnorderedAccessViewOffset(0);
	LayoutDesc.setBindingOffsets(offsets);
	LayoutDesc.addItem(nvrhi::BindingLayoutItem::ConstantBuffer(0));
	LayoutDesc.addItem(nvrhi::BindingLayoutItem::ConstantBuffer(1));
	LayoutDesc.addItem(nvrhi::BindingLayoutItem::ConstantBuffer(2));

	BindingLayout = NvrhiDevice->createBindingLayout(LayoutDesc);

	// Create graphics pipeline
	nvrhi::GraphicsPipelineDesc PipelineDesc;
	PipelineDesc.setVertexShader(CubeVS);
	PipelineDesc.setPixelShader(CubeFS);
	PipelineDesc.setInputLayout(CubeInputLayout);
	PipelineDesc.addBindingLayout(BindingLayout);
	PipelineDesc.setPrimType(nvrhi::PrimitiveType::TriangleList);
	PipelineDesc.renderState.setRasterState(nvrhi::RasterState().setCullBack());
	PipelineDesc.renderState.depthStencilState.setDepthTestEnable(true).setDepthWriteEnable(true).setDepthFunc(nvrhi::ComparisonFunc::Less);

	CubePipeline = NvrhiDevice->createGraphicsPipeline(PipelineDesc, FBInfo);

	HLVM_LOG(LogTest, info, TXT("FCubeRenderPass initialized successfully"));
	return true;
}

void FCubeRenderPass::Animate(float fElapsedTimeSeconds)
{
	RotationAngle += fElapsedTimeSeconds * glm::radians(36.0f);
}

void FCubeRenderPass::Render(nvrhi::IFramebuffer* framebuffer)
{
	if (!NvrhiDevice || !framebuffer || !CubePipeline)
		return;

	// Create command list
	nvrhi::CommandListHandle cmd = NvrhiDevice->createCommandList();
	cmd->open();

	// Update MVP matrices
	FMat4 model = glm::rotate(FMat4(1.0f), RotationAngle, FVec3(0.0f, 1.0f, 0.0f));
	FMat4 view = glm::lookAt(FVec3(2.0f, 2.0f, 2.0f), FVec3(0.0f), FVec3(0.0f, 1.0f, 0.0f));
	FMat4 proj = glm::perspective(glm::radians(45.0f), 1.0f, 0.1f, 100.0f);
	FMat4 mvp = proj * view * model;

	// Prepare uniform data
	FMVPUniform CubeMVPUniform;
	memcpy(CubeMVPUniform.MVP, glm::value_ptr(mvp), sizeof(float) * 16);

	FLightUniform LightUniform;
	LightUniform.LightPosition[0] = 5.0f;
	LightUniform.LightPosition[1] = 5.0f;
	LightUniform.LightPosition[2] = -5.0f;
	LightUniform.LightColor[0] = 1.0f;
	LightUniform.LightColor[1] = 1.0f;
	LightUniform.LightColor[2] = 1.0f;

	FObjectUniform CubeObjectUniform;
	CubeObjectUniform.ObjectColor[0] = 0.2f;
	CubeObjectUniform.ObjectColor[1] = 0.2f;
	CubeObjectUniform.ObjectColor[2] = 1.0f;

	// Create constant buffers
	nvrhi::BufferHandle CubeMVPBuffer = NvrhiDevice->createBuffer(
		nvrhi::BufferDesc().setByteSize(sizeof(FMVPUniform)).setIsConstantBuffer(true).setInitialState(nvrhi::ResourceStates::ConstantBuffer).setKeepInitialState(true));
	cmd->writeBuffer(CubeMVPBuffer, &CubeMVPUniform, sizeof(FMVPUniform), 0);

	nvrhi::BufferHandle LightBuffer = NvrhiDevice->createBuffer(
		nvrhi::BufferDesc().setByteSize(sizeof(FLightUniform)).setIsConstantBuffer(true).setInitialState(nvrhi::ResourceStates::ConstantBuffer).setKeepInitialState(true));
	cmd->writeBuffer(LightBuffer, &LightUniform, sizeof(FLightUniform), 0);

	nvrhi::BufferHandle CubeObjectBuffer = NvrhiDevice->createBuffer(
		nvrhi::BufferDesc().setByteSize(sizeof(FObjectUniform)).setIsConstantBuffer(true).setInitialState(nvrhi::ResourceStates::ConstantBuffer).setKeepInitialState(true));
	cmd->writeBuffer(CubeObjectBuffer, &CubeObjectUniform, sizeof(FObjectUniform), 0);

	// Create binding set
	nvrhi::BindingSetDesc CubeSetDesc;
	CubeSetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, CubeMVPBuffer));
	CubeSetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(1, LightBuffer));
	CubeSetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(2, CubeObjectBuffer));
	nvrhi::BindingSetHandle CubeBindingSet = NvrhiDevice->createBindingSet(CubeSetDesc, BindingLayout);

	// Clear color attachment (pink background)
	nvrhi::Color ClearColor(0.1f, 1.0f, 0.1f, 1.0f);
	nvrhi::utils::ClearColorAttachment(cmd, framebuffer, 0, ClearColor);
	// Clear depth to 1.0 (infinity)
	nvrhi::utils::ClearDepthStencilAttachment(cmd, framebuffer, 1.0f, 0u);

	// Draw cube
	{
		nvrhi::GraphicsState State;
		State.setPipeline(CubePipeline);
		State.setFramebuffer(framebuffer);
		State.addBindingSet(CubeBindingSet);

		nvrhi::VertexBufferBinding CubeVBBinding;
		CubeVBBinding.setBuffer(CubeVertexBuffer->GetBufferHandle().Get());
		CubeVBBinding.setSlot(0);
		CubeVBBinding.setOffset(0);
		State.addVertexBuffer(CubeVBBinding);

		nvrhi::IndexBufferBinding CubeIndexBinding;
		CubeIndexBinding.setBuffer(CubeIndexBuffer->GetBufferHandle().Get());
		CubeIndexBinding.setOffset(0);
		CubeIndexBinding.setFormat(nvrhi::Format::R32_UINT);
		State.setIndexBuffer(CubeIndexBinding);

		nvrhi::Viewport Viewport(0, float(WIDTH), 0, float(HEIGHT), 0.0f, 1.0f);
		State.viewport.addViewportAndScissorRect(Viewport);

		cmd->setGraphicsState(State);

		nvrhi::DrawArguments CubeDrawArgs;
		CubeDrawArgs.setVertexCount(36);
		cmd->drawIndexed(CubeDrawArgs);
	}

	cmd->close();
	NvrhiDevice->executeCommandList(cmd);
}

void FCubeRenderPass::BackBufferResizing()
{
	CubePipeline = nullptr;
}

// =============================================================================
// TEST IMPLEMENTATION
// =============================================================================

RECORD_BOOL(test_ImguiVk)
{
	HLVM_LOG(LogTest, info, TXT("Starting ImGui Vulkan Test..."));

#if HLVM_VULKAN_RENDERER
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
		TUniquePtr<FDeviceManager> DeviceManager = FDeviceManager::Create(nvrhi::GraphicsAPI::VULKAN);
		if (!DeviceManager)
		{
			throw runtime_error("Failed to create DeviceManager");
		}

		// Configure device creation parameters
		FDeviceCreationParameters& DeviceParams = const_cast<FDeviceCreationParameters&>(DeviceManager->GetDeviceParams());
		DeviceParams.BackBufferWidth = WIDTH;
		DeviceParams.BackBufferHeight = HEIGHT;
		DeviceParams.SwapChainBufferCount = 2;
		DeviceParams.VSyncMode = 0;
		DeviceParams.bEnableDebugRuntime = HLVM_BUILD_DEBUG;
		DeviceParams.bEnableNVRHIValidationLayer = HLVM_BUILD_DEBUG;
		DeviceParams.bEnableRayTracingExtensions = false;

		// Create window, device, and swapchain
		if (!DeviceManager->CreateWindowDeviceAndSwapChain(WindowProps))
		{
			throw runtime_error("Failed to create device and swapchain");
		}

		nvrhi::IDevice* NvrhiDevice = DeviceManager->GetDevice();
		if (!NvrhiDevice)
		{
			throw runtime_error("Failed to get NVRHI device");
		}

		HLVM_LOG(LogTest, info, TXT("DeviceManager created successfully. GPU: {}"), FString(DeviceManager->GetRendererString()));

		// 3. Get framebuffer info from first framebuffer
		nvrhi::IFramebuffer* FirstFB = DeviceManager->GetFramebuffer(0);
		if (!FirstFB)
		{
			throw runtime_error("Failed to get framebuffer 0 from DeviceManager");
		}

		// 4. Create ShaderFactory for ImGui
		HLVM_LOG(LogTest, info, TXT("Creating ShaderFactory..."));
		std::shared_ptr<FShaderFactoryImpl> ShaderFactory = std::make_shared<FShaderFactoryImpl>();
		if (!ShaderFactory->Initialize(NvrhiDevice))
		{
			throw runtime_error("Failed to initialize ShaderFactory");
		}

		// 5. Create and initialize render passes
		HLVM_LOG(LogTest, info, TXT("Creating render passes..."));

		// Create FCubeRenderPass
		TSharedPtr<FCubeRenderPass> CubePass = std::make_shared<FCubeRenderPass>(DeviceManager.get());
		if (!CubePass->Initialize(NvrhiDevice, FirstFB))
		{
			throw runtime_error("Failed to initialize FCubeRenderPass");
		}

		// Create FUIRenderer
		TSharedPtr<FUIRenderer> UIRenderPass = std::make_shared<FUIRenderer>(DeviceManager.get());
		if (!UIRenderPass->Initialize(NvrhiDevice, ShaderFactory))
		{
			throw runtime_error("Failed to initialize FUIRenderer");
		}

		// 6. Add render passes to DeviceManager (cube first, then UI on top)
		DeviceManager->AddRenderPassToBack(CubePass);
		DeviceManager->AddRenderPassToBack(UIRenderPass);

		HLVM_LOG(LogTest, info, TXT("Starting render loop (RunMessageLoop)..."));

		std::thread([&]() {
			FTimer Timer;
			// Auto-close after 3 seconds
			while (Timer.MarkSec() < 1.0)
			{
			}
			DeviceManager->StopMessageLoop();
		}).detach();

		// 7. Run the message loop - this will call Render() on each pass
		DeviceManager->RunMessageLoop();

		HLVM_LOG(LogTest, info, TXT("Test completed successfully!"));

		// Cleanup - shared_ptr will release when going out of scope
		DeviceManager->RemoveRenderPass(UIRenderPass);
		DeviceManager->RemoveRenderPass(CubePass);

		UIRenderPass->Shutdown();

		return true;
	}
	catch (const exception& e)
	{
		HLVM_LOG(LogTest, critical, TXT("Test failed: {}"), TO_TCHAR_CSTR(e.what()));
		return false;
	}
	catch (...)
	{
		HLVM_LOG(LogTest, critical, TXT("Unknown fatal error occurred"));
		return false;
	}
#else  // HLVM_VULKAN_RENDERER
	HLVM_LOG(LogTest, warning, TXT("Vulkan renderer not enabled - skipping test"));
	return true;
#endif // HLVM_VULKAN_RENDERER
}
#pragma clang diagnostic pop
