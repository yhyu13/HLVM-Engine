/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 *
 * TestCameraControls - Interactive Camera Control Sample with Sponza Scene
 *
 * Demonstrates interactive camera movement with:
 *   - WASD: forward/backward/strafe
 *   - Z/C: pan up/down along camera up vector
 *   - Mouse RMB + drag: rotate (yaw/pitch)
 *   - Arrow keys: rotate (yaw/pitch)
 *   - Escape: release mouse capture
 *
 * Renders the full Sponza deferred pipeline (GBuffer, SSAO, SSR, Lighting, Bloom, Tonemap).
 */

#include "Test.h"
#include "Renderer/DeviceManager.h"
#include "Renderer/Common/FCommonRenderPasses.h"
#include "Renderer/Deferred/FDeferredFrameRenderer.h"
#include "Renderer/Scene3D/FSceneGPUData.h"
#include "Renderer/SceneGraph/PerspectiveCameraNode.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

DECLARE_LOG_CATEGORY(LogTest)

#if HLVM_VULKAN_RENDERER

// =============================================================================
// CONFIGURATION
// =============================================================================

static const char*    WINDOW_TITLE = "Camera Controls - Sponza";
static const uint32_t WINDOW_WIDTH = 800;
static const uint32_t WINDOW_HEIGHT = 600;

// =============================================================================
// CAMERA CONTROL PASS
// =============================================================================

class FCameraControlPass : public IRenderPass
{
public:
    using IRenderPass::IRenderPass;

    bool Initialize(nvrhi::IDevice* Device, nvrhi::IFramebuffer* Framebuffer)
    {
        (void)Framebuffer;
        NvrhiDevice = Device;

        // Initialize deferred frame renderer
        const auto ShaderDataDir = FString::Format(
            TXT("{}/Engine/Source/Runtime/Test/TestSponzaDeferred_Data"),
            *GProjectRoot);

        HLVM_LOG(LogTest, info, TXT("Initializing deferred frame renderer..."));
        if (!DeferredRenderer.Initialize(NvrhiDevice, ShaderDataDir))
        {
            HLVM_LOG(LogTest, err, TXT("Failed to initialize deferred frame renderer"));
            return false;
        }
        HLVM_LOG(LogTest, info, TXT("Deferred frame renderer initialized successfully"));

        // Load Sponza scene
        HLVM_LOG(LogTest, info, TXT("Loading Sponza scene..."));
        const FString GitRoot = FString::Format(TXT("{}"), *GProjectRoot);
        const FPath ScenePath = FPath(FString::Format(
            TXT("{}/Samples/Assets/Sponza/glTF/Sponza.gltf"), *GitRoot));

        if (!SceneGPUData.Initialize(NvrhiDevice, ScenePath))
        {
            HLVM_LOG(LogTest, err, TXT("Failed to initialize scene GPU data"));
            return false;
        }
        HLVM_LOG(LogTest, info, TXT("Sponza scene loaded successfully"));

        // Create camera
        Camera = TUniquePtr<FPerspectiveCameraNode>(new FPerspectiveCameraNode());
        Camera->SetPosition(FVec3(0.0f, 2.0f, 5.0f));
        Camera->SetFovY(glm::radians(60.0f));
        Camera->SetAspectRatio(static_cast<float>(WINDOW_WIDTH) / static_cast<float>(WINDOW_HEIGHT));
        Camera->SetNearPlane(0.1f);
        Camera->SetFarPlane(1000.0f);
        Camera->UpdateWorldTransform();

        // Start in center of Sponza, looking at the floor/walls
        auto DrawData = SceneGPUData.BuildDrawData();
        Camera->SetPosition(FVec3(DrawData.SceneCenter.x, 2.0f, DrawData.SceneCenter.z));
        Camera->UpdateWorldTransform();

        return true;
    }

    void Shutdown()
    {
        DeferredRenderer.Shutdown();
        SceneGPUData.Shutdown();
        Camera = nullptr;
        NvrhiDevice = nullptr;
    }

    // -------------------------------------------------------------------------
    // Input Handling
    // -------------------------------------------------------------------------

