/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 *
 * Ray Traced Triangle Test
 *
 * Demonstrates HLVM Engine ray tracing pipeline by rendering a simple
 * ray traced triangle using NVRHI's ray tracing APIs.
 * Adapted from NVIDIA Donut rt_triangle sample.
 */

#include "Test.h"
#include "Renderer/DeviceManager.h"
#include "Renderer/RHI/Object/Buffer.h"
#include "Renderer/Common/FBindingCache.h"
#include "Renderer/Common/FCommonRenderPasses.h"
#include <nvrhi/utils.h>
#include <Utility/Timer.h>

DECLARE_LOG_CATEGORY(LogTest)

#if HLVM_VULKAN_RENDERER

// =============================================================================
// CONFIGURATION
// =============================================================================

static const char*	  WINDOW_TITLE = "Ray Traced Triangle Test";
static const uint32_t WIDTH = 800;
static const uint32_t HEIGHT = 600;

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
// FRayTracedTrianglePass - IRenderPass implementation
// =============================================================================

class FRayTracedTrianglePass : public IRenderPass
{
public:
	using IRenderPass::IRenderPass;

	bool Initialize(nvrhi::IDevice* Device, nvrhi::IFramebuffer* Framebuffer, const FString& InWindowTitle)
	{
		NvrhiDevice = Device;
		FBInfo = Framebuffer->getFramebufferInfo();
		WindowTitle = InWindowTitle;

		// Initialize binding cache
		BindingCache.SetDevice(NvrhiDevice);
		// Load ray tracing shaders from linked SPIR-V library
		const auto DataDir = FString::Format(
			TXT("{}/../../Test/TestRayTracedTriangle_Data"),
			*GExecutablePath);

		auto RayTracingLibCode = ReadBinaryFile(
			FPath::Combine(DataDir, TXT("RayTracingLib.spv")).string());

		ShaderLibrary = NvrhiDevice->createShaderLibrary(
			RayTracingLibCode.data(), RayTracingLibCode.size());
		if (!ShaderLibrary)
		{
			HLVM_LOG(LogTest, err, TXT("Failed to load ray tracing shader library"));
			return false;
		}

		// Get ray tracing shaders from library
		// For shader LIBRARIES, NVRHI finds shaders by type - name is ignored in pipeline
		// But for lookup, we use actual entry point names
		RayGenShader = ShaderLibrary->getShader("RayGen", nvrhi::ShaderType::RayGeneration);
		ClosestHitShader = ShaderLibrary->getShader("ClosestHit", nvrhi::ShaderType::ClosestHit);
		MissShader = ShaderLibrary->getShader("Miss", nvrhi::ShaderType::Miss);

		if (!RayGenShader || !ClosestHitShader || !MissShader)
		{
			HLVM_LOG(LogTest, err, TXT("Failed to get ray tracing shaders"));
			HLVM_LOG(LogTest, err, TXT("RayGen={}, ClosestHit={}, Miss={}"),
				RayGenShader ? TXT("ok") : TXT("null"),
				ClosestHitShader ? TXT("ok") : TXT("null"),
				MissShader ? TXT("ok") : TXT("null"));
			return false;
		}

		HLVM_LOG(LogTest, info, TXT("Ray tracing shaders loaded successfully"));

		// Create command list for initialization
		nvrhi::CommandListHandle InitCmdList = NvrhiDevice->createCommandList();
		InitCmdList->open();

		// Create triangle geometry buffers
		nvrhi::BufferDesc BufferDesc;
		BufferDesc.byteSize = sizeof(uint32_t) * 3;
		BufferDesc.initialState = nvrhi::ResourceStates::ShaderResource;
		BufferDesc.keepInitialState = true;
		BufferDesc.isAccelStructBuildInput = true;
		IndexBuffer = NvrhiDevice->createBuffer(BufferDesc);

		BufferDesc.byteSize = sizeof(float) * 9; // 3 vertices * 3 floats
		BufferDesc.isAccelStructBuildInput = true;
		VertexBuffer = NvrhiDevice->createBuffer(BufferDesc);

		// Write triangle data
		uint32_t Indices[3] = { 0, 1, 2 };
		float	 Vertices[9] = {
			   0.0f, -1.0f, 1.0f, // vertex 0
			   -1.0f, 1.0f, 1.0f, // vertex 1
			   1.0f, 1.0f, 1.0f	  // vertex 2
		};

		InitCmdList->writeBuffer(IndexBuffer, Indices, sizeof(Indices));
		InitCmdList->writeBuffer(VertexBuffer, Vertices, sizeof(Vertices));

		// Create BLAS (Bottom Level Acceleration Structure)
		nvrhi::rt::AccelStructDesc BlasDesc;
		BlasDesc.isTopLevel = false;
		nvrhi::rt::GeometryDesc GeometryDesc;
		auto&					Triangles = GeometryDesc.geometryData.triangles;
		Triangles.indexBuffer = IndexBuffer;
		Triangles.vertexBuffer = VertexBuffer;
		Triangles.indexFormat = nvrhi::Format::R32_UINT;
		Triangles.indexCount = 3;
		Triangles.vertexFormat = nvrhi::Format::RGB32_FLOAT;
		Triangles.vertexStride = sizeof(float) * 3;
		Triangles.vertexCount = 3;
		GeometryDesc.geometryType = nvrhi::rt::GeometryType::Triangles;
		GeometryDesc.flags = nvrhi::rt::GeometryFlags::Opaque;
		BlasDesc.bottomLevelGeometries.push_back(GeometryDesc);

		BottomLevelAS = NvrhiDevice->createAccelStruct(BlasDesc);
		nvrhi::utils::BuildBottomLevelAccelStruct(InitCmdList, BottomLevelAS, BlasDesc);

		// Create TLAS (Top Level Acceleration Structure)
		nvrhi::rt::AccelStructDesc TlasDesc;
		TlasDesc.isTopLevel = true;
		TlasDesc.topLevelMaxInstances = 1;
		TopLevelAS = NvrhiDevice->createAccelStruct(TlasDesc);

		// Identity transform matrix (3x4 float array)
		float Transform[12] = {
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f
		};

		nvrhi::rt::InstanceDesc InstanceDesc;
		InstanceDesc.bottomLevelAS = BottomLevelAS;
		InstanceDesc.instanceMask = 1;
		InstanceDesc.flags = nvrhi::rt::InstanceFlags::TriangleFrontCounterclockwise;
		memcpy(InstanceDesc.transform, Transform, sizeof(Transform));

		InitCmdList->buildTopLevelAccelStruct(TopLevelAS, &InstanceDesc, 1);

		InitCmdList->close();
		NvrhiDevice->executeCommandList(InitCmdList);

		HLVM_LOG(LogTest, info, TXT("Acceleration structures built successfully"));

		// Create ray tracing binding layout
		nvrhi::BindingLayoutDesc RayTracingLayoutDesc;
		RayTracingLayoutDesc.visibility = nvrhi::ShaderType::All;
		RayTracingLayoutDesc.bindingOffsets.setUnorderedAccessViewOffset(0);
		RayTracingLayoutDesc.bindings = {
			nvrhi::BindingLayoutItem::RayTracingAccelStruct(0),
			nvrhi::BindingLayoutItem::Texture_UAV(1)
		};
		BindingLayout = NvrhiDevice->createBindingLayout(RayTracingLayoutDesc);

		// Create ray tracing pipeline
		nvrhi::rt::PipelineDesc PipelineDesc;
		PipelineDesc.globalBindingLayouts = { BindingLayout };
		PipelineDesc.shaders = {
			{ "", RayGenShader, nullptr },
			{ "", MissShader, nullptr }
		};
		PipelineDesc.hitGroups = { { "HitGroup",
			ClosestHitShader,
			nullptr,
			nullptr,
			nullptr,
			false } };
		PipelineDesc.maxPayloadSize = sizeof(float) * 4;

		Pipeline = NvrhiDevice->createRayTracingPipeline(PipelineDesc);
		if (!Pipeline)
		{
			HLVM_LOG(LogTest, err, TXT("Failed to create ray tracing pipeline"));
			return false;
		}

		HLVM_LOG(LogTest, info, TXT("Ray tracing pipeline created successfully"));

		// Create shader table
		ShaderTable = Pipeline->createShaderTable();
		ShaderTable->setRayGenerationShader("RayGen");
		ShaderTable->addHitGroup("HitGroup");
		ShaderTable->addMissShader("Miss");

		HLVM_LOG(LogTest, info, TXT("Shader table created successfully"));

		// Blit is now handled by FCommonRenderPasses::BlitTexture()

		// Create command list
		CommandList = NvrhiDevice->createCommandList();

		HLVM_LOG(LogTest, info, TXT("FRayTracedTrianglePass initialized successfully"));
		return true;
	}

