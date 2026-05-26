#pragma once

#include "Core/String.h"
#include "Renderer/Common/FBindingCache.h"
#include "Renderer/Deferred/FGBufferFillPass.h"
#include "Renderer/Deferred/FDeferredLightingPass.h"
#include "Renderer/Deferred/FLightData.h"
#include "Renderer/PostProcess/FBloomPass.h"
#include "Renderer/PostProcess/FToneMappingPass.h"
#include "Renderer/PostProcess/FJointBilateralUpsamplePass.h"
#include "Renderer/PostProcess/FSSAOPass.h"
#include "Renderer/PostProcess/FSSRPass.h"
#include "Renderer/PostProcess/FTAAPass.h"
#include "Renderer/PostProcess/FMotionBlurPass.h"
#include "Renderer/PostProcess/FDOFPass.h"
#include "Renderer/PostProcess/FLensEffectsPass.h"
#include "Renderer/PostProcess/FExposureAdaptationPass.h"
#include "Renderer/PostProcess/FContactShadowsPass.h"
#include "Renderer/Shadow/FShadowMapPass.h"
#include "Renderer/Utility/FGPUProfiler.h"
#include "Renderer/Shader/ShaderHotReloader.h"
#include "Renderer/RenderGraph/FRenderGraph.h"
#include <nvrhi/nvrhi.h>
#include <glm/glm.hpp>

class FRenderPassDumper;

class FDeferredFrameRenderer : public IShaderReloadable
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
        glm::mat4 PrevViewMatrix = glm::mat4(1.0f);
        glm::mat4 PrevProjMatrix = glm::mat4(1.0f);
    };

    struct FGBufferMeshItem
    {
        nvrhi::BufferHandle VertexBuffer;
        nvrhi::BufferHandle IndexBuffer;
        uint32_t IndexCount;
        nvrhi::TextureHandle DiffuseTexture;
        nvrhi::TextureHandle NormalTexture;
        nvrhi::TextureHandle MetallicTexture;
        nvrhi::TextureHandle RoughnessTexture;
        nvrhi::TextureHandle AOTexture;
        FGBufferFillPass::FMaterialConstants MaterialConstants;
        glm::mat4 ModelMatrix{1.0f};  // Per-mesh world transform
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

        // Lighting data (single directional light for now)
        const FLightData* Lights = nullptr;
        uint32_t LightCount = 0;
    };

    bool Initialize(nvrhi::IDevice* InDevice, const FString& InShaderDataDir);
    void Render(nvrhi::ICommandList* CmdList, const FRenderParams& Params);
    void Shutdown();

    // IShaderReloadable
    virtual void ReloadShaders() override;

    // Shader hot-reload update (call once per frame)
    void UpdateShaderHotReload();

    // GPU profiling
    void EndProfilingFrame() { Profiler.EndFrame(); }
    [[nodiscard]] const FGPUProfiler& GetProfiler() const { return Profiler; }
    [[nodiscard]] FGPUProfiler& GetProfiler() { return Profiler; }

    nvrhi::TextureHandle GetSDRTexture() const
    {
        return SDRTexture;
    }

    FDeferredFrameRenderer() = default;
    ~FDeferredFrameRenderer() override
    {
        Shutdown();
    }
    FDeferredFrameRenderer(const FDeferredFrameRenderer&) = delete;
    FDeferredFrameRenderer& operator=(const FDeferredFrameRenderer&) = delete;

private:
    void ResizeIfNeeded(uint32_t Width, uint32_t Height);
    void CreateIntermediateTextures(uint32_t Width, uint32_t Height);

    // GPU Instancing helpers
    struct FInstancedMeshGroup
    {
        nvrhi::BufferHandle VertexBuffer;
        nvrhi::BufferHandle IndexBuffer;
        uint32_t IndexCount = 0;
        TVector<glm::mat4> InstanceMatrices;
        FGBufferFillPass::FMaterialBinding Material;
    };

    struct FInstancedShadowGroup
    {
        nvrhi::BufferHandle VertexBuffer;
        nvrhi::BufferHandle IndexBuffer;
        uint32_t IndexCount = 0;
        TVector<glm::mat4> InstanceMatrices;
    };

    TVector<FInstancedMeshGroup> GroupMeshesByGeometry(const FGBufferMeshItem* Meshes, uint32_t Count);
    TVector<FInstancedShadowGroup> GroupShadowMeshesByGeometry(const FShadowMapPass::FMeshDrawItem* Meshes, uint32_t Count);

    // Sub-passes
    FGBufferFillPass GBufferPass;
    FShadowMapPass ShadowPass;
    SSao::FSSAOPass HBAOPass;
    FJointBilateralUpsamplePass BilateralBlurPass;
    SSr::FSSRPass SSRPass;
    FDeferredLightingPass LightingPass;
    TAA::FTAAPass TAAPass;
    MotionBlur::FMotionBlurPass MotionBlurPass;
    DOF::FDOFPass DOFPass;
    FBloomPass BloomPass;
    FToneMappingPass ToneMapPass;
    LensEffects::FLensEffectsPass LensEffectsPass;
    Exposure::FExposureAdaptationPass ExposurePass;
    ContactShadows::FContactShadowsPass ContactShadowsPass;

    // Intermediate textures
    nvrhi::TextureHandle HDRTexture;
    nvrhi::TextureHandle SDRTexture;
    nvrhi::TextureHandle SSAOTexture;
    nvrhi::TextureHandle SSAOBlurTexture;
    nvrhi::TextureHandle BloomHalfResTexture;
    nvrhi::TextureHandle BloomBlurTempTexture;
    nvrhi::TextureHandle BloomTexture;
    nvrhi::TextureHandle SSRTexture;
    nvrhi::TextureHandle TAAOutputTexture;
    nvrhi::TextureHandle TAAHistoryTexture;
    nvrhi::TextureHandle MotionBlurTexture;
    nvrhi::TextureHandle DOFTexture;
    nvrhi::TextureHandle LensEffectsTexture;
    nvrhi::TextureHandle AdaptedLuminanceTexture;
    nvrhi::TextureHandle ContactShadowTexture;

    uint32_t FrameIndex = 0;
    bool bTAANeedsHistoryInit = true;

    // State
    nvrhi::IDevice* Device = nullptr;
    FString ShaderDataDir;
    uint32_t CurrentWidth = 0;
    uint32_t CurrentHeight = 0;
    FBindingCache BindingCache;
    FRenderGraph PostProcessGraph;
    bool bIsInitialized = false;
    bool bTAAInitialized = false;
    bool bMotionBlurInitialized = false;
    bool bDOFInitialized = false;
    bool bLensEffectsInitialized = false;
    bool bExposureAdaptationInitialized = false;

    FGPUProfiler Profiler;
    bool bHotReloadRegistered = false;
};
