/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 *
 * TestRenderSponza.cpp - Sponza Scene PBR Rendering Test
 *
 * Demonstrates HLVM Engine rendering with the Sponza glTF scene.
 * Uses NVRHI/Vulkan with pre-compiled SPIR-V shaders.
 *
 * RENDERING PIPELINE OVERVIEW:
 * --------------------------
 * 1. DeviceManager creates Vulkan device, swapchain, and framebuffers
 * 2. Sponza scene loaded from glTF via FScene3DLoader
 * 3. Per-mesh vertex/index buffers created from IMesh interface
 * 4. White texture fallback for missing PBR textures
 * 5. Scene rotated via FNode hierarchy (root node rotation)
 * 6. Per-frame: update MVP matrices, draw indexed meshes
 */

#include "Test.h"
#include "Renderer/DeviceManager.h"
#include "Renderer/Window/WindowDefinition.h"
#include "Renderer/SceneGraph/PerspectiveCameraNode.h"
#include "Renderer/SceneGraph/FNode.h"
#include "Renderer/Scene3D/Scene3DLoader.h"
#include "Renderer/Scene3D/Scene3DNode.h"
#include "Renderer/Mesh/IMesh.h"
#include "Renderer/RHI/Object/Buffer.h"
#include <nvrhi/utils.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cfloat>
#include <fstream>
#include <vector>
#include <memory>

DECLARE_LOG_CATEGORY(LogTest)

#if HLVM_VULKAN_RENDERER
#include "Renderer/Window/GLFW3/GLFW3VulkanWindow.h"
#include "Renderer/RHI/Vulkan/VulkanDefinition.h"
#include "Renderer/RHI/RHICommon.h"

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
#pragma clang diagnostic ignored "-Wold-style-cast"

using namespace std;

// =============================================================================
// CONFIGURATION
// =============================================================================

const uint32_t	   WIDTH = 800;
const uint32_t	   HEIGHT = 600;
static const char* WINDOW_TITLE = "Render Sponza Test";

// =============================================================================
// TEST STRUCTURE
// =============================================================================

struct FMeshRenderData
{
	TUniquePtr<FStaticVertexBuffer> VertexBuffer;
	TUniquePtr<FStaticIndexBuffer>	IndexBuffer;
	uint32_t						IndexCount = 0;
};

struct FRenderSponzaTestContext
{
	// DeviceManager
	TUniquePtr<FDeviceManager> DeviceManager;
	nvrhi::IDevice*			   NvrhiDevice = nullptr;

	// Command list
	nvrhi::CommandListHandle NvrhiCommandList;

	// Shaders
	nvrhi::ShaderHandle VS;
	nvrhi::ShaderHandle FS;

	// Pipeline
	nvrhi::InputLayoutHandle	  InputLayout;
	nvrhi::BindingLayoutHandle	  BindingLayout;
	nvrhi::GraphicsPipelineHandle Pipeline;

	// White texture fallback
	nvrhi::TextureHandle WhiteTexture;

	// Scene data
	std::shared_ptr<FScene3DNode> Scene;
	TUniquePtr<FNode>			   RootNode;
	TVector<FMeshRenderData>	   MeshRenderData;

	// Camera
	TUniquePtr<FPerspectiveCameraNode> Camera;

	// Scene bounds (for camera positioning)
	FVec3 SceneCenter = FVec3(0.0f);
	float SceneRadius = 10.0f;
};

static void CleanupResources(FRenderSponzaTestContext& Context)
{
	HLVM_LOG(LogTest, info, TXT("Cleaning up resources..."));

	Context.Pipeline = nullptr;
	Context.InputLayout = nullptr;
	Context.BindingLayout = nullptr;

	Context.MeshRenderData.clear();
	Context.WhiteTexture = nullptr;

	Context.VS = nullptr;
	Context.FS = nullptr;

	if (Context.NvrhiCommandList)
	{
		Context.NvrhiCommandList.Reset();
	}

	Context.DeviceManager = nullptr;
	Context.NvrhiDevice = nullptr;

	Context.Scene = nullptr;
	Context.RootNode = nullptr;
	Context.Camera = nullptr;
}

