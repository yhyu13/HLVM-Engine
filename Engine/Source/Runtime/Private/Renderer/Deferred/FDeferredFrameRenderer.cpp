#include "Renderer/Deferred/FDeferredFrameRenderer.h"
#include "Renderer/Common/FCommonRenderPasses.h"
#include "Image/FRenderPassDumper.h"
#include "Core/Log.h"
#include "Utility/CVar/CVarMacros.h"
#include <glm/gtc/type_ptr.hpp>

DECLARE_LOG_CATEGORY(LogRenderer)

AUTO_CVAR_BOOL(r_SSAO, true,
    "Enable SSAO (HBAO)", EConsoleVariableFlag::Saved)
AUTO_CVAR_FLOAT(r_SSAO_RadiusScale, 0.05f,
    "SSAO sample radius as fraction of scene radius", EConsoleVariableFlag::Saved)

AUTO_CVAR_BOOL(r_SSR, true,
    "Enable screen-space reflections", EConsoleVariableFlag::Saved)
AUTO_CVAR_INT(r_SSR_MaxSteps, 32,
    "SSR max ray march steps", EConsoleVariableFlag::Saved)
AUTO_CVAR_FLOAT(r_SSR_StepSize, 5.0f,
    "SSR view-space step size", EConsoleVariableFlag::Saved)
AUTO_CVAR_FLOAT(r_SSR_MaxDistance, 500.0f,
    "SSR max reflection distance", EConsoleVariableFlag::Saved)
AUTO_CVAR_FLOAT(r_SSR_Thickness, 2.0f,
    "SSR surface thickness in view-space units", EConsoleVariableFlag::Saved)

AUTO_CVAR_BOOL(r_TAA, true,
    "Enable temporal anti-aliasing", EConsoleVariableFlag::Saved)
AUTO_CVAR_FLOAT(r_TAA_BlendFactor, 0.1f,
    "TAA history blend factor (0=none, 1=full)", EConsoleVariableFlag::Saved)
AUTO_CVAR_FLOAT(r_TAA_DepthThreshold, 0.1f,
    "TAA disocclusion depth threshold", EConsoleVariableFlag::Saved)

AUTO_CVAR_BOOL(r_MotionBlur, true,
    "Enable camera motion blur", EConsoleVariableFlag::Saved)
AUTO_CVAR_FLOAT(r_MotionBlur_VelocityScale, 1.0f,
    "Motion blur strength multiplier", EConsoleVariableFlag::Saved)
AUTO_CVAR_FLOAT(r_MotionBlur_MinVelocity, 0.5f,
    "Min pixel velocity to trigger motion blur", EConsoleVariableFlag::Saved)

AUTO_CVAR_BOOL(r_DOF, true,
    "Enable depth of field", EConsoleVariableFlag::Saved)
AUTO_CVAR_FLOAT(r_DOF_FocalDepth, 0.98f,
    "Focal plane depth [0,1] (tune for your projection)", EConsoleVariableFlag::Saved)
AUTO_CVAR_FLOAT(r_DOF_Aperture, 1.0f,
    "DOF aperture size (blur strength multiplier)", EConsoleVariableFlag::Saved)
AUTO_CVAR_FLOAT(r_DOF_DepthScale, 500.0f,
    "Depth-to-CoC scale factor (tuned for perspective depth)", EConsoleVariableFlag::Saved)
AUTO_CVAR_FLOAT(r_DOF_MaxBlurRadius, 10.0f,
    "Max blur radius in pixels", EConsoleVariableFlag::Saved)

AUTO_CVAR_BOOL(r_LensEffects, true,
    "Enable lens effects (CA + vignette + grain)", EConsoleVariableFlag::Saved)
AUTO_CVAR_FLOAT(r_CA_Amount, 2.0f,
    "Chromatic aberration pixel offset at corners", EConsoleVariableFlag::Saved)
AUTO_CVAR_FLOAT(r_Vignette_Intensity, 0.8f,
    "Vignette darkness (0=none, 2=heavy)", EConsoleVariableFlag::Saved)
AUTO_CVAR_FLOAT(r_Grain_Intensity, 0.03f,
    "Film grain intensity", EConsoleVariableFlag::Saved)

AUTO_CVAR_BOOL(r_Bloom, true,
    "Enable bloom post-process", EConsoleVariableFlag::Saved)

AUTO_CVAR_BOOL(r_ExposureAdaptation, true,
    "Enable automatic exposure adaptation", EConsoleVariableFlag::Saved)
AUTO_CVAR_FLOAT(r_Exposure_KeyValue, 0.18f,
    "Target middle gray (Reinhard key value)", EConsoleVariableFlag::Saved)
AUTO_CVAR_FLOAT(r_Exposure_AdaptationSpeed, 0.05f,
    "Exposure adaptation speed per frame", EConsoleVariableFlag::Saved)

AUTO_CVAR_BOOL(r_ContactShadows, true,
    "Enable screen-space contact shadows", EConsoleVariableFlag::Saved)
AUTO_CVAR_FLOAT(r_ContactShadows_MaxDistance, 1.5f,
    "Contact shadow max ray distance", EConsoleVariableFlag::Saved)
AUTO_CVAR_INT(r_ContactShadows_StepCount, 16,
    "Contact shadow ray march steps", EConsoleVariableFlag::Saved)
AUTO_CVAR_FLOAT(r_ContactShadows_Thickness, 0.05f,
    "Contact shadow depth bias", EConsoleVariableFlag::Saved)

AUTO_CVAR_BOOL(r_ShaderHotReload, false,
    "Enable automatic shader hot-reload polling", EConsoleVariableFlag::Saved)