	void Shutdown()
	{
		BindingCache.Clear();

		CommandList = nullptr;
		BindingLayout = nullptr;
		ShaderTable = nullptr;
		Pipeline = nullptr;
		TopLevelAS = nullptr;
		BottomLevelAS = nullptr;
		VertexBuffer = nullptr;
		IndexBuffer = nullptr;
		RenderTarget = nullptr;
		RayTracingBindingSet = nullptr;
		ShaderLibrary = nullptr;
		RayGenShader = nullptr;
		ClosestHitShader = nullptr;
		MissShader = nullptr;

		HLVM_LOG(LogTest, info, TXT("FRayTracedTrianglePass shutdown complete"));
	}

	virtual void Animate(float fElapsedTimeSeconds) override
	{
		FrameCount++;
		FPSUpdateTimer += fElapsedTimeSeconds;
		float FPS = float(FrameCount) / FPSUpdateTimer;
		if (FPSUpdateTimer >= 1.0f)
		{
			WindowTitle = FString::Format(TXT("Ray Traced Triangle Test - FPS: {:.1f}"), FPS);

			if (auto* DM = GetDeviceManager())
			{
				DM->SetWindowTitle(WindowTitle);
			}
		}
		// Log
		HLVM_LOG(LogTest, info, TXT("FPS: {:.1f}"), FPS);
	}

