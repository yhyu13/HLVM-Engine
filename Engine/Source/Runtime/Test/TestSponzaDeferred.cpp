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
#include "Renderer/Common/FCommonRenderPasses.h"
#include "Image/FRenderPassDumper.h"
#include "Renderer/Deferred/FDeferredFrameRenderer.h"
#include "Renderer/Scene3D/FSceneGPUData.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

DECLARE_LOG_CATEGORY(LogTest)

#if HLVM_VULKAN_RENDERER

// =============================================================================
// CONFIGURATION
// =============================================================================

static const char*	  WINDOW_TITLE = "Sponza Deferred - Scene Test";
static const uint32_t WINDOW_WIDTH = 800;
static const uint32_t WINDOW_HEIGHT = 600;

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

		const auto ShaderDataDir = FString::Format(
			TXT("{}/Engine/Source/Runtime/Test/TestSponzaDeferred_Data"),
			*GProjectRoot);

		// =====================================================================
		// Initialize deferred frame renderer
		// =====================================================================
		HLVM_LOG(LogTest, info, TXT("Initializing deferred frame renderer..."));
		if (!DeferredRenderer.Initialize(NvrhiDevice, ShaderDataDir))
		{
			HLVM_LOG(LogTest, err, TXT("Failed to initialize deferred frame renderer"));
			return false;
		}
		HLVM_LOG(LogTest, info, TXT("Deferred frame renderer initialized successfully"));

		// =====================================================================
		// Load scene GPU data
		// =====================================================================
		HLVM_LOG(LogTest, info, TXT("Loading scene GPU data..."));
		const FString GitRoot = FString::Format(TXT("{}"), *GProjectRoot);
		const FPath ScenePath = FPath(FString::Format(
			TXT("{}/Samples/Assets/Sponza/glTF/Sponza.gltf"), *GitRoot));

		if (!SceneGPUData.Initialize(NvrhiDevice, ScenePath))
		{
			HLVM_LOG(LogTest, err, TXT("Failed to initialize scene GPU data"));
			return false;
		}
		HLVM_LOG(LogTest, info, TXT("Scene GPU data initialized successfully"));

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

		DeferredRenderer.Shutdown();
		SceneGPUData.Shutdown();
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

		auto DrawData = SceneGPUData.BuildDrawData();

		// =====================================================================
		// Camera orbit around scene center - low angle for visible SSR
		// =====================================================================
		float angle = static_cast<float>(FrameCount) * 0.01f;
		float camX = DrawData.SceneCenter.x + sinf(angle) * 5.0f;
		float camZ = DrawData.SceneCenter.z + cosf(angle) * 5.0f;
		float camY = 2.0f;

		glm::vec3 target = DrawData.SceneCenter + glm::vec3(0.0f, 2.0f, 0.0f);
		glm::mat4 view = glm::lookAtLH(glm::vec3(camX, camY, camZ), target, glm::vec3(0.f, 1.f, 0.f));
		float	  aspectRatio = float(CurrentFBInfo.width) / float(CurrentFBInfo.height);
		glm::mat4 proj = glm::perspectiveLH_ZO(glm::radians(90.0f), aspectRatio, 0.1f, 1000.0f);

		// Light matrix
		float	  localSceneRadius = glm::length(glm::vec3(
			DrawData.BBoxMax.x - DrawData.BBoxMin.x,
			DrawData.BBoxMax.y - DrawData.BBoxMin.y,
			DrawData.BBoxMax.z - DrawData.BBoxMin.z)) * 0.5f;
		float	  worldSceneRadius = localSceneRadius * 2.0f;
		glm::vec3 WorldSceneCenter = DrawData.SceneCenter * 2.0f;
		glm::vec3 LightDirVec(0.577f, 0.577f, 0.577f);
		glm::vec3 LightPos = WorldSceneCenter + glm::normalize(LightDirVec) * worldSceneRadius * 2.0f;
		glm::mat4 LightView = glm::lookAtLH(LightPos, WorldSceneCenter, glm::vec3(0, 1, 0));
		float	  orthoSize = worldSceneRadius * 1.5f;
		glm::mat4 LightProj = glm::orthoLH_ZO(-orthoSize, orthoSize, -orthoSize, orthoSize, 0.1f, worldSceneRadius * 4.0f);
		glm::mat4 LightViewProj = LightProj * LightView;

		// Build view data
		FDeferredFrameRenderer::FViewData ViewData;
		ViewData.ViewMatrix = view;
		ViewData.ProjMatrix = proj;
		ViewData.CameraPos = glm::vec3(camX, camY, camZ);
		ViewData.LightViewProj = LightViewProj;
		ViewData.ModelMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(2.0f));
		ViewData.SceneRadius = DrawData.SceneRadius;
		ViewData.PrevViewMatrix = PrevViewMatrix;
		ViewData.PrevProjMatrix = PrevProjMatrix;

		// Store current matrices for next frame's TAA
		PrevViewMatrix = view;
		PrevProjMatrix = proj;

		// Render
		FDeferredFrameRenderer::FRenderParams Params;
		Params.View = &ViewData;
		Params.GBufferMeshes = DrawData.GBufferItems.data();
		Params.GBufferMeshCount = static_cast<uint32_t>(DrawData.GBufferItems.size());
		Params.ShadowMeshes = DrawData.ShadowItems.data();
		Params.ShadowMeshCount = static_cast<uint32_t>(DrawData.ShadowItems.size());
		Params.OutputFramebuffer = Framebuffer;
		Params.FrameDumper = FrameDumper.IsEnabled() ? &FrameDumper : nullptr;

		nvrhi::CommandListParameters CmdListParams;
		CmdListParams.enableImmediateExecution = false;
		nvrhi::CommandListHandle CmdList = NvrhiDevice->createCommandList(CmdListParams);
		CmdList->open();

		DeferredRenderer.Render(CmdList, Params);

		CmdList->close();
		NvrhiDevice->executeCommandList(CmdList);
		NvrhiDevice->waitForIdle();

		// =====================================================================
		// Frame dump readback
		// =====================================================================
		if (FrameDumper.IsEnabled())
		{
			int frameNum = FrameDumper.GetCurrentFrame();
			if (FrameDumper.ReadbackAndSave())
			{
				HLVM_LOG(LogTest, info, TXT("Dumped frame {}"), frameNum);
				FString refPath = FString::Format(
					TXT("{}/../../Test/TestSponzaDeferred_Data/Reference/frame_{:04d}.png"),
					*GExecutablePath, frameNum);
				FrameDumper.CompareAgainstReference(refPath, 0.01f);
			}
			if (FrameDumper.IsLastFrame())
			{
				return;
			}
		}
	}

	virtual void BackBufferResizing() override
	{
		// Renderer auto-resizes on next Render() call
	}

private:
	nvrhi::IDevice*		   NvrhiDevice = nullptr;
	nvrhi::FramebufferInfoEx FBInfo;
	FString				   WindowTitle;

	FSceneGPUData SceneGPUData;
	FDeferredFrameRenderer DeferredRenderer;
	FRenderPassDumper FrameDumper;

	uint32_t FrameCount = 0;
	float	 FPSUpdateTimer = 0.0f;
	glm::mat4 PrevViewMatrix = glm::mat4(1.0f);
	glm::mat4 PrevProjMatrix = glm::mat4(1.0f);
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
