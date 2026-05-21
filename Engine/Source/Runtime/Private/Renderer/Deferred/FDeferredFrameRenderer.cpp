#include "Renderer/Deferred/FDeferredFrameRenderer.h"
#include "Renderer/Common/FCommonRenderPasses.h"
#include "Image/FRenderPassDumper.h"
#include "Core/Log.h"
#include "Utility/CVar/CVarMacros.h"
#include <glm/gtc/type_ptr.hpp>

DECLARE_LOG_CATEGORY(LogRenderer)

AUTO_CVAR_FLOAT(r_SSAO_RadiusScale, 0.05f,
    "SSAO sample radius as fraction of scene radius", EConsoleVariableFlag::Saved)

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

    if (!ToneMapPass.Initialize(Device, ShaderDataDir))
    {
        HLVM_LOG(LogRenderer, err, TXT("FDeferredFrameRenderer: Failed to initialize tone mapping pass"));
        return false;
    }

    HLVM_LOG(LogRenderer, info, TXT("FDeferredFrameRenderer initialized successfully"));
    bIsInitialized = true;
    bTAAInitialized = false;
    return true;
}

void FDeferredFrameRenderer::Shutdown()
{
    GBufferPass.Shutdown();
    ShadowPass.Shutdown();
    HBAOPass.Shutdown();
    BilateralBlurPass.Shutdown();
    SSRPass.Shutdown();
    LightingPass.Shutdown();
    TAAPass.Shutdown();
    BloomPass.Shutdown();
    ToneMapPass.Shutdown();

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

    BindingCache.Clear();
    Device = nullptr;
    CurrentWidth = 0;
    CurrentHeight = 0;
    bIsInitialized = false;
    bTAAInitialized = false;
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

    // =====================================================================
    // Shadow Pass
    // =====================================================================
    {
        FShadowMapPass::FRenderDesc ShadowDesc;
        ShadowDesc.LightViewProj = Params.View->LightViewProj;
        ShadowDesc.ModelMatrix = Params.View->ModelMatrix;
        ShadowDesc.MeshDrawItems = Params.ShadowMeshes;
        ShadowDesc.MeshDrawItemCount = Params.ShadowMeshCount;

        ShadowPass.Render(CmdList, ShadowDesc);
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
    }

    // =====================================================================
    // HBAO Pass
    // =====================================================================
    {
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
    }

    // Transition SSAO to SRV for blur
    CmdList->setTextureState(SSAOTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
    // Transition SSAO blur to UAV
    CmdList->setTextureState(SSAOBlurTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);

    // =====================================================================
    // Bilateral Blur Pass
    // =====================================================================
    {
        FJointBilateralUpsamplePass::FDesc BilateralDesc;
        BilateralDesc.InputTexture = SSAOTexture;
        BilateralDesc.DepthTexture = GBufferPass.GetDepthTexture();
        BilateralDesc.OutputTexture = SSAOBlurTexture;
        BilateralDesc.OutputWidth = CurrentWidth;
        BilateralDesc.OutputHeight = CurrentHeight;
        BilateralDesc.DepthSigma = 0.01f;
        BilateralBlurPass.Dispatch(CmdList, BilateralDesc);
    }

    // Transition SSAO blur to SRV for lighting
    CmdList->setTextureState(SSAOBlurTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);

    // =====================================================================
    // Lighting Pass
    // =====================================================================
    {
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
        LightingDesc.HDROutputTexture = HDRTexture;
        LightingDesc.ShadowSampler = ShadowPass.GetShadowSampler();
        LightingDesc.Width = CurrentWidth;
        LightingDesc.Height = CurrentHeight;

        FDeferredLightingPass::FConstants LightingConstants;
        memset(&LightingConstants, 0, sizeof(LightingConstants));
        memcpy(LightingConstants.InvViewProj, glm::value_ptr(invViewProj), 64);
        LightingConstants.LightDir[0] = 0.577f;
        LightingConstants.LightDir[1] = 0.577f;
        LightingConstants.LightDir[2] = 0.577f;
        LightingConstants.LightIntensity = 0.8f;
        LightingConstants.CameraPos[0] = Params.View->CameraPos.x;
        LightingConstants.CameraPos[1] = Params.View->CameraPos.y;
        LightingConstants.CameraPos[2] = Params.View->CameraPos.z;
        LightingConstants.ShadowHardness = 16.0f;
        LightingConstants.AmbientColor[0] = 0.03f;
        LightingConstants.AmbientColor[1] = 0.03f;
        LightingConstants.AmbientColor[2] = 0.03f;
        LightingConstants.MinAO = 0.3f;
        LightingConstants.ScreenSize[0] = float(CurrentWidth);
        LightingConstants.ScreenSize[1] = float(CurrentHeight);
        memcpy(LightingConstants.LightViewProj, glm::value_ptr(Params.View->LightViewProj), 64);
        LightingConstants.ShadowMapSize = 2048.0f;
        LightingConstants.ShadowBias = 0.005f;

        LightingPass.Dispatch(CmdList, LightingDesc, LightingConstants);
    }

    // Transition HDR to SRV for SSR
    CmdList->setTextureState(HDRTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
    // Transition SSR texture to UAV
    CmdList->setTextureState(SSRTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);

    // =====================================================================
    // SSR Pass
    // =====================================================================
    {
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
    }

    // Transition SSR to SRV for tone mapping
    CmdList->setTextureState(SSRTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);

    // =====================================================================
    // TAA Pass
    // =====================================================================
    nvrhi::TextureHandle ToneMapInputTexture = HDRTexture;
    if (CVar_r_TAA && bTAAInitialized)
    {
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

        // Transition TAA output to SRV for bloom/tone mapping
        CmdList->setTextureState(TAAOutputTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);

        // Copy TAA output to history for next frame
        CmdList->setTextureState(TAAOutputTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::CopySource);
        CmdList->setTextureState(TAAHistoryTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::CopyDest);
        CmdList->copyTexture(TAAHistoryTexture.Get(), nvrhi::TextureSlice(), TAAOutputTexture.Get(), nvrhi::TextureSlice());

        ToneMapInputTexture = TAAOutputTexture;
    }

    // =====================================================================
    // Bloom Pass
    // =====================================================================
    {
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
    }

    // =====================================================================
    // Tone Mapping Pass
    // =====================================================================
    {
        CmdList->setTextureState(ToneMapInputTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        CmdList->setTextureState(BloomTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        CmdList->setTextureState(SDRTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);

        FToneMappingPass::FDesc ToneMapDesc;
        ToneMapDesc.HDRInputTexture = ToneMapInputTexture;
        ToneMapDesc.BloomTexture = BloomTexture;
        ToneMapDesc.SSRTexture = SSRTexture;
        ToneMapDesc.SDROutputTexture = SDRTexture;
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

        ToneMapPass.Dispatch(CmdList, ToneMapDesc, ToneMapConstants);
    }

    // =====================================================================
    // Frame Dump (optional)
    // =====================================================================
    if (Params.FrameDumper && Params.FrameDumper->IsEnabled())
    {
        CmdList->setTextureState(SDRTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::CopySource);
        Params.FrameDumper->BeginDump(Device, SDRTexture.Get(), CurrentWidth, CurrentHeight);
        Params.FrameDumper->PrepareCopy(CmdList);
    }

    // =====================================================================
    // Blit to output framebuffer
    // =====================================================================
    CmdList->setTextureState(SDRTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);

    FCommonRenderPasses::BlitParameters BlitParams;
    FCommonRenderPasses::BlitTexture(
        CmdList,
        Params.OutputFramebuffer,
        SDRTexture,
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