    virtual bool KeyboardUpdate(int key, int scancode, int action, int mods) override
    {
        (void)scancode;
        (void)mods;

        bool bPressed = (action == GLFW_PRESS);
        bool bReleased = (action == GLFW_RELEASE);

        if (bPressed || bReleased)
        {
            switch (key)
            {
                case GLFW_KEY_W: bW = bPressed; return true;
                case GLFW_KEY_A: bA = bPressed; return true;
                case GLFW_KEY_S: bS = bPressed; return true;
                case GLFW_KEY_D: bD = bPressed; return true;
                case GLFW_KEY_Z: bZ = bPressed; return true;
                case GLFW_KEY_C: bC = bPressed; return true;
                case GLFW_KEY_UP:    bArrowUp    = bPressed; return true;
                case GLFW_KEY_DOWN:  bArrowDown  = bPressed; return true;
                case GLFW_KEY_LEFT:  bArrowLeft  = bPressed; return true;
                case GLFW_KEY_RIGHT: bArrowRight = bPressed; return true;
                case GLFW_KEY_ESCAPE:
                    if (bPressed)
                    {
                        bMouseCaptured = false;
                        if (auto* DM = GetDeviceManager())
                        {
                            if (GLFWwindow* Window = static_cast<GLFWwindow*>(DM->GetGLFWWindow()))
                            {
                                glfwSetInputMode(Window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                            }
                        }
                    }
                    return true;
            }
        }
        return false;
    }

    virtual bool MouseButtonUpdate(int button, int action, int mods) override
    {
        (void)mods;
        if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
        {
            bMouseCaptured = true;
            if (auto* DM = GetDeviceManager())
            {
                if (GLFWwindow* Window = static_cast<GLFWwindow*>(DM->GetGLFWWindow()))
                {
                    glfwSetInputMode(Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                    glfwGetCursorPos(Window, &LastMouseX, &LastMouseY);
                }
            }
            return true;
        }
        return false;
    }

    virtual bool MousePosUpdate(double xpos, double ypos) override
    {
        if (!bMouseCaptured)
        {
            LastMouseX = xpos;
            LastMouseY = ypos;
            return false;
        }

        double dx = xpos - LastMouseX;
        double dy = ypos - LastMouseY;

        Yaw -= static_cast<float>(dx) * MouseSensitivity;
        Pitch -= static_cast<float>(dy) * MouseSensitivity;

        const float PitchLimit = glm::pi<float>() * 0.5f - 0.01f;
        Pitch = glm::clamp(Pitch, -PitchLimit, PitchLimit);

        Camera->SetRotation(FVec3(Pitch, Yaw, 0.0f));

        LastMouseX = xpos;
        LastMouseY = ypos;
        return true;
    }

    // -------------------------------------------------------------------------
    // Animation & Camera Update
    // -------------------------------------------------------------------------

    virtual void Animate(float deltaTime) override
    {
        float SimTime = static_cast<float>(SimTimer.MarkSec());

        // Input simulation for automated demo
        if (bSimulateInput && SimTime < 10.0f)
        {
            if (SimTime < 2.0f)
            {
                bW = true; bD = true;
                Yaw -= 0.3f * deltaTime;
                Pitch += 0.1f * deltaTime;
            }
            else if (SimTime < 4.0f)
            {
                bW = false; bD = false;
                bA = true; bZ = true;
                Yaw += 0.5f * deltaTime;
                Pitch -= 0.15f * deltaTime;
            }
            else if (SimTime < 6.0f)
            {
                bA = false; bZ = false;
                bS = true; bD = true; bC = true;
                Pitch -= 0.4f * deltaTime;
            }
            else if (SimTime < 8.0f)
            {
                bS = false; bD = false; bC = false;
                bW = true; bZ = true;
                Yaw -= 0.6f * deltaTime;
                Pitch += 0.3f * deltaTime;
            }
            else
            {
                bW = false; bZ = false;
                bS = true;
                Yaw += 0.2f * deltaTime;
            }

            const float PitchLimit = glm::pi<float>() * 0.5f - 0.01f;
            Pitch = glm::clamp(Pitch, -PitchLimit, PitchLimit);
            Camera->SetRotation(FVec3(Pitch, Yaw, 0.0f));
        }

        // Arrow key rotation (when mouse is not captured and not simulating)
        if (!bMouseCaptured && !bSimulateInput)
        {
            if (bArrowLeft)  Yaw += ArrowKeyRotationSpeed * deltaTime;
            if (bArrowRight) Yaw -= ArrowKeyRotationSpeed * deltaTime;
            if (bArrowUp)    Pitch += ArrowKeyRotationSpeed * deltaTime;
            if (bArrowDown)  Pitch -= ArrowKeyRotationSpeed * deltaTime;

            const float PitchLimit = glm::pi<float>() * 0.5f - 0.01f;
            Pitch = glm::clamp(Pitch, -PitchLimit, PitchLimit);
            Camera->SetRotation(FVec3(Pitch, Yaw, 0.0f));
        }

        // Movement
        FVec3 Forward = Camera->ComputeForward();
        FVec3 Right = Camera->ComputeRight();
        FVec3 Up = Camera->ComputeUp();

        float moveForward = (bW ? 1.0f : 0.0f) - (bS ? 1.0f : 0.0f);
        float moveRight   = (bD ? 1.0f : 0.0f) - (bA ? 1.0f : 0.0f);
        float moveUp      = (bZ ? 1.0f : 0.0f) - (bC ? 1.0f : 0.0f);

        FVec3 DeltaPos = (Forward * moveForward + Right * moveRight + Up * moveUp) * MovementSpeed * deltaTime;
        Camera->SetPosition(Camera->GetPosition() + DeltaPos);

        Camera->UpdateWorldTransform();

        // Update FPS counter
        FrameCount++;
        FPSUpdateTimer += deltaTime;
        if (FPSUpdateTimer >= 1.0f)
        {
            FString Title = FString::Format(TXT("Camera Controls - FPS: {:.1f}"), float(FrameCount) / FPSUpdateTimer);
            if (auto* DM = GetDeviceManager())
            {
                DM->SetWindowTitle(Title);
            }
            FPSUpdateTimer = 0.0f;
            FrameCount = 0;
        }
    }

    // -------------------------------------------------------------------------
    // Rendering
    // -------------------------------------------------------------------------

    virtual void Render(nvrhi::IFramebuffer* Framebuffer) override
    {
        if (!NvrhiDevice || !Framebuffer || !Camera)
            return;

        auto DrawData = SceneGPUData.BuildDrawData();

        // Build view data from camera
        FDeferredFrameRenderer::FViewData ViewData;
        ViewData.ViewMatrix = Camera->GetViewMatrix();
        ViewData.ProjMatrix = Camera->GetProjectionMatrix();
        ViewData.CameraPos = Camera->GetPosition();
        ViewData.ModelMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(2.0f));
        ViewData.SceneRadius = DrawData.SceneRadius;
        ViewData.PrevViewMatrix = PrevViewMatrix;
        ViewData.PrevProjMatrix = PrevProjMatrix;

        // Store current matrices for next frame's TAA
        PrevViewMatrix = ViewData.ViewMatrix;
        PrevProjMatrix = ViewData.ProjMatrix;

        // Light matrix (same as TestSponzaDeferred)
        float     localSceneRadius = glm::length(glm::vec3(
            DrawData.BBoxMax.x - DrawData.BBoxMin.x,
            DrawData.BBoxMax.y - DrawData.BBoxMin.y,
            DrawData.BBoxMax.z - DrawData.BBoxMin.z)) * 0.5f;
        float     worldSceneRadius = localSceneRadius * 2.0f;
        glm::vec3 WorldSceneCenter = DrawData.SceneCenter * 2.0f;
        glm::vec3 LightDirVec(0.577f, 0.577f, 0.577f);
        glm::vec3 LightPos = WorldSceneCenter + glm::normalize(LightDirVec) * worldSceneRadius * 2.0f;
        glm::mat4 LightView = glm::lookAtLH(LightPos, WorldSceneCenter, glm::vec3(0, 1, 0));
        float     orthoSize = worldSceneRadius * 1.5f;
        glm::mat4 LightProj = glm::orthoLH_ZO(-orthoSize, orthoSize, -orthoSize, orthoSize, 0.1f, worldSceneRadius * 4.0f);
        ViewData.LightViewProj = LightProj * LightView;

        // Render params
        FDeferredFrameRenderer::FRenderParams Params;
        Params.View = &ViewData;
        Params.GBufferMeshes = DrawData.GBufferItems.data();
        Params.GBufferMeshCount = static_cast<uint32_t>(DrawData.GBufferItems.size());
        Params.ShadowMeshes = DrawData.ShadowItems.data();
        Params.ShadowMeshCount = static_cast<uint32_t>(DrawData.ShadowItems.size());
        Params.OutputFramebuffer = Framebuffer;
        Params.FrameDumper = nullptr;

        nvrhi::CommandListParameters CmdListParams;
        CmdListParams.enableImmediateExecution = false;
        nvrhi::CommandListHandle CmdList = NvrhiDevice->createCommandList(CmdListParams);
        CmdList->open();

        DeferredRenderer.Render(CmdList, Params);

        CmdList->close();
        NvrhiDevice->executeCommandList(CmdList);
        NvrhiDevice->waitForIdle();
    }

    virtual void BackBufferResized(const uint32_t width, const uint32_t height, const uint32_t sampleCount) override
    {
        (void)sampleCount;
        if (Camera)
        {
            Camera->SetAspectRatio(static_cast<float>(width) / static_cast<float>(height));
            Camera->UpdateWorldTransform();
        }
    }

private:
    nvrhi::IDevice* NvrhiDevice = nullptr;

    // Scene & Renderer
    FSceneGPUData SceneGPUData;
    FDeferredFrameRenderer DeferredRenderer;

    // Camera
    TUniquePtr<FPerspectiveCameraNode> Camera;

    // Input state
    bool bW = false, bA = false, bS = false, bD = false;
    bool bZ = false, bC = false;
    bool bArrowUp = false, bArrowDown = false, bArrowLeft = false, bArrowRight = false;
    bool bMouseCaptured = false;
    double LastMouseX = 0.0, LastMouseY = 0.0;
    float Yaw = 0.0f, Pitch = 0.0f;
    float MovementSpeed = 5.0f;
    float MouseSensitivity = 0.003f;
    float ArrowKeyRotationSpeed = 1.5f;

    // Simulation
    FTimer SimTimer;
    bool bSimulateInput = true;
    uint32_t FrameCount = 0;
    float FPSUpdateTimer = 0.0f;

    // TAA previous frame matrices
    glm::mat4 PrevViewMatrix = glm::mat4(1.0f);
    glm::mat4 PrevProjMatrix = glm::mat4(1.0f);
};

// =============================================================================
// TEST IMPLEMENTATION
// =============================================================================

RECORD_BOOL(test_CameraControls)
{
    HLVM_LOG(LogTest, info, TXT("=== Starting Camera Controls Test ==="));

    try
    {
        IWindow::Properties WindowProps;
        WindowProps.Title = WINDOW_TITLE;
        WindowProps.Extent = { WINDOW_WIDTH, WINDOW_HEIGHT };
        WindowProps.Resizable = true;
        WindowProps.VSync = IWindow::EVsync::Off;

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
        DeviceParams.bEnableDebugRuntime = HLVM_BUILD_DEBUG;
        DeviceParams.bEnableNVRHIValidationLayer = HLVM_BUILD_DEBUG;
        DeviceParams.bEnableRayTracingExtensions = false;

        if (!DeviceManager->CreateWindowDeviceAndSwapChain(WindowProps))
        {
            throw std::runtime_error("Failed to create window, device and swap chain");
        }

        nvrhi::IDevice* NvrhiDevice = DeviceManager->GetDevice();
        nvrhi::IFramebuffer* FirstFB = DeviceManager->GetFramebuffer(0);

        TSharedPtr<FCameraControlPass> Pass = std::make_shared<FCameraControlPass>(DeviceManager.get());
        if (!Pass->Initialize(NvrhiDevice, FirstFB))
        {
            throw std::runtime_error("Failed to initialize FCameraControlPass");
        }

        DeviceManager->AddRenderPassToBack(Pass);

        // Run for 15 seconds then stop
        std::thread([&]() {
            FTimer Timer;
            while (Timer.MarkSec() < 15.0)
            {
            }
            DeviceManager->StopMessageLoop();
        }).detach();

        DeviceManager->RunMessageLoop();

        Pass->Shutdown();

        HLVM_LOG(LogTest, info, TXT("Camera Controls Test completed successfully!"));
        return true;
    }
    catch (const std::exception& e)
    {
        HLVM_LOG(LogTest, critical, TXT("Test failed: {}"), TO_TCHAR_CSTR(e.what()));
        return false;
    }
}

#else // HLVM_VULKAN_RENDERER

RECORD_BOOL(test_CameraControls)
{
    HLVM_LOG(LogTest, warning, TXT("Vulkan renderer not enabled - skipping test"));
    return true;
}

#endif // HLVM_VULKAN_RENDERER