// =============================================================================
// UNIFORM BUFFERS
// =============================================================================

// Binding 0: MVP matrices (192 bytes = 3x mat4, 16-byte aligned)
struct FMVPUniform
{
	alignas(16) float Model[4][4];
	alignas(16) float View[4][4];
	alignas(16) float Proj[4][4];
};

// Binding 4: Light uniform (16 bytes)
struct FLightUniform
{
	alignas(16) float LightPosition[3];
	alignas(16) float LightColor[3];
};

// Binding 5: Camera uniform (16 bytes)
struct FCameraUniform
{
	alignas(16) float CameraPosition[3];
	alignas(16) float Padding[1];
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
	file.read(buffer.data(), fileSize);
	file.close();

	return buffer;
}

// =============================================================================
// TEST IMPLEMENTATION
// =============================================================================

RECORD_BOOL(test_RenderSponza)
{
	HLVM_LOG(LogTest, info, TXT("Starting Render Sponza Test..."));

	FRenderSponzaTestContext Context;

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

		// 3. Create command list
		HLVM_LOG(LogTest, info, TXT("Creating command list..."));
		nvrhi::CommandListParameters Params = {};
		Params.enableImmediateExecution = false;
		Context.NvrhiCommandList = Context.NvrhiDevice->createCommandList(Params);
		Context.NvrhiCommandList->open();

		// 4. Load shaders from SPIR-V files
		HLVM_LOG(LogTest, info, TXT("Loading shaders..."));
		const auto DataDir = FString::Format(TXT("{}/../../Test/{}_Data"), *GExecutablePath, *GExecutableName);

		auto VSCode = ReadBinaryFile(FPath::Combine(DataDir, TXT("shader.vert.spv")).string());
		auto FSCode = ReadBinaryFile(FPath::Combine(DataDir, TXT("shader.frag.spv")).string());

		nvrhi::ShaderDesc VSDesc;
		VSDesc.setShaderType(nvrhi::ShaderType::Vertex);
		Context.VS = Context.NvrhiDevice->createShader(VSDesc, VSCode.data(), VSCode.size());

		nvrhi::ShaderDesc FSDesc;
		FSDesc.setShaderType(nvrhi::ShaderType::Pixel);
		Context.FS = Context.NvrhiDevice->createShader(FSDesc, FSCode.data(), FSCode.size());

		// 5. Create white texture fallback
		HLVM_LOG(LogTest, info, TXT("Creating white texture fallback..."));
		static const uint32_t WhitePixel = 0xFFFFFFFF;
		Context.WhiteTexture = Context.NvrhiDevice->createTexture(
			nvrhi::TextureDesc()
				.setWidth(1)
				.setHeight(1)
				.setFormat(nvrhi::Format::RGBA8_UNORM)
				.setIsUAV(false)
				.setInitialState(nvrhi::ResourceStates::ShaderResource)
				.setKeepInitialState(true));

		Context.NvrhiCommandList->writeTexture(Context.WhiteTexture, 0, 0, &WhitePixel, 4);

		// 6. Load Sponza scene
		HLVM_LOG(LogTest, info, TXT("Loading Sponza scene..."));
		const FString GitRoot = FString::Format(TXT("{}/../../../../.."), *GExecutablePath);
		const FPath scenePath = FPath(FString::Format(TXT("{}{}"), *GitRoot, TXT("/Samples/Assets/sponza/Sponza01.gltf")));
		Context.Scene = FScene3DLoader::LoadFromFile(scenePath);
		if (!Context.Scene)
		{
			throw runtime_error("Failed to load Sponza scene");
		}

		auto meshes = Context.Scene->GetAllMesh();
		HLVM_LOG(LogTest, info, TXT("Loaded scene with {} meshes"), meshes.size());

		// 7. Calculate scene bounding box for camera positioning
		HLVM_LOG(LogTest, info, TXT("Calculating scene bounding box..."));
		FVec3 Min(+FLT_MAX), Max(-FLT_MAX);
		for (auto& mesh : meshes)
		{
			for (auto& vert : mesh->GetVertices())
			{
				Min = glm::min(Min, vert.Position);
				Max = glm::max(Max, vert.Position);
			}
		}
		Context.SceneCenter = (Min + Max) * 0.5f;
		Context.SceneRadius = glm::length(Max - Min) * 0.5f;
		HLVM_LOG(LogTest, info, TXT("Scene center: ({:.2f}, {:.2f}, {:.2f}), radius: {:.2f}"),
				 Context.SceneCenter.x, Context.SceneCenter.y, Context.SceneCenter.z, Context.SceneRadius);

		// 8. Create root node for scene rotation
		Context.RootNode = TUniquePtr<FNode>(new FNode());
		Context.RootNode->SetName(TXT("SponzaRoot"));

		// 9. Create per-mesh render data
		HLVM_LOG(LogTest, info, TXT("Creating per-mesh render data..."));
		Context.MeshRenderData.reserve(meshes.size());
		for (auto& mesh : meshes)
		{
			FMeshRenderData MeshData;

			const auto& Vertices = mesh->GetVertices();
			MeshData.VertexBuffer = TUniquePtr<FStaticVertexBuffer>(new FStaticVertexBuffer());
			MeshData.VertexBuffer->Initialize(
				Context.NvrhiCommandList.Get(),
				Context.NvrhiDevice,
				Vertices.data(),
				Vertices.size() * sizeof(FVertex));

			const auto& Indices = mesh->GetIndices();
			MeshData.IndexCount = static_cast<uint32_t>(Indices.size());
			MeshData.IndexBuffer = TUniquePtr<FStaticIndexBuffer>(new FStaticIndexBuffer());
			MeshData.IndexBuffer->Initialize(
				Context.NvrhiCommandList.Get(),
				Context.NvrhiDevice,
				Indices.data(),
				Indices.size() * sizeof(uint32_t),
				nvrhi::Format::R32_UINT);

			Context.MeshRenderData.push_back(std::move(MeshData));
		}

		Context.NvrhiCommandList->close();
		Context.NvrhiDevice->executeCommandList(Context.NvrhiCommandList);

		// 10. Create input layout (FVertex: position(12) + normal(12) + uv(8) + tangent(12) = 44 bytes)
		HLVM_LOG(LogTest, info, TXT("Creating input layout..."));
		nvrhi::VertexAttributeDesc Attrs[4];
		Attrs[0].setName("POSITION").setFormat(nvrhi::Format::RGB32_FLOAT).setOffset(0).setElementStride(sizeof(FVertex));
		Attrs[1].setName("NORMAL").setFormat(nvrhi::Format::RGB32_FLOAT).setOffset(12).setElementStride(sizeof(FVertex));
		Attrs[2].setName("TEXCOORD").setFormat(nvrhi::Format::RG32_FLOAT).setOffset(24).setElementStride(sizeof(FVertex));
		Attrs[3].setName("TANGENT").setFormat(nvrhi::Format::RGB32_FLOAT).setOffset(32).setElementStride(sizeof(FVertex));

		Context.InputLayout = Context.NvrhiDevice->createInputLayout(Attrs, 4, Context.VS);

		// 11. Create binding layout (6 bindings)
		HLVM_LOG(LogTest, info, TXT("Creating binding layout..."));
		nvrhi::BindingLayoutDesc LayoutDesc;
		LayoutDesc.setVisibility(nvrhi::ShaderType::All);

		nvrhi::VulkanBindingOffsets offsets;
		offsets.setConstantBufferOffset(0).setShaderResourceOffset(0).setSamplerOffset(0).setUnorderedAccessViewOffset(0);
		LayoutDesc.setBindingOffsets(offsets);

		LayoutDesc.addItem(nvrhi::BindingLayoutItem::ConstantBuffer(0));
		LayoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(1));
		LayoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(2));
		LayoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(3));
		LayoutDesc.addItem(nvrhi::BindingLayoutItem::ConstantBuffer(4));
		LayoutDesc.addItem(nvrhi::BindingLayoutItem::ConstantBuffer(5));

		Context.BindingLayout = Context.NvrhiDevice->createBindingLayout(LayoutDesc);

		// 12. Get framebuffer info
		nvrhi::IFramebuffer* FirstFB = Context.DeviceManager->GetFramebuffer(0);
		if (!FirstFB)
		{
			throw runtime_error("Failed to get framebuffer 0 from DeviceManager");
		}
		nvrhi::FramebufferInfo FBInfo = FirstFB->getFramebufferInfo();

		// 13. Create graphics pipeline
		HLVM_LOG(LogTest, info, TXT("Creating graphics pipeline..."));
		{
			// YuHang : pipeline desc need to locally scoped in order to release deps for handles
			// Otherwise it will cause a segfault due to we clean up resources before pipeline is released
			nvrhi::GraphicsPipelineDesc PipelineDesc;
			PipelineDesc.setVertexShader(Context.VS);
			PipelineDesc.setPixelShader(Context.FS);
			PipelineDesc.setInputLayout(Context.InputLayout);
			PipelineDesc.addBindingLayout(Context.BindingLayout);
			PipelineDesc.setPrimType(nvrhi::PrimitiveType::TriangleList);
			PipelineDesc.renderState.setRasterState(nvrhi::RasterState().setCullNone());
			PipelineDesc.renderState.depthStencilState.setDepthTestEnable(true).setDepthWriteEnable(true).setDepthFunc(nvrhi::ComparisonFunc::Less);

			Context.Pipeline = Context.NvrhiDevice->createGraphicsPipeline(PipelineDesc, FBInfo);
		}

		// 14. Create camera
		HLVM_LOG(LogTest, info, TXT("Creating camera..."));
		Context.Camera = TUniquePtr<FPerspectiveCameraNode>(new FPerspectiveCameraNode());

		// Camera must be BEHIND scene (z > scene_center_z + scene_radius) since camera looks in -Z direction
		FVec3 CameraPos = FVec3(Context.SceneCenter.x, Context.SceneCenter.y, Context.SceneCenter.z + Context.SceneRadius * 2.0f);
		Context.Camera->MoveToAndLookAt(CameraPos, Context.SceneCenter);
		Context.Camera->SetFovY(glm::radians(45.0f));
		Context.Camera->SetAspectRatio(static_cast<float>(WIDTH) / static_cast<float>(HEIGHT));
		Context.Camera->SetNearPlane(0.1f);
		Context.Camera->SetFarPlane(100000.0f);
		Context.Camera->UpdateWorldTransform();

		// Scale scene down to fit in view frustum
		const float SceneScale = 1.f;
		Context.RootNode->SetScale(FVec3(SceneScale, SceneScale, SceneScale));
		HLVM_LOG(LogTest, info, TXT("Scene scaled by {:.4f} to fit view frustum"), SceneScale);

		// DIAGNOSTIC: Log camera position
		FMat4 ViewMat = Context.Camera->GetViewMatrix();
		FMat4 ProjMat = Context.Camera->GetProjectionMatrix();
		FVec3 CamPos = Context.Camera->GetPosition();
		HLVM_LOG(LogTest, info, TXT("Camera position: ({:.2f}, {:.2f}, {:.2f})"), CamPos.x, CamPos.y, CamPos.z);
		HLVM_LOG(LogTest, info, TXT("Camera far plane: {:.2f}, near plane: {:.2f}"), Context.Camera->GetFarPlane(), Context.Camera->GetNearPlane());

		// 15. Render loop
		HLVM_LOG(LogTest, info, TXT("Starting render loop..."));
		FTimer		  Timer;
		int			  TestFrameCount = 0;
		constexpr int MaxFrames = 600;

		while (TestFrameCount < MaxFrames)
		{
			HLVM_ENSURE(Context.DeviceManager->BeginFrame());

			nvrhi::IFramebuffer* Framebuffer = Context.DeviceManager->GetCurrentFramebuffer();
			if (!Framebuffer)
			{
				HLVM_LOG(LogTest, err, TXT("Failed to get current framebuffer"));
				break;
			}

			float Angle = static_cast<float>(Timer.MarkSec()) * glm::radians(30.0f);
			Context.RootNode->SetRotation(FVec3(0.0f, Angle, 0.0f));
			Context.RootNode->UpdateWorldTransform();

			FMat4 Model = Context.RootNode->GetWorldTransform();
			FMat4 View = Context.Camera->GetViewMatrix();
			FMat4 Projection = Context.Camera->GetProjectionMatrix();

			Context.NvrhiCommandList = Context.NvrhiDevice->createCommandList();
			Context.NvrhiCommandList->open();

			FMVPUniform MVPUniform;
			memcpy(MVPUniform.Model, glm::value_ptr(Model), sizeof(float) * 16);
			memcpy(MVPUniform.View, glm::value_ptr(View), sizeof(float) * 16);
			memcpy(MVPUniform.Proj, glm::value_ptr(Projection), sizeof(float) * 16);

			nvrhi::BufferHandle MVPBuffer = Context.NvrhiDevice->createBuffer(
				nvrhi::BufferDesc()
					.setByteSize(sizeof(FMVPUniform))
					.setIsConstantBuffer(true)
					.setInitialState(nvrhi::ResourceStates::ConstantBuffer)
					.setKeepInitialState(true));
			Context.NvrhiCommandList->writeBuffer(MVPBuffer, &MVPUniform, sizeof(FMVPUniform), 0);

			FLightUniform LightUniform;
			FVec3 LightPos = FVec3(0.0f, 10.0f, 10.0f);
			FVec3 LightColor = FVec3(1.0f, 1.0f, 1.0f);
			memcpy(LightUniform.LightPosition, glm::value_ptr(LightPos), sizeof(float) * 3);
			memcpy(LightUniform.LightColor, glm::value_ptr(LightColor), sizeof(float) * 3);

			nvrhi::BufferHandle LightBuffer = Context.NvrhiDevice->createBuffer(
				nvrhi::BufferDesc()
					.setByteSize(sizeof(FLightUniform))
					.setIsConstantBuffer(true)
					.setInitialState(nvrhi::ResourceStates::ConstantBuffer)
					.setKeepInitialState(true));
			Context.NvrhiCommandList->writeBuffer(LightBuffer, &LightUniform, sizeof(FLightUniform), 0);

			FCameraUniform CameraUniform;
			FVec3 CamPos = Context.Camera->GetPosition();
			memcpy(CameraUniform.CameraPosition, glm::value_ptr(CamPos), sizeof(float) * 3);

			nvrhi::BufferHandle CameraBuffer = Context.NvrhiDevice->createBuffer(
				nvrhi::BufferDesc()
					.setByteSize(sizeof(FCameraUniform))
					.setIsConstantBuffer(true)
					.setInitialState(nvrhi::ResourceStates::ConstantBuffer)
					.setKeepInitialState(true));
			Context.NvrhiCommandList->writeBuffer(CameraBuffer, &CameraUniform, sizeof(FCameraUniform), 0);

			nvrhi::BindingSetDesc SetDesc;
			SetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, MVPBuffer));
			SetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(1, Context.WhiteTexture, nvrhi::Format::RGBA8_UNORM));
			SetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(2, Context.WhiteTexture, nvrhi::Format::RGBA8_UNORM));
			SetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(3, Context.WhiteTexture, nvrhi::Format::RGBA8_UNORM));
			SetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(4, LightBuffer));
			SetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(5, CameraBuffer));
			nvrhi::BindingSetHandle BindingSet = Context.NvrhiDevice->createBindingSet(SetDesc, Context.BindingLayout);

			nvrhi::Color ClearColor(0.4f, 0.4f, 0.4f, 1.0f);
			nvrhi::utils::ClearColorAttachment(Context.NvrhiCommandList, Framebuffer, 0, ClearColor);
			nvrhi::utils::ClearDepthStencilAttachment(Context.NvrhiCommandList, Framebuffer, 1.0f, 0u);

			auto RenderMeshes = Context.Scene->GetAllMesh();
			for (size_t MeshIdx = 0; MeshIdx < RenderMeshes.size() && MeshIdx < Context.MeshRenderData.size(); ++MeshIdx)
			{
				auto& MeshData = Context.MeshRenderData[MeshIdx];
				auto& Mesh = RenderMeshes[MeshIdx];

				// DIAGNOSTIC: Log first 5 meshes NDC coordinates
				if (MeshIdx < 5)
				{
					const auto& Vertices = Mesh->GetVertices();
					const auto& Indices = Mesh->GetIndices();

					FVec3 FirstPos = Vertices.empty() ? FVec3(0) : Vertices[0].Position;
					FMat4 ModelMat = Context.RootNode->GetWorldTransform();
					FMat4 VP = ProjMat * ViewMat * ModelMat;
					FVec4 ClipPos = VP * FVec4(FirstPos, 1.0f);
					FVec3 NDCPos = ClipPos.w != 0.0f ? FVec3(ClipPos) / ClipPos.w : FVec3(0);

					HLVM_LOG(LogTest, info, TXT("Mesh[{}]: name={}, firstVert=({:.2f}, {:.2f}, {:.2f}), NDC=({:.4f}, {:.4f}, {:.4f})"),
						MeshIdx, Mesh->GetName().c_str(), FirstPos.x, FirstPos.y, FirstPos.z, NDCPos.x, NDCPos.y, NDCPos.z);
				}

				nvrhi::GraphicsState State;
				State.setPipeline(Context.Pipeline);
				State.setFramebuffer(Framebuffer);
				State.addBindingSet(BindingSet);

				nvrhi::VertexBufferBinding VBBinding;
				VBBinding.setBuffer(MeshData.VertexBuffer->GetBufferHandle().Get());
				VBBinding.setSlot(0);
				VBBinding.setOffset(0);
				State.addVertexBuffer(VBBinding);

				nvrhi::IndexBufferBinding IndexBinding;
				IndexBinding.setBuffer(MeshData.IndexBuffer->GetBufferHandle().Get());
				IndexBinding.setOffset(0);
				IndexBinding.setFormat(nvrhi::Format::R32_UINT);
				State.setIndexBuffer(IndexBinding);

				nvrhi::Viewport Viewport(0, float(WIDTH), 0, float(HEIGHT), 0.0f, 1.0f);
				State.viewport.addViewportAndScissorRect(Viewport);

				Context.NvrhiCommandList->setGraphicsState(State);

				nvrhi::DrawArguments DrawArgs;
				DrawArgs.setVertexCount(MeshData.IndexCount);
				Context.NvrhiCommandList->drawIndexed(DrawArgs);
			}

			Context.NvrhiCommandList->close();
			Context.NvrhiDevice->executeCommandList(Context.NvrhiCommandList);

			Context.DeviceManager->EndFrame();
			Context.DeviceManager->Present();

			Context.NvrhiDevice->waitForIdle();

			TestFrameCount++;

			if (Timer.MarkSec() > 3.0)
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

#endif // HLVM_VULKAN_RENDERER
