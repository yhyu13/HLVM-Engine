#pragma once

#include "Core/String.h"
#include "Renderer/Common/FBindingCache.h"
#include "Renderer/Deferred/FGBufferFillPass.h"
#include "Renderer/Deferred/FDeferredLightingPass.h"
#include "Renderer/Deferred/FLightData.h"
#include "Renderer/Shadow/FShadowMapPass.h"
#include "Renderer/Utility/FGPUProfiler.h"
#include "Renderer/Shader/ShaderHotReloader.h"
#include "Renderer/RenderGraph/FRenderGraph.h"
#include <nvrhi/nvrhi.h>
#include <glm/glm.hpp>

// Forward declarations — post-process passes are implementation details
namespace SSao { class FSSAOPass; }
namespace SSr { class FSSRPass; }
namespace TAA { class FTAAPass; }
namespace MotionBlur { class FMotionBlurPass; }
namespace DOF { class FDOFPass; }
namespace LensEffects { class FLensEffectsPass; }
namespace Exposure { class FExposureAdaptationPass; }
namespace ContactShadows { class FContactShadowsPass; }
class FBloomPass;
class FToneMappingPass;
class FJointBilateralUpsamplePass;
class FRenderPassDumper;

/**
 * @brief Deferred frame renderer — thin orchestrator over passes
 *
 * Core passes (GBuffer, Shadow, Lighting) are direct members.
 * Post-process passes are owned via TUniquePtr to reduce header coupling.
 *
 * To add a post-process pass:
 *   1. Create the pass class
 *   2. Add it in FDeferredFrameRenderer.cpp (not this header!)
 */
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
        glm::mat4 ModelMatrix{1.0f};
    };

    struct FRenderParams
    {
        const FViewData* View = nullptr;
        const FGBufferMeshItem* GBufferMeshes = nullptr;
        uint32_t GBufferMeshCount = 0;
        const FShadowMapPass::FMeshDrawItem* ShadowMeshes = nullptr;
        uint32_t ShadowMeshCount = 0;
        nvrhi::IFramebuffer* OutputFramebuffer = nullptr;
        FRenderPassDumper* FrameDumper = nullptr;
        const FLightData* Lights = nullptr;
        uint32_t LightCount = 0;
    };

    bool Initialize(nvrhi::IDevice* InDevice, const FString& InShaderDataDir);
    void Render(nvrhi::ICommandList* CmdList, const FRenderParams& Params);
    void Shutdown();

    virtual void ReloadShaders() override;
    void UpdateShaderHotReload();

    void EndProfilingFrame() { Profiler.EndFrame(); }
    [[nodiscard]] const FGPUProfiler& GetProfiler() const { return Profiler; }
    [[nodiscard]] FGPUProfiler& GetProfiler() { return Profiler; }

    [[nodiscard]] nvrhi::TextureHandle GetSDRTexture() const { return SDRTexture; }

    FDeferredFrameRenderer();
    ~FDeferredFrameRenderer() override;

    FDeferredFrameRenderer(const FDeferredFrameRenderer&) = delete;
    FDeferredFrameRenderer& operator=(const FDeferredFrameRenderer&) = delete;

private:
    void ResizeIfNeeded(uint32_t Width, uint32_t Height);
    void CreateIntermediateTextures(uint32_t Width, uint32_t Height);

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

    // =====================================================================
    // Core passes (direct members — fundamental to deferred pipeline)
    // =====================================================================
    FGBufferFillPass GBufferPass;
    FShadowMapPass ShadowPass;
    FDeferredLightingPass LightingPass;

    // =====================================================================
    // Post-process passes (opaque pointers — reduces header coupling)
    // =====================================================================
    TUniquePtr<SSao::FSSAOPass> HBAOPass;
    TUniquePtr<FJointBilateralUpsamplePass> BilateralBlurPass;
    TUniquePtr<SSr::FSSRPass> SSRPass;
    TUniquePtr<TAA::FTAAPass> TAAPass;
    TUniquePtr<MotionBlur::FMotionBlurPass> MotionBlurPass;
    TUniquePtr<DOF::FDOFPass> DOFPass;
    TUniquePtr<FBloomPass> BloomPass;
    TUniquePtr<FToneMappingPass> ToneMapPass;
    TUniquePtr<LensEffects::FLensEffectsPass> LensEffectsPass;
    TUniquePtr<Exposure::FExposureAdaptationPass> ExposurePass;
    TUniquePtr<ContactShadows::FContactShadowsPass> ContactShadowsPass;

    // =====================================================================
    // Intermediate textures
    // =====================================================================
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