	virtual void Render(nvrhi::IFramebuffer* Framebuffer) override
	{
		if (!NvrhiDevice || !Framebuffer || !Pipeline)
			return;

		const auto& CurrentFBInfo = Framebuffer->getFramebufferInfo();

		if (!RenderTarget || CurrentFBInfo.width != LastWidth || CurrentFBInfo.height != LastHeight)
		{
			LastWidth = CurrentFBInfo.width;
			LastHeight = CurrentFBInfo.height;

			nvrhi::TextureDesc TextureDesc;
			TextureDesc.dimension = nvrhi::TextureDimension::Texture2D;
			TextureDesc.width = CurrentFBInfo.width;
			TextureDesc.height = CurrentFBInfo.height;
			TextureDesc.format = nvrhi::Format::RGBA8_UNORM;
			TextureDesc.isUAV = true;
			TextureDesc.isRenderTarget = false;
			TextureDesc.initialState = nvrhi::ResourceStates::UnorderedAccess;
			TextureDesc.keepInitialState = true;
			TextureDesc.debugName = "RayTracedRT";

			RenderTarget = NvrhiDevice->createTexture(TextureDesc);

			BindingCache.Clear();

			nvrhi::BindingSetDesc BindingSetDesc;
			BindingSetDesc.bindings = {
				nvrhi::BindingSetItem::RayTracingAccelStruct(0, TopLevelAS),
				nvrhi::BindingSetItem::Texture_UAV(1, RenderTarget)
			};
			RayTracingBindingSet = BindingCache.GetOrCreateBindingSet(BindingSetDesc, BindingLayout);
		}

		CommandList->open();

		// Ray tracing pass
		{
			nvrhi::rt::State RTState;
			RTState.shaderTable = ShaderTable;
			RTState.bindings = { RayTracingBindingSet };
			CommandList->setRayTracingState(RTState);

			nvrhi::rt::DispatchRaysArguments Args;
			Args.width = CurrentFBInfo.width;
			Args.height = CurrentFBInfo.height;
			CommandList->dispatchRays(Args);
		}
		// Blit pass - known NVRHI framebuffer state issue after RT dispatch
		FCommonRenderPasses::BlitParameters BlitParams;
		FCommonRenderPasses::BlitTexture(
			CommandList,
			Framebuffer,
			RenderTarget,
			&BindingCache,
			CurrentFBInfo.width,
			CurrentFBInfo.height,
			BlitParams);

		CommandList->close();
		NvrhiDevice->executeCommandList(CommandList);
	}