bool FDeferredFrameRenderer::Initialize(nvrhi::IDevice* InDevice, const FString& InShaderDataDir)
{
    if (bIsInitialized)
    {
        Shutdown();
    }

    Device = InDevice;
    ShaderDataDir = InShaderDataDir;
    BindingCache.SetDevice(Device);

    // Initialize sub-passes
    if (!ShadowPass.Initialize(Device, ShaderDataDir, FShadowMapPass::FDesc()))
    {
        HLVM_LOG(LogRenderer, err, TXT("FDeferredFrameRenderer: Failed to initialize shadow pass"));
        return false;
    }

    const FString CommonShaderDir = FString::Format(
        TXT("{}/Engine/Source/Runtime/Shader"),
        *GProjectRoot);

    if (!HBAOPass.Initialize(Device, CommonShaderDir))
    {
        HLVM_LOG(LogRenderer, err, TXT("FDeferredFrameRenderer: Failed to initialize HBAO pass"));
        return false;
    }

    if (!BilateralBlurPass.Initialize(Device, CommonShaderDir))
    {
        HLVM_LOG(LogRenderer, err, TXT("FDeferredFrameRenderer: Failed to initialize bilateral blur pass"));
        return false;
    }

    if (!SSRPass.Initialize(Device, ShaderDataDir))
    {
        HLVM_LOG(LogRenderer, err, TXT("FDeferredFrameRenderer: Failed to initialize SSR pass"));
        return false;
    }

    if (!LightingPass.Initialize(Device, ShaderDataDir))
    {
        HLVM_LOG(LogRenderer, err, TXT("FDeferredFrameRenderer: Failed to initialize lighting pass"));
        return false;
    }

    if (!BloomPass.Initialize(Device, ShaderDataDir))
    {
        HLVM_LOG(LogRenderer, err, TXT("FDeferredFrameRenderer: Failed to initialize bloom pass"));
        return false;
    }

    if (!ContactShadowsPass.Initialize(Device, ShaderDataDir))
    {
        HLVM_LOG(LogRenderer, err, TXT("FDeferredFrameRenderer: Failed to initialize contact shadows pass"));
        return false;
    }

    if (!ToneMapPass.Initialize(Device, ShaderDataDir))
    {
        HLVM_LOG(LogRenderer, err, TXT("FDeferredFrameRenderer: Failed to initialize tone mapping pass"));
        return false;
    }

    HLVM_LOG(LogRenderer, info, TXT("FDeferredFrameRenderer initialized successfully"));
    Profiler.Initialize(Device);

    FShaderHotReloader::Get().Register(this);
    bHotReloadRegistered = true;

    bIsInitialized = true;
    bTAAInitialized = false;
    bMotionBlurInitialized = false;
    bDOFInitialized = false;
    bLensEffectsInitialized = false;
    bExposureAdaptationInitialized = false;
    return true;
}

void FDeferredFrameRenderer::Shutdown()
{
    if (bHotReloadRegistered)
    {
        FShaderHotReloader::Get().Unregister(this);
        bHotReloadRegistered = false;
    }

    Profiler.Shutdown();
    GBufferPass.Shutdown();
    ShadowPass.Shutdown();
    HBAOPass.Shutdown();
    BilateralBlurPass.Shutdown();
    SSRPass.Shutdown();
    LightingPass.Shutdown();
    TAAPass.Shutdown();
    MotionBlurPass.Shutdown();
    DOFPass.Shutdown();
    BloomPass.Shutdown();
    ToneMapPass.Shutdown();
    LensEffectsPass.Shutdown();
    ExposurePass.Shutdown();
    ContactShadowsPass.Shutdown();

    HDRTexture = nullptr;
    SDRTexture = nullptr;
    SSAOTexture = nullptr;
    SSAOBlurTexture = nullptr;
    BloomHalfResTexture = nullptr;
    BloomBlurTempTexture = nullptr;
    BloomTexture = nullptr;
    SSRTexture = nullptr;
    TAAOutputTexture = nullptr;
    TAAHistoryTexture = nullptr;
    MotionBlurTexture = nullptr;
    DOFTexture = nullptr;
    LensEffectsTexture = nullptr;
    AdaptedLuminanceTexture = nullptr;
    ContactShadowTexture = nullptr;

    BindingCache.Clear();
    Device = nullptr;
    CurrentWidth = 0;
    CurrentHeight = 0;
    bIsInitialized = false;
    bTAAInitialized = false;
    bMotionBlurInitialized = false;
    bDOFInitialized = false;
    bLensEffectsInitialized = false;
}

void FDeferredFrameRenderer::ReloadShaders()
{
    if (!bIsInitialized || !Device)
    {
        return;
    }

    HLVM_LOG(LogRenderer, info, TXT("FDeferredFrameRenderer: Reloading shaders..."));

    // Preserve current state
    FString SavedShaderDataDir = ShaderDataDir;
    nvrhi::IDevice* SavedDevice = Device;

    // Full shutdown + re-initialize
    // Note: this destroys and recreates all intermediate textures;
    // they will be recreated on the next Render() call via ResizeIfNeeded()
    Shutdown();

    if (!Initialize(SavedDevice, SavedShaderDataDir))
    {
        HLVM_LOG(LogRenderer, err, TXT("FDeferredFrameRenderer: Shader reload failed"));
    }
    else
    {
        HLVM_LOG(LogRenderer, info, TXT("FDeferredFrameRenderer: Shader reload completed"));
    }
}

void FDeferredFrameRenderer::UpdateShaderHotReload()
{
    if (CVar_r_ShaderHotReload)
    {
        FShaderHotReloader::Get().Update();
    }
}

void FDeferredFrameRenderer::ResizeIfNeeded(uint32_t Width, uint32_t Height)
{
    if (Width == CurrentWidth && Height == CurrentHeight)
    {
        return;
    }

    CurrentWidth = Width;
    CurrentHeight = Height;

    // Resize GBuffer pass
    GBufferPass.Resize(Width, Height);

    // Recreate intermediate textures
    CreateIntermediateTextures(Width, Height);

    HLVM_LOG(LogRenderer, info, TXT("FDeferredFrameRenderer resized to {}x{}"), Width, Height);
}

