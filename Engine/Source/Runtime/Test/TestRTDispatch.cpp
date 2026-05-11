/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 *
 * RT Dispatch Test - Diagnostic ray tracing to verify geometry rendering
 */

#include "Test.h"
#include "Renderer/DeviceManager.h"
#include "Renderer/RHI/Object/Buffer.h"
#include "Renderer/Common/FBindingCache.h"
#include "Renderer/Common/FCommonRenderPasses.h"
#include "Renderer/SceneGraph/FNode.h"
#include "Renderer/Scene3D/Scene3DLoader.h"
#include "Renderer/Mesh/StaticMesh.h"
#include "Renderer/RayTracing/BLASBuilder.h"
#include "Renderer/RayTracing/TLASBuilder.h"
#include <nvrhi/utils.h>
#include <Utility/Timer.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

DECLARE_LOG_CATEGORY(LogTest)

#if HLVM_VULKAN_RENDERER

// =============================================================================
// CONFIGURATION
// =============================================================================

static const char*	  WINDOW_TITLE = "RT Dispatch Test";
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
// FRTShadowsPass - IRenderPass implementation
// =============================================================================

class FRTShadowsPass : public IRenderPass
{
public:
	using IRenderPass::IRenderPass;

	bool Initialize(nvrhi::IDevice* Device, nvrhi::IFramebuffer* Framebuffer, const FString& InWindowTitle)
	{
		NvrhiDevice = Device;
		FBInfo = Framebuffer->getFramebufferInfo();
		WindowTitle = InWindowTitle;

		BindingCache.SetDevice(NvrhiDevice);

		const auto DataDir = FString::Format(
			TXT("{}/../../Test/TestRTDispatch_Data"),
			*GExecutablePath);

		auto RTShadowsCode = ReadBinaryFile(
			FPath::Combine(DataDir, TXT("RTShadows.spv")).string());

		ShaderLibrary = NvrhiDevice->createShaderLibrary(
			RTShadowsCode.data(), RTShadowsCode.size());
		if (!ShaderLibrary)
		{
			HLVM_LOG(LogTest, err, TXT("Failed to load RT shadow shader library"));
			return false;
		}

		// Get ray tracing shaders from library
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

		// Load Sponza scene
		HLVM_LOG(LogTest, info, TXT("Loading Sponza scene..."));
		const FString				  GitRoot = FString::Format(TXT("{}/../../../../.."), *GExecutablePath);
		const FPath					  ScenePath = FPath(FString::Format(TXT("{}{}"), *GitRoot, TXT("/Samples/Assets/sponza/Sponza01.gltf")));
		std::shared_ptr<FScene3DNode> Scene = FScene3DLoader::LoadFromFile(ScenePath);
		if (!Scene || Scene->IsEmpty())
		{
			HLVM_LOG(LogTest, err, TXT("Failed to load Sponza scene"));
			return false;
		}
		auto StaticMeshes = Scene->GetAllStaticMesh();
		HLVM_LOG(LogTest, info, TXT("Loaded scene with {} meshes"), StaticMeshes.size());

		// Build BLAS for each mesh
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

		// Build TLAS with all mesh instances
		if (!TLASBuilder.Initialize(NvrhiDevice, static_cast<uint32_t>(StaticMeshes.size())))
		{
			HLVM_LOG(LogTest, err, TXT("Failed to initialize TLAS builder"));
			return false;
		}
		HLVM_LOG(LogTest, info, TXT("Building TLAS with {} instances..."), StaticMeshes.size());

		for (uint32_t i = 0; i < static_cast<uint32_t>(StaticMeshes.size()); ++i)
		{
			FTLASBuilder::FInstanceDesc InstanceDesc;
			InstanceDesc.BottomLevelAS = MeshBLASHandles[i];
			InstanceDesc.InstanceMask = 1;
			InstanceDesc.InstanceFlags = nvrhi::rt::InstanceFlags::TriangleFrontCounterclockwise;
			// Apply scene transform - scale down Sponza (it's large)
			glm::mat4 MeshTransform = glm::scale(glm::mat4(1.0f), glm::vec3(0.01f));
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

		InitCmdList->close();
		NvrhiDevice->executeCommandList(InitCmdList);

		HLVM_LOG(LogTest, info, TXT("Acceleration structures built successfully"));

		// Create ray tracing binding layout - following TestRayTracedTriangle pattern
		// Shader uses: push constants for Camera, t0 for TLAS, u1 for Output
		// NVRHI: PushConstants slot, size
		nvrhi::BindingLayoutDesc RayTracingLayoutDesc;
		RayTracingLayoutDesc.visibility = nvrhi::ShaderType::All;
		// Override UAV offset to 0 to match test - this makes u1 -> 1
		RayTracingLayoutDesc.bindingOffsets.setUnorderedAccessViewOffset(0);
		RayTracingLayoutDesc.bindings = {
			nvrhi::BindingLayoutItem::PushConstants(0, sizeof(float) * 12), // 3 float4 registers
			nvrhi::BindingLayoutItem::RayTracingAccelStruct(0),				// t0 -> Vulkan 0
			nvrhi::BindingLayoutItem::Texture_UAV(1)						// u1 -> Vulkan 1
		};
		BindingLayout = NvrhiDevice->createBindingLayout(RayTracingLayoutDesc);
		if (!BindingLayout)
		{
			HLVM_LOG(LogTest, err, TXT("Failed to create binding layout"));
			return false;
		}

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
		PipelineDesc.maxPayloadSize = sizeof(bool) + sizeof(float) + 16; // missed + hitT + padding

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

		// Create command list
		CommandList = NvrhiDevice->createCommandList();
		if (!CommandList)
		{
			HLVM_LOG(LogTest, err, TXT("Failed to create command list"));
			return false;
		}

		// Note: Push constants are used instead of a buffer - no CameraBuffer needed

		HLVM_LOG(LogTest, info, TXT("FRTShadowsPass initialized successfully"));
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
		RenderTarget = nullptr;
		RayTracingBindingSet = nullptr;
		ShaderLibrary = nullptr;
		RayGenShader = nullptr;
		ClosestHitShader = nullptr;
		MissShader = nullptr;
		MeshBLASHandles.clear();
		TLASBuilder.Reset();

		HLVM_LOG(LogTest, info, TXT("FRTShadowsPass shutdown complete"));
	}

	virtual void Animate(float fElapsedTimeSeconds) override
	{
		FrameCount++;
		FPSUpdateTimer += fElapsedTimeSeconds;
		float FPS = float(FrameCount) / FPSUpdateTimer;
		if (FPSUpdateTimer >= 1.0f)
		{
			WindowTitle = FString::Format(TXT("RT Dispatch Test - FPS: {:.1f}"), FPS);

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
			TextureDesc.format = nvrhi::Format::RGBA16_FLOAT;
			TextureDesc.isUAV = true;
			TextureDesc.isRenderTarget = false;
			TextureDesc.initialState = nvrhi::ResourceStates::UnorderedAccess;
			TextureDesc.keepInitialState = true;
			TextureDesc.debugName = "ShadowRT";

			RenderTarget = NvrhiDevice->createTexture(TextureDesc);

			BindingCache.Clear();

			// Recreate binding set with new render target
			nvrhi::BindingSetDesc BindingSetDesc;
			BindingSetDesc.bindings = {
				nvrhi::BindingSetItem::PushConstants(0, sizeof(float) * 12),
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

			// Update push constants AFTER setRayTracingState
			float CameraData[12] = {
				0.0f, 5.0f, 10.0f, 0.0f, // CameraPos
				1.0f, 0.0f, 0.0f, 0.0f,	 // CameraRight (unused)
				0.0f, 1.0f, 0.0f, 0.0f	 // CameraUp (unused)
			};
			CommandList->setPushConstants(CameraData, sizeof(CameraData));

			nvrhi::rt::DispatchRaysArguments Args;
			Args.width = CurrentFBInfo.width;
			Args.height = CurrentFBInfo.height;
			CommandList->dispatchRays(Args);
		}

		// Blit pass
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

	nvrhi::rt::AccelStructHandle		  TopLevelAS;
	TVector<nvrhi::rt::AccelStructHandle> MeshBLASHandles;
	FTLASBuilder						  TLASBuilder;

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

RECORD_BOOL(test_RTDispatch)
{
	HLVM_LOG(LogTest, info, TXT("Starting RT Dispatch Test..."));

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
		TSharedPtr<FRTShadowsPass> RTPass =
			std::make_shared<FRTShadowsPass>(DeviceManager.get());
		if (!RTPass->Initialize(NvrhiDevice, FirstFB, FString(TXT("RT Dispatch Test"))))
		{
			throw std::runtime_error("Failed to initialize FRTShadowsPass");
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

RECORD_BOOL(test_RTDispatch)
{
	HLVM_LOG(LogTest, warning, TXT("Vulkan renderer not enabled - skipping test"));
	return true;
}

#endif // HLVM_VULKAN_RENDERER
