#pragma once

#include "Core/String.h"
#include "Renderer/Common/FBindingCache.h"
#include "Renderer/Deferred/FGBufferFillPass.h"
#include "Renderer/Deferred/FDeferredLightingPass.h"
#include "Renderer/PostProcess/FBloomPass.h"
#include "Renderer/PostProcess/FToneMappingPass.h"
#include "Renderer/PostProcess/FJointBilateralUpsamplePass.h"
#include "Renderer/PostProcess/FSSAOPass.h"
#include "Renderer/PostProcess/FSSRPass.h"
#include "Renderer/Shadow/FShadowMapPass.h"
#include <nvrhi/nvrhi.h>
#include <glm/glm.hpp>

class FRenderPassDumper;

class FDeferredFrameRenderer
{
public:
    struct FViewData
    {
        glm::mat4 ViewMatrix;
        glm::mat4 ProjMatrix;
        glm::vec3 CameraPos;
        glm::mat4 LightViewProj;
        glm::mat4 ModelMatrix;
        float SceneRadius = 0.0f;
    };

    struct FGBufferMeshItem
    {
        nvrhi::BufferHandle VertexBuffer;
        nvrhi::BufferHandle IndexBuffer;
        uint32_t IndexCount;
        nvrhi::TextureHandle DiffuseTexture;
        nvrhi::TextureHandle NormalTexture;
        FGBufferFillPass::FMaterialConstants MaterialConstants;
    };

    struct FRenderParams
    {
        const FViewData* View;
        const FGBufferMeshItem* GBufferMeshes;
        uint32_t GBufferMeshCount;
        const FShadowMapPass::FMeshDrawItem* ShadowMeshes;
        uint32_t ShadowMeshCount;
        nvrhi::IFramebuffer* OutputFramebuffer;
        FRenderPassDumper* FrameDumper;
    };

    bool Initialize(nvrhi::IDevice* InDevice, const FString& InShaderDataDir);
    void Render(nvrhi::ICommandList* CmdList, const FRenderParams& Params);
    void Shutdown();

    nvrhi::TextureHandle GetSDRTexture() const
    {
        return SDRTexture;
    }

    FDeferredFrameRenderer() = default;
    ~FDeferredFrameRenderer()
    {
        Shutdown();
    }
    FDeferredFrameRenderer(const FDeferredFrameRenderer&) = delete;
    FDeferredFrameRenderer& operator=(const FDeferredFrameRenderer&) = delete;

private:
    void ResizeIfNeeded(uint32_t Width, uint32_t Height);
    void CreateIntermediateTextures(uint32_t Width, uint32_t Height);

    // Sub-passes
    FGBufferFillPass GBufferPass;
    FShadowMapPass ShadowPass;
    SSao::FSSAOPass HBAOPass;
    FJointBilateralUpsamplePass BilateralBlurPass;
    SSr::FSSRPass SSRPass;
    FDeferredLightingPass LightingPass;
    FBloomPass BloomPass;
    FToneMappingPass ToneMapPass;

    // Intermediate textures
    nvrhi::TextureHandle HDRTexture;
    nvrhi::TextureHandle SDRTexture;
    nvrhi::TextureHandle SSAOTexture;
    nvrhi::TextureHandle SSAOBlurTexture;
    nvrhi::TextureHandle BloomHalfResTexture;
    nvrhi::TextureHandle BloomBlurTempTexture;
    nvrhi::TextureHandle BloomTexture;
    nvrhi::TextureHandle SSRTexture;

    // State
    nvrhi::IDevice* Device = nullptr;
    FString ShaderDataDir;
    uint32_t CurrentWidth = 0;
    uint32_t CurrentHeight = 0;
    FBindingCache BindingCache;
    bool bIsInitialized = false;
};