void FDeferredFrameRenderer::CreateIntermediateTextures(uint32_t Width, uint32_t Height)
{
    nvrhi::TextureDesc Desc;
    Desc.dimension = nvrhi::TextureDimension::Texture2D;
    Desc.width = Width;
    Desc.height = Height;
    Desc.isRenderTarget = false;
    Desc.isUAV = true;
    Desc.isTypeless = false;
    Desc.initialState = nvrhi::ResourceStates::UnorderedAccess;
    Desc.keepInitialState = true;

    Desc.format = nvrhi::Format::RGBA32_FLOAT;
    Desc.debugName = "HDRTexture";
    HDRTexture = Device->createTexture(Desc);

    Desc.format = nvrhi::Format::RGBA8_UNORM;
    Desc.debugName = "SDRTexture";
    SDRTexture = Device->createTexture(Desc);

    Desc.format = nvrhi::Format::R8_UNORM;
    Desc.debugName = "SSAOTexture";
    SSAOTexture = Device->createTexture(Desc);

    Desc.debugName = "SSAOBlurTexture";
    SSAOBlurTexture = Device->createTexture(Desc);

    // Bloom textures (half-res + full-res)
    Desc.format = nvrhi::Format::R11G11B10_FLOAT;
    Desc.width = std::max(1u, Width / 2);
    Desc.height = std::max(1u, Height / 2);
    Desc.debugName = "BloomHalfRes";
    BloomHalfResTexture = Device->createTexture(Desc);

    Desc.debugName = "BloomBlurTemp";
    BloomBlurTempTexture = Device->createTexture(Desc);

    Desc.width = Width;
    Desc.height = Height;
    Desc.debugName = "BloomTexture";
    BloomTexture = Device->createTexture(Desc);

    // SSR texture (half-res RGBA16_FLOAT)
    Desc.format = nvrhi::Format::RGBA16_FLOAT;
    Desc.width = std::max(1u, Width / 2);
    Desc.height = std::max(1u, Height / 2);
    Desc.debugName = "SSRTexture";
    SSRTexture = Device->createTexture(Desc);

    // TAA textures (full-res RGBA32_FLOAT to match HDRTexture for copy compatibility)
    Desc.format = nvrhi::Format::RGBA32_FLOAT;
    Desc.width = Width;
    Desc.height = Height;
    Desc.debugName = "TAAOutput";
    TAAOutputTexture = Device->createTexture(Desc);
    Desc.debugName = "TAAHistory";
    TAAHistoryTexture = Device->createTexture(Desc);

    // Motion blur texture (full-res RGBA32_FLOAT)
    Desc.debugName = "MotionBlur";
    MotionBlurTexture = Device->createTexture(Desc);

    // DOF texture (full-res RGBA32_FLOAT)
    Desc.debugName = "DOF";
    DOFTexture = Device->createTexture(Desc);

    // Lens effects texture (full-res RGBA8_UNORM)
    Desc.format = nvrhi::Format::RGBA8_UNORM;
    Desc.debugName = "LensEffects";
    LensEffectsTexture = Device->createTexture(Desc);

    // Adapted luminance texture (1x1 R32_FLOAT, persisted across frames)
    Desc.format = nvrhi::Format::R32_FLOAT;
    Desc.width = 1;
    Desc.height = 1;
    Desc.debugName = "AdaptedLuminance";
    AdaptedLuminanceTexture = Device->createTexture(Desc);

    // Contact shadow texture (full-res R8_UNORM)
    Desc.format = nvrhi::Format::R8_UNORM;
    Desc.width = Width;
    Desc.height = Height;
    Desc.debugName = "ContactShadow";
    ContactShadowTexture = Device->createTexture(Desc);

    bTAANeedsHistoryInit = true;
}

