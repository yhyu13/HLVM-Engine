#include "Renderer/Deferred/FDeferredFrameRenderer.h"
#include "Renderer/Common/FCommonRenderPasses.h"
#include "Image/FRenderPassDumper.h"
#include "Core/Log.h"
#include "Utility/CVar/CVarMacros.h"
#include <glm/gtc/type_ptr.hpp>

DECLARE_LOG_CATEGORY(LogRenderer)

AUTO_CVAR_FLOAT(r_SSAO_RadiusScale, 0.05f,
    "SSAO sample radius as fraction of scene radius", EConsoleVariableFlag::Saved)

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
    return true;
}

void FDeferredFrameRenderer::Shutdown()
{
    GBufferPass.Shutdown();
    ShadowPass.Shutdown();
    HBAOPass.Shutdown();
    BilateralBlurPass.Shutdown();
    LightingPass.Shutdown();
    BloomPass.Shutdown();
    ToneMapPass.Shutdown();

    HDRTexture = nullptr;
    SDRTexture = nullptr;
    SSAOTexture = nullptr;
    SSAOBlurTexture = nullptr;
    BloomHalfResTexture = nullptr;
    BloomBlurTempTexture = nullptr;
    BloomTexture = nullptr;

    BindingCache.Clear();
    Device = nullptr;
    CurrentWidth = 0;
    CurrentHeight = 0;
    bIsInitialized = false;
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
    // GBuffer Pass
    // =====================================================================
    {
        // Build view constants
        FGBufferFillPass::FViewConstants ViewConstants;
        memset(&ViewConstants, 0, sizeof(ViewConstants));
        memcpy(ViewConstants.ModelMatrix, glm::value_ptr(Params.View->ModelMatrix), 64);
        memcpy(ViewConstants.ViewMatrix, glm::value_ptr(Params.View->ViewMatrix), 64);
        memcpy(ViewConstants.ProjMatrix, glm::value_ptr(Params.View->ProjMatrix), 64);
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

    // =====================================================================
    // Bloom Pass
    // =====================================================================
    {
        FBloomPass::FDesc BloomDesc;
        BloomDesc.HDRTexture = HDRTexture;
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

        CmdList->setTextureState(HDRTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        CmdList->setTextureState(BloomTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);

        BloomPass.Dispatch(CmdList, BloomDesc);

        CmdList->setTextureState(BloomTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
    }

    // =====================================================================
    // Tone Mapping Pass
    // =====================================================================
    {
        CmdList->setTextureState(HDRTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        CmdList->setTextureState(BloomTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        CmdList->setTextureState(SDRTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);

        FToneMappingPass::FDesc ToneMapDesc;
        ToneMapDesc.HDRInputTexture = HDRTexture;
        ToneMapDesc.BloomTexture = BloomTexture;
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
}
