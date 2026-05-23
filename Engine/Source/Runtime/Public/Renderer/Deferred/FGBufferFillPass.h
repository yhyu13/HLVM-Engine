#pragma once

#include "Core/String.h"
#include <nvrhi/nvrhi.h>

class FGBufferFillPass
{
public:
    struct FDesc
    {
        uint32_t Width = 0;
        uint32_t Height = 0;
    };

    struct FViewConstants
    {
        float ModelMatrix[16];
        float ViewMatrix[16];
        float ProjMatrix[16];
        float CameraPos[4];
        float Pad[12];
    };
    static_assert(sizeof(FViewConstants) == 256, "FViewConstants must be 256 bytes");

    struct FMaterialConstants
    {
        float AlbedoTint[4];
        float Metallic;
        float Roughness;
        float EmissiveStrength;
        float Pad;
    };

    struct FMaterialBinding
    {
        nvrhi::TextureHandle DiffuseTexture;
        nvrhi::TextureHandle NormalTexture;
        nvrhi::TextureHandle MetallicTexture;
        nvrhi::TextureHandle RoughnessTexture;
        nvrhi::TextureHandle AOTexture;
        FMaterialConstants Constants;
    };

    struct FMeshDrawItem
    {
        nvrhi::BufferHandle VertexBuffer;
        nvrhi::BufferHandle IndexBuffer;
        uint32_t IndexCount;
        FMaterialBinding Material;
    };

    struct FRenderDesc
    {
        FViewConstants ViewConstants;
        const FMeshDrawItem* MeshDrawItems;
        uint32_t MeshDrawItemCount;
    };

    bool Initialize(nvrhi::IDevice* InDevice, const FString& InShaderDataDir, const FDesc& Desc);
    void Render(nvrhi::ICommandList* CmdList, const FRenderDesc& Desc);
    void Resize(uint32_t Width, uint32_t Height);
    void Shutdown();

    nvrhi::TextureHandle GetDiffuseTexture() const
    {
        return GBufferDiffuseTexture;
    }
    nvrhi::TextureHandle GetSpecularTexture() const
    {
        return GBufferSpecularTexture;
    }
    nvrhi::TextureHandle GetNormalsTexture() const
    {
        return GBufferNormalsTexture;
    }
    nvrhi::TextureHandle GetEmissiveTexture() const
    {
        return GBufferEmissiveTexture;
    }
    nvrhi::TextureHandle GetDepthTexture() const
    {
        return GBufferDepthTexture;
    }
    nvrhi::IFramebuffer* GetFramebuffer() const
    {
        return GBufferFramebuffer.Get();
    }
    nvrhi::SamplerHandle GetLinearSampler() const
    {
        return GBufferLinearSampler;
    }

    FGBufferFillPass() = default;
    ~FGBufferFillPass()
    {
        Shutdown();
    }
    FGBufferFillPass(const FGBufferFillPass&) = delete;
    FGBufferFillPass& operator=(const FGBufferFillPass&) = delete;

private:
    nvrhi::IDevice* Device = nullptr;
    nvrhi::ShaderHandle GBufferVS;
    nvrhi::ShaderHandle GBufferPS;
    nvrhi::InputLayoutHandle GBufferInputLayout;
    nvrhi::BindingLayoutHandle GBufferBindingLayout;
    nvrhi::GraphicsPipelineHandle GBufferPipeline;
    nvrhi::BufferHandle ViewConstantsBuffer;
    nvrhi::BufferHandle MaterialConstantBuffer;
    nvrhi::SamplerHandle GBufferLinearSampler;
    nvrhi::TextureHandle GBufferDiffuseTexture;
    nvrhi::TextureHandle GBufferSpecularTexture;
    nvrhi::TextureHandle GBufferNormalsTexture;
    nvrhi::TextureHandle GBufferEmissiveTexture;
    nvrhi::TextureHandle GBufferDepthTexture;
    nvrhi::FramebufferHandle GBufferFramebuffer;
    bool bIsInitialized = false;
};