	virtual void BackBufferResizing() override
	{
		RenderTarget = nullptr;
		RayTracingBindingSet = nullptr;
		BindingCache.Clear();
	}

	uint32_t GetFrameCount() const
	{
		return FrameCount;
	}

private:
	nvrhi::IDevice*		   NvrhiDevice = nullptr;
	nvrhi::FramebufferInfo FBInfo;
	FString				   WindowTitle;

	nvrhi::ShaderLibraryHandle ShaderLibrary;
	nvrhi::ShaderHandle		   RayGenShader;
	nvrhi::ShaderHandle		   ClosestHitShader;
	nvrhi::ShaderHandle		   MissShader;

	nvrhi::rt::AccelStructHandle BottomLevelAS;
	nvrhi::rt::AccelStructHandle TopLevelAS;
	nvrhi::BufferHandle			 IndexBuffer;
	nvrhi::BufferHandle			 VertexBuffer;

	nvrhi::rt::PipelineHandle	 Pipeline;
	nvrhi::rt::ShaderTableHandle ShaderTable;

	nvrhi::BindingLayoutHandle BindingLayout;
	nvrhi::TextureHandle	   RenderTarget;
	nvrhi::BindingSetHandle	   RayTracingBindingSet;

	FBindingCache BindingCache;

	nvrhi::CommandListHandle CommandList;

	uint32_t LastWidth = 0;
	uint32_t LastHeight = 0;

	uint32_t FrameCount = 0;
	float	 FPSUpdateTimer = 0.0f;
};

// =============================================================================
// TEST IMPLEMENTATION
// =============================================================================

RECORD_BOOL(test_RayTracedTriangle)
{
	HLVM_LOG(LogTest, info, TXT("Starting Ray Traced Triangle Test..."));

	try
	{
		HLVM_LOG(LogTest, info, TXT("Creating window..."));
		IWindow::Properties WindowProps;
		WindowProps.Title = WINDOW_TITLE;
		WindowProps.Extent = { WIDTH, HEIGHT };
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
			throw std::runtime_error("Ray tracing not supported on this device");
		}

		nvrhi::IFramebuffer* FirstFB = DeviceManager->GetFramebuffer(0);

		HLVM_LOG(LogTest, info, TXT("Creating render pass..."));
		TSharedPtr<FRayTracedTrianglePass> RTPass =
			std::make_shared<FRayTracedTrianglePass>(DeviceManager.get());
		if (!RTPass->Initialize(NvrhiDevice, FirstFB, FString(TXT("Ray Traced Triangle Test"))))
		{
			throw std::runtime_error("Failed to initialize FRayTracedTrianglePass");
		}

		DeviceManager->AddRenderPassToBack(RTPass);

		HLVM_LOG(LogTest, info, TXT("Starting render loop..."));

		std::thread([&]() {
			FTimer Timer;
			while (Timer.MarkSec() < 1.0)
			{
			}
			DeviceManager->StopMessageLoop();
		}).detach();

		DeviceManager->RunMessageLoop();

		RTPass->Shutdown();

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

RECORD_BOOL(test_RayTracedTriangle)
{
	HLVM_LOG(LogTest, warning, TXT("Vulkan renderer not enabled - skipping test"));
	return true;
}

#endif // HLVM_VULKAN_RENDERER