void FDeferredFrameRenderer::Render(nvrhi::ICommandList* CmdList, const FRenderParams& Params)
{
    if (!bIsInitialized || !CmdList || !Params.OutputFramebuffer)
    {
        return;
    }

    const auto& OutputFBInfo = Params.OutputFramebuffer->getFramebufferInfo();
    ResizeIfNeeded(OutputFBInfo.width, OutputFBInfo.height);

    // Initialize GBuffer pass on first render (lazy init since we need dimensions)
    if (!GBufferPass.GetDiffuseTexture())
    {
        FGBufferFillPass::FDesc GBufferDesc;
        GBufferDesc.Width = CurrentWidth;
        GBufferDesc.Height = CurrentHeight;
        if (!GBufferPass.Initialize(Device, ShaderDataDir, GBufferDesc))
        {
            HLVM_LOG(LogRenderer, err, TXT("FDeferredFrameRenderer: Failed to initialize GBuffer pass"));
            return;
        }
    }

    // Initialize TAA pass on first render if enabled
    if (CVar_r_TAA && !bTAAInitialized)
    {
        if (!TAAPass.Initialize(Device, ShaderDataDir))
        {
            HLVM_LOG(LogRenderer, err, TXT("FDeferredFrameRenderer: Failed to initialize TAA pass"));
            return;
        }
        bTAAInitialized = true;
    }

    // Initialize motion blur pass on first render if enabled
    if (CVar_r_MotionBlur && !bMotionBlurInitialized)
    {
        if (!MotionBlurPass.Initialize(Device, ShaderDataDir))
        {
            HLVM_LOG(LogRenderer, err, TXT("FDeferredFrameRenderer: Failed to initialize motion blur pass"));
            return;
        }
        bMotionBlurInitialized = true;
    }

    // Initialize DOF pass on first render if enabled
    if (CVar_r_DOF && !bDOFInitialized)
    {
        if (!DOFPass.Initialize(Device, ShaderDataDir))
        {
            HLVM_LOG(LogRenderer, err, TXT("FDeferredFrameRenderer: Failed to initialize DOF pass"));
            return;
        }
        bDOFInitialized = true;
    }

    // Initialize lens effects pass on first render if enabled
    if (CVar_r_LensEffects && !bLensEffectsInitialized)
    {
        if (!LensEffectsPass.Initialize(Device, ShaderDataDir))
        {
            HLVM_LOG(LogRenderer, err, TXT("FDeferredFrameRenderer: Failed to initialize lens effects pass"));
            return;
        }
        bLensEffectsInitialized = true;
    }

    // Initialize exposure adaptation pass on first render if enabled
    if (CVar_r_ExposureAdaptation && !bExposureAdaptationInitialized)
    {
        if (!ExposurePass.Initialize(Device, ShaderDataDir))
        {
            HLVM_LOG(LogRenderer, err, TXT("FDeferredFrameRenderer: Failed to initialize exposure adaptation pass"));
            return;
        }
        bExposureAdaptationInitialized = true;
    }

    // Begin GPU profiling frame
    Profiler.BeginFrame();

    // =====================================================================
    // Shadow Pass
    // =====================================================================
    {
        Profiler.BeginPass(CmdList, TXT("Shadow"));
        FShadowMapPass::FRenderDesc ShadowDesc;
        ShadowDesc.LightViewProj = Params.View->LightViewProj;
        ShadowDesc.ModelMatrix = Params.View->ModelMatrix;
        ShadowDesc.MeshDrawItems = Params.ShadowMeshes;
        ShadowDesc.MeshDrawItemCount = Params.ShadowMeshCount;

        ShadowPass.Render(CmdList, ShadowDesc);
        Profiler.EndPass(CmdList);
    }

    // =====================================================================
    // Camera Jitter (for TAA)
    // =====================================================================
    glm::vec2 jitter(0.0f, 0.0f);
    glm::mat4 jitteredProj = Params.View->ProjMatrix;
    if (CVar_r_TAA)
    {
        static const float Halton23[16][2] = {
            {0.5000f, 0.3333f}, {0.2500f, 0.6667f}, {0.7500f, 0.1111f}, {0.1250f, 0.4444f},
            {0.6250f, 0.7778f}, {0.3750f, 0.2222f}, {0.8750f, 0.5556f}, {0.0625f, 0.8889f},
            {0.5625f, 0.0370f}, {0.3125f, 0.3704f}, {0.8125f, 0.7037f}, {0.1875f, 0.1481f},
            {0.6875f, 0.4815f}, {0.4375f, 0.8148f}, {0.9375f, 0.2593f}, {0.0313f, 0.5926f}
        };
        uint32_t idx = FrameIndex % 16;
        jitter.x = (Halton23[idx][0] - 0.5f) * (2.0f / float(CurrentWidth));
        jitter.y = (Halton23[idx][1] - 0.5f) * (2.0f / float(CurrentHeight));
        jitteredProj[2][0] += jitter.x;
        jitteredProj[2][1] += jitter.y;
    }

    // =====================================================================
    // GBuffer Pass
    // =====================================================================
    {
        Profiler.BeginPass(CmdList, TXT("GBuffer"));
        // Build view constants
        FGBufferFillPass::FViewConstants ViewConstants;
        memset(&ViewConstants, 0, sizeof(ViewConstants));
        memcpy(ViewConstants.ModelMatrix, glm::value_ptr(Params.View->ModelMatrix), 64);
        memcpy(ViewConstants.ViewMatrix, glm::value_ptr(Params.View->ViewMatrix), 64);
        memcpy(ViewConstants.ProjMatrix, glm::value_ptr(jitteredProj), 64);
        ViewConstants.CameraPos[0] = Params.View->CameraPos.x;
        ViewConstants.CameraPos[1] = Params.View->CameraPos.y;
        ViewConstants.CameraPos[2] = Params.View->CameraPos.z;
        ViewConstants.CameraPos[3] = 1.0f;

        // Build mesh draw items
        TVector<FGBufferFillPass::FMeshDrawItem> GBufferItems;
        GBufferItems.reserve(Params.GBufferMeshCount);
        for (uint32_t i = 0; i < Params.GBufferMeshCount; ++i)
        {
            const auto& Src = Params.GBufferMeshes[i];
            FGBufferFillPass::FMeshDrawItem Item;
            Item.VertexBuffer = Src.VertexBuffer;
            Item.IndexBuffer = Src.IndexBuffer;
            Item.IndexCount = Src.IndexCount;
            Item.Material.DiffuseTexture = Src.DiffuseTexture;
            Item.Material.NormalTexture = Src.NormalTexture;
            Item.Material.Constants = Src.MaterialConstants;
            GBufferItems.push_back(Item);
        }

        FGBufferFillPass::FRenderDesc GBufferDesc;
        GBufferDesc.ViewConstants = ViewConstants;
        GBufferDesc.MeshDrawItems = GBufferItems.data();
        GBufferDesc.MeshDrawItemCount = static_cast<uint32_t>(GBufferItems.size());

        GBufferPass.Render(CmdList, GBufferDesc);
        Profiler.EndPass(CmdList);
    }

    // =====================================================================
    // HBAO Pass
    // =====================================================================
    if (CVar_r_SSAO)
    {
        Profiler.BeginPass(CmdList, TXT("SSAO"));
        SSao::FHBAOConstants HBAOConstants;
        memset(&HBAOConstants, 0, sizeof(HBAOConstants));

        glm::mat4 invProj = glm::inverse(Params.View->ProjMatrix);
        memcpy(HBAOConstants.ProjMatrix, glm::value_ptr(Params.View->ProjMatrix), 64);
        memcpy(HBAOConstants.InvProjMatrix, glm::value_ptr(invProj), 64);
        memcpy(HBAOConstants.ViewMatrix, glm::value_ptr(Params.View->ViewMatrix), 64);

        HBAOConstants.ScreenSize[0] = float(CurrentWidth);
        HBAOConstants.ScreenSize[1] = float(CurrentHeight);
        HBAOConstants.InvScreenSize[0] = 1.0f / float(CurrentWidth);
        HBAOConstants.InvScreenSize[1] = 1.0f / float(CurrentHeight);
        HBAOConstants.SampleRadius = Params.View->SceneRadius * CVar_r_SSAO_RadiusScale;
        HBAOConstants.AngleBias = 0.2f;
        HBAOConstants.MaxRadiusPixels = 50.0f;
        HBAOConstants.AttenuationScale = 1.0f;
        HBAOConstants.MinAO = 0.3f;

        SSao::FSSAOPass::FDesc HBAODesc;
        HBAODesc.DepthTexture = GBufferPass.GetDepthTexture();
        HBAODesc.NormalTexture = GBufferPass.GetNormalsTexture();
        HBAODesc.OutputTexture = SSAOTexture;
        HBAODesc.OutputWidth = CurrentWidth;
        HBAODesc.OutputHeight = CurrentHeight;

        CmdList->setTextureState(SSAOTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);
        HBAOPass.Dispatch(CmdList, HBAODesc, HBAOConstants);
        Profiler.EndPass(CmdList);

        // Transition SSAO to SRV for blur
        CmdList->setTextureState(SSAOTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        // Transition SSAO blur to UAV
        CmdList->setTextureState(SSAOBlurTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);

        // =====================================================================
        // Bilateral Blur Pass
        // =====================================================================
        Profiler.BeginPass(CmdList, TXT("BilateralBlur"));
        FJointBilateralUpsamplePass::FDesc BilateralDesc;
        BilateralDesc.InputTexture = SSAOTexture;
        BilateralDesc.DepthTexture = GBufferPass.GetDepthTexture();
        BilateralDesc.OutputTexture = SSAOBlurTexture;
        BilateralDesc.OutputWidth = CurrentWidth;
        BilateralDesc.OutputHeight = CurrentHeight;
        BilateralDesc.DepthSigma = 0.01f;
        BilateralBlurPass.Dispatch(CmdList, BilateralDesc);
        Profiler.EndPass(CmdList);
    }

    // Transition SSAO blur to SRV for lighting
    CmdList->setTextureState(SSAOBlurTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);

    // =====================================================================
    // Contact Shadows Pass
    // =====================================================================
    {
        Profiler.BeginPass(CmdList, TXT("ContactShadows"));
        CmdList->setTextureState(GBufferPass.GetDepthTexture(), nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        CmdList->setTextureState(ContactShadowTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);

        ContactShadows::FContactShadowConstants CSConstants;
        memset(&CSConstants, 0, sizeof(CSConstants));

        glm::mat4 viewProj = Params.View->ProjMatrix * Params.View->ViewMatrix;
        glm::mat4 invViewProj = glm::inverse(viewProj);
        memcpy(CSConstants.ViewProj, glm::value_ptr(viewProj), 64);
        memcpy(CSConstants.InvViewProj, glm::value_ptr(invViewProj), 64);
        static const glm::vec3 DefaultLightDir = glm::vec3(0.577f, 0.577f, 0.577f);
        glm::vec3 LightDir = (Params.Lights && Params.LightCount > 0)
            ? Params.Lights[0].Direction
            : DefaultLightDir;
        CSConstants.LightDir[0] = LightDir.x;
        CSConstants.LightDir[1] = LightDir.y;
        CSConstants.LightDir[2] = LightDir.z;
        CSConstants.MaxDistance = CVar_r_ContactShadows_MaxDistance;
        CSConstants.ScreenSize[0] = float(CurrentWidth);
        CSConstants.ScreenSize[1] = float(CurrentHeight);
        CSConstants.RcpScreenSize[0] = 1.0f / float(CurrentWidth);
        CSConstants.RcpScreenSize[1] = 1.0f / float(CurrentHeight);
        CSConstants.StepCount = CVar_r_ContactShadows ? CVar_r_ContactShadows_StepCount : 0;
        CSConstants.Thickness = CVar_r_ContactShadows_Thickness;

        ContactShadows::FContactShadowsPass::FDesc CSDesc;
        CSDesc.DepthTexture = GBufferPass.GetDepthTexture();
        CSDesc.OutputTexture = ContactShadowTexture;
        CSDesc.Width = CurrentWidth;
        CSDesc.Height = CurrentHeight;

        ContactShadowsPass.Dispatch(CmdList, CSDesc, CSConstants);

        // Transition contact shadow texture to SRV for lighting
        CmdList->setTextureState(ContactShadowTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        Profiler.EndPass(CmdList);
    }

    // =====================================================================
    // Lighting Pass
    // =====================================================================
    {
        Profiler.BeginPass(CmdList, TXT("Lighting"));
        glm::mat4 invViewProj = glm::inverse(Params.View->ProjMatrix * Params.View->ViewMatrix);

        CmdList->setTextureState(HDRTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);

        FDeferredLightingPass::FDesc LightingDesc;
        LightingDesc.GBufferDiffuse = GBufferPass.GetDiffuseTexture();
        LightingDesc.GBufferMaterial = GBufferPass.GetSpecularTexture();
        LightingDesc.GBufferNormals = GBufferPass.GetNormalsTexture();
        LightingDesc.GBufferEmissive = GBufferPass.GetEmissiveTexture();
        LightingDesc.GBufferDepth = GBufferPass.GetDepthTexture();
        LightingDesc.ShadowMap = ShadowPass.GetShadowMapTexture();
        LightingDesc.SSAOTexture = SSAOBlurTexture;
        LightingDesc.ContactShadowTexture = ContactShadowTexture;
        LightingDesc.HDROutputTexture = HDRTexture;
        LightingDesc.ShadowSampler = ShadowPass.GetShadowSampler();
        LightingDesc.Width = CurrentWidth;
        LightingDesc.Height = CurrentHeight;
        LightingDesc.bEnableSSAO = CVar_r_SSAO.GetValue();
        LightingDesc.bEnableContactShadows = CVar_r_ContactShadows.GetValue();

        FDeferredLightingPass::FConstants LightingConstants;
        memset(&LightingConstants, 0, sizeof(LightingConstants));
        memcpy(LightingConstants.InvViewProj, glm::value_ptr(invViewProj), 64);
        static const glm::vec3 DefaultLightDir = glm::vec3(0.577f, 0.577f, 0.577f);
        static const float DefaultLightIntensity = 0.8f;
        static const glm::vec3 DefaultAmbientColor = glm::vec3(0.03f, 0.03f, 0.03f);

        glm::vec3 LightDir = (Params.Lights && Params.LightCount > 0)
            ? Params.Lights[0].Direction
            : DefaultLightDir;
        float LightIntensity = (Params.Lights && Params.LightCount > 0)
            ? Params.Lights[0].Intensity
            : DefaultLightIntensity;
        glm::vec3 AmbientColor = (Params.Lights && Params.LightCount > 0)
            ? DefaultAmbientColor  // TODO: add AmbientColor to FLightData
            : DefaultAmbientColor;

        LightingConstants.LightDir[0] = LightDir.x;
        LightingConstants.LightDir[1] = LightDir.y;
        LightingConstants.LightDir[2] = LightDir.z;
        LightingConstants.LightIntensity = LightIntensity;
        LightingConstants.CameraPos[0] = Params.View->CameraPos.x;
        LightingConstants.CameraPos[1] = Params.View->CameraPos.y;
        LightingConstants.CameraPos[2] = Params.View->CameraPos.z;
        LightingConstants.ShadowHardness = 16.0f;
        LightingConstants.AmbientColor[0] = AmbientColor.x;
        LightingConstants.AmbientColor[1] = AmbientColor.y;
        LightingConstants.AmbientColor[2] = AmbientColor.z;
        LightingConstants.MinAO = 0.3f;
        LightingConstants.ScreenSize[0] = float(CurrentWidth);
        LightingConstants.ScreenSize[1] = float(CurrentHeight);
        memcpy(LightingConstants.LightViewProj, glm::value_ptr(Params.View->LightViewProj), 64);
        LightingConstants.ShadowMapSize = 2048.0f;
        LightingConstants.ShadowBias = 0.005f;

        LightingPass.Dispatch(CmdList, LightingDesc, LightingConstants);
        Profiler.EndPass(CmdList);
    }

    // Transition HDR to SRV for SSR
    CmdList->setTextureState(HDRTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
    // Transition SSR texture to UAV
    CmdList->setTextureState(SSRTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);

    // =====================================================================
    // SSR Pass
    // =====================================================================
    if (CVar_r_SSR)
    {
        Profiler.BeginPass(CmdList, TXT("SSR"));
        SSr::FSSRConstants SSRConstants;
        memset(&SSRConstants, 0, sizeof(SSRConstants));

        glm::mat4 invProj = glm::inverse(jitteredProj);
        memcpy(SSRConstants.ProjMatrix, glm::value_ptr(jitteredProj), 64);
        memcpy(SSRConstants.InvProjMatrix, glm::value_ptr(invProj), 64);
        memcpy(SSRConstants.ViewMatrix, glm::value_ptr(Params.View->ViewMatrix), 64);

        SSRConstants.FullScreenSize[0] = float(CurrentWidth);
        SSRConstants.FullScreenSize[1] = float(CurrentHeight);
        SSRConstants.InvFullScreenSize[0] = 1.0f / float(CurrentWidth);
        SSRConstants.InvFullScreenSize[1] = 1.0f / float(CurrentHeight);

        uint32_t halfW = std::max(1u, CurrentWidth / 2);
        uint32_t halfH = std::max(1u, CurrentHeight / 2);
        SSRConstants.HalfScreenSize[0] = float(halfW);
        SSRConstants.HalfScreenSize[1] = float(halfH);
        SSRConstants.InvHalfScreenSize[0] = 1.0f / float(halfW);
        SSRConstants.InvHalfScreenSize[1] = 1.0f / float(halfH);

        SSRConstants.MaxSteps = CVar_r_SSR_MaxSteps;
        SSRConstants.StepSize = CVar_r_SSR_StepSize;
        SSRConstants.MaxDistance = CVar_r_SSR_MaxDistance;
        SSRConstants.Thickness = CVar_r_SSR_Thickness;

        SSr::FSSRPass::FDesc SSRDesc;
        SSRDesc.DepthTexture = GBufferPass.GetDepthTexture();
        SSRDesc.NormalTexture = GBufferPass.GetNormalsTexture();
        SSRDesc.MaterialTexture = GBufferPass.GetSpecularTexture();
        SSRDesc.HDRTexture = HDRTexture;
        SSRDesc.OutputTexture = SSRTexture;
        SSRDesc.OutputWidth = halfW;
        SSRDesc.OutputHeight = halfH;

        SSRPass.Dispatch(CmdList, SSRDesc, SSRConstants);
        Profiler.EndPass(CmdList);
    }

    // Transition SSR to SRV for tone mapping
    CmdList->setTextureState(SSRTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);

    // =====================================================================
    // TAA Pass
    // =====================================================================
    nvrhi::TextureHandle ToneMapInputTexture = HDRTexture;
    if (CVar_r_TAA && bTAAInitialized)
    {
        Profiler.BeginPass(CmdList, TXT("TAA"));
        // Initialize history on first frame by copying current HDR to history
        bool bIsFirstFrame = bTAANeedsHistoryInit;
        if (bTAANeedsHistoryInit)
        {
            CmdList->setTextureState(HDRTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::CopySource);
            CmdList->setTextureState(TAAHistoryTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::CopyDest);
            CmdList->copyTexture(TAAHistoryTexture.Get(), nvrhi::TextureSlice(), HDRTexture.Get(), nvrhi::TextureSlice());
            bTAANeedsHistoryInit = false;
        }

        // Ensure resources are in correct states
        CmdList->setTextureState(HDRTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        CmdList->setTextureState(TAAHistoryTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        CmdList->setTextureState(GBufferPass.GetDepthTexture(), nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        CmdList->setTextureState(TAAOutputTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);

        TAA::FTAAConstants TAAConstants;
        memset(&TAAConstants, 0, sizeof(TAAConstants));

        glm::mat4 currViewProj = jitteredProj * Params.View->ViewMatrix;
        glm::mat4 invCurrViewProj = glm::inverse(currViewProj);
        glm::mat4 prevViewProj = Params.View->PrevProjMatrix * Params.View->PrevViewMatrix;

        memcpy(TAAConstants.InverseCurrViewProj, glm::value_ptr(invCurrViewProj), 64);
        memcpy(TAAConstants.PrevViewProj, glm::value_ptr(prevViewProj), 64);
        TAAConstants.OutputSize[0] = float(CurrentWidth);
        TAAConstants.OutputSize[1] = float(CurrentHeight);
        TAAConstants.RcpOutputSize[0] = 1.0f / float(CurrentWidth);
        TAAConstants.RcpOutputSize[1] = 1.0f / float(CurrentHeight);
        TAAConstants.BlendFactor = bIsFirstFrame ? 1.0f : CVar_r_TAA_BlendFactor;
        TAAConstants.DepthThreshold = CVar_r_TAA_DepthThreshold;

        TAA::FTAAPass::FDesc TAADesc;
        TAADesc.CurrentFrameTexture = HDRTexture;
        TAADesc.HistoryFrameTexture = TAAHistoryTexture;
        TAADesc.DepthTexture = GBufferPass.GetDepthTexture();
        TAADesc.OutputTexture = TAAOutputTexture;
        TAADesc.OutputWidth = CurrentWidth;
        TAADesc.OutputHeight = CurrentHeight;

        TAAPass.Dispatch(CmdList, TAADesc, TAAConstants);

        // Transition TAA output to SRV for motion blur / bloom
        CmdList->setTextureState(TAAOutputTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);

        ToneMapInputTexture = TAAOutputTexture;
        Profiler.EndPass(CmdList);

        // =====================================================================
        // TAA History Copy — must happen BEFORE motion blur so history
        // matches TAA output for correct temporal reprojection next frame.
        // =====================================================================
        CmdList->setTextureState(TAAOutputTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::CopySource);
        CmdList->setTextureState(TAAHistoryTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::CopyDest);
        CmdList->copyTexture(TAAHistoryTexture.Get(), nvrhi::TextureSlice(), TAAOutputTexture.Get(), nvrhi::TextureSlice());
    }

    // =====================================================================
    // Motion Blur Pass
    // =====================================================================
    if (CVar_r_MotionBlur && bMotionBlurInitialized)
    {
        Profiler.BeginPass(CmdList, TXT("MotionBlur"));
        nvrhi::TextureHandle MotionBlurInput = ToneMapInputTexture;

        CmdList->setTextureState(MotionBlurInput, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        CmdList->setTextureState(GBufferPass.GetDepthTexture(), nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        CmdList->setTextureState(MotionBlurTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);

        MotionBlur::FMotionBlurConstants MBConstants;
        memset(&MBConstants, 0, sizeof(MBConstants));

        glm::mat4 currViewProj = jitteredProj * Params.View->ViewMatrix;
        glm::mat4 invCurrViewProj = glm::inverse(currViewProj);
        glm::mat4 prevViewProj = Params.View->PrevProjMatrix * Params.View->PrevViewMatrix;

        memcpy(MBConstants.InverseCurrViewProj, glm::value_ptr(invCurrViewProj), 64);
        memcpy(MBConstants.PrevViewProj, glm::value_ptr(prevViewProj), 64);
        MBConstants.OutputSize[0] = float(CurrentWidth);
        MBConstants.OutputSize[1] = float(CurrentHeight);
        MBConstants.RcpOutputSize[0] = 1.0f / float(CurrentWidth);
        MBConstants.RcpOutputSize[1] = 1.0f / float(CurrentHeight);
        MBConstants.VelocityScale = CVar_r_MotionBlur_VelocityScale;
        MBConstants.MinVelocity = CVar_r_MotionBlur_MinVelocity;

        MotionBlur::FMotionBlurPass::FDesc MBDesc;
        MBDesc.ColorTexture = MotionBlurInput;
        MBDesc.DepthTexture = GBufferPass.GetDepthTexture();
        MBDesc.OutputTexture = MotionBlurTexture;
        MBDesc.OutputWidth = CurrentWidth;
        MBDesc.OutputHeight = CurrentHeight;

        MotionBlurPass.Dispatch(CmdList, MBDesc, MBConstants);

        // Transition motion blur output to SRV for DOF / bloom
        CmdList->setTextureState(MotionBlurTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);

        ToneMapInputTexture = MotionBlurTexture;
        Profiler.EndPass(CmdList);
    }

    // =====================================================================
    // DOF Pass
    // =====================================================================
    if (CVar_r_DOF && bDOFInitialized)
    {
        Profiler.BeginPass(CmdList, TXT("DOF"));
        nvrhi::TextureHandle DOFInput = ToneMapInputTexture;

        CmdList->setTextureState(DOFInput, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        CmdList->setTextureState(GBufferPass.GetDepthTexture(), nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        CmdList->setTextureState(DOFTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);

        DOF::FDOFConstants DOFConstants;
        memset(&DOFConstants, 0, sizeof(DOFConstants));
        DOFConstants.FocalDepth = CVar_r_DOF_FocalDepth;
        DOFConstants.Aperture = CVar_r_DOF_Aperture;
        DOFConstants.DepthScale = CVar_r_DOF_DepthScale;
        DOFConstants.MaxBlurRadius = CVar_r_DOF_MaxBlurRadius;
        DOFConstants.OutputSize[0] = float(CurrentWidth);
        DOFConstants.OutputSize[1] = float(CurrentHeight);
        DOFConstants.RcpOutputSize[0] = 1.0f / float(CurrentWidth);
        DOFConstants.RcpOutputSize[1] = 1.0f / float(CurrentHeight);

        DOF::FDOFPass::FDesc DOFDesc;
        DOFDesc.ColorTexture = DOFInput;
        DOFDesc.DepthTexture = GBufferPass.GetDepthTexture();
        DOFDesc.OutputTexture = DOFTexture;
        DOFDesc.OutputWidth = CurrentWidth;
        DOFDesc.OutputHeight = CurrentHeight;

        DOFPass.Dispatch(CmdList, DOFDesc, DOFConstants);

        // Transition DOF output to SRV for bloom/tone mapping
        CmdList->setTextureState(DOFTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);

        ToneMapInputTexture = DOFTexture;
        Profiler.EndPass(CmdList);
    }

    // =====================================================================
    // Bloom Pass
    // =====================================================================
    if (CVar_r_Bloom)
    {
        Profiler.BeginPass(CmdList, TXT("Bloom"));
        FBloomPass::FDesc BloomDesc;
        BloomDesc.HDRTexture = ToneMapInputTexture;
        BloomDesc.OutputTexture = BloomTexture;
        BloomDesc.HalfResTexture = BloomHalfResTexture;
        BloomDesc.BlurTempTexture = BloomBlurTempTexture;
        BloomDesc.LinearSampler = GBufferPass.GetLinearSampler();
        BloomDesc.FullResWidth = CurrentWidth;
        BloomDesc.FullResHeight = CurrentHeight;
        BloomDesc.Threshold = 0.8f;
        BloomDesc.Intensity = 0.4f;
        BloomDesc.Sigma = 2.0f;
        BloomDesc.BlurIterations = 2;

        CmdList->setTextureState(ToneMapInputTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        CmdList->setTextureState(BloomTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);

        BloomPass.Dispatch(CmdList, BloomDesc);

        CmdList->setTextureState(BloomTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        Profiler.EndPass(CmdList);
    }

    // =====================================================================
    // Exposure Adaptation Pass
    // =====================================================================
    if (CVar_r_ExposureAdaptation && bExposureAdaptationInitialized)
    {
        Profiler.BeginPass(CmdList, TXT("Exposure"));
        CmdList->setTextureState(ToneMapInputTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        CmdList->setTextureState(AdaptedLuminanceTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);

        Exposure::FExposureConstants ExposureConstants;
        memset(&ExposureConstants, 0, sizeof(ExposureConstants));
        ExposureConstants.AdaptationSpeed = CVar_r_Exposure_AdaptationSpeed;
        ExposureConstants.KeyValue = CVar_r_Exposure_KeyValue;

        Exposure::FExposureAdaptationPass::FDesc ExposureDesc;
        ExposureDesc.SceneColorTexture = ToneMapInputTexture;
        ExposureDesc.AdaptedLuminanceTexture = AdaptedLuminanceTexture;

        ExposurePass.Dispatch(CmdList, ExposureDesc, ExposureConstants);

        // Transition adapted luminance to SRV for tone mapping
        CmdList->setTextureState(AdaptedLuminanceTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        Profiler.EndPass(CmdList);
    }

    // =====================================================================
    // Tone Mapping Pass
    // =====================================================================
    {
        Profiler.BeginPass(CmdList, TXT("ToneMap"));
        CmdList->setTextureState(ToneMapInputTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        CmdList->setTextureState(BloomTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        CmdList->setTextureState(SDRTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);

        FToneMappingPass::FDesc ToneMapDesc;
        ToneMapDesc.HDRInputTexture = ToneMapInputTexture;
        ToneMapDesc.BloomTexture = BloomTexture;
        ToneMapDesc.SSRTexture = SSRTexture;
        ToneMapDesc.SDROutputTexture = SDRTexture;
        ToneMapDesc.AdaptedLuminanceTexture = AdaptedLuminanceTexture;
        ToneMapDesc.Width = CurrentWidth;
        ToneMapDesc.Height = CurrentHeight;

        FToneMappingPass::FConstants ToneMapConstants;
        memset(&ToneMapConstants, 0, sizeof(ToneMapConstants));
        ToneMapConstants.Exposure = 1.0f;
        ToneMapConstants.Gamma = 2.2f;
        ToneMapConstants.TonemapMode = 0; // ACES
        ToneMapConstants.BloomIntensity = 0.4f;
        ToneMapConstants.TextureSize[0] = float(CurrentWidth);
        ToneMapConstants.TextureSize[1] = float(CurrentHeight);
        ToneMapConstants.KeyValue = CVar_r_Exposure_KeyValue;
        ToneMapConstants.UseExposureAdaptation = CVar_r_ExposureAdaptation ? 1 : 0;

        ToneMapPass.Dispatch(CmdList, ToneMapDesc, ToneMapConstants);
        Profiler.EndPass(CmdList);
    }

    // =====================================================================
    // Lens Effects Pass
    // =====================================================================
    nvrhi::TextureHandle BlitSourceTexture = SDRTexture;
    if (CVar_r_LensEffects && bLensEffectsInitialized)
    {
        Profiler.BeginPass(CmdList, TXT("LensEffects"));
        // Transition SDR to SRV for lens effects read
        CmdList->setTextureState(SDRTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        CmdList->setTextureState(LensEffectsTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);

        LensEffects::FLensEffectsConstants LEConstants;
        memset(&LEConstants, 0, sizeof(LEConstants));
        LEConstants.ChromaticAmount = CVar_r_CA_Amount;
        LEConstants.VignetteIntensity = CVar_r_Vignette_Intensity;
        LEConstants.GrainIntensity = CVar_r_Grain_Intensity;
        LEConstants.FrameIndex = static_cast<TINT32>(FrameIndex);
        LEConstants.OutputSize[0] = float(CurrentWidth);
        LEConstants.OutputSize[1] = float(CurrentHeight);
        LEConstants.RcpOutputSize[0] = 1.0f / float(CurrentWidth);
        LEConstants.RcpOutputSize[1] = 1.0f / float(CurrentHeight);

        LensEffects::FLensEffectsPass::FDesc LEDesc;
        LEDesc.SDRTexture = SDRTexture;
        LEDesc.OutputTexture = LensEffectsTexture;
        LEDesc.OutputWidth = CurrentWidth;
        LEDesc.OutputHeight = CurrentHeight;

        LensEffectsPass.Dispatch(CmdList, LEDesc, LEConstants);

        // Transition lens effects output to SRV for blit
        CmdList->setTextureState(LensEffectsTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);

        BlitSourceTexture = LensEffectsTexture;
        Profiler.EndPass(CmdList);
    }

    // =====================================================================
    // Frame Dump (optional)
    // =====================================================================
    if (Params.FrameDumper && Params.FrameDumper->IsEnabled())
    {
        CmdList->setTextureState(BlitSourceTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::CopySource);
        Params.FrameDumper->BeginDump(Device, BlitSourceTexture.Get(), CurrentWidth, CurrentHeight);
        Params.FrameDumper->PrepareCopy(CmdList);
    }

    // =====================================================================
    // Blit to output framebuffer
    // =====================================================================
    CmdList->setTextureState(BlitSourceTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);

    FCommonRenderPasses::BlitParameters BlitParams;
    FCommonRenderPasses::BlitTexture(
        CmdList,
        Params.OutputFramebuffer,
        BlitSourceTexture,
        &BindingCache,
        CurrentWidth,
        CurrentHeight,
        BlitParams);

    // =====================================================================
    // Transition GBuffer textures back to RenderTarget for next frame
    // =====================================================================
    CmdList->setTextureState(GBufferPass.GetDiffuseTexture(), nvrhi::AllSubresources, nvrhi::ResourceStates::RenderTarget);
    CmdList->setTextureState(GBufferPass.GetSpecularTexture(), nvrhi::AllSubresources, nvrhi::ResourceStates::RenderTarget);
    CmdList->setTextureState(GBufferPass.GetNormalsTexture(), nvrhi::AllSubresources, nvrhi::ResourceStates::RenderTarget);
    CmdList->setTextureState(GBufferPass.GetEmissiveTexture(), nvrhi::AllSubresources, nvrhi::ResourceStates::RenderTarget);
    CmdList->setTextureState(GBufferPass.GetDepthTexture(), nvrhi::AllSubresources, nvrhi::ResourceStates::DepthWrite);

    // Update frame state for TAA
    FrameIndex++;
}
