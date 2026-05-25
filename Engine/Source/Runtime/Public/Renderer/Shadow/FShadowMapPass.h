#pragma once

#include "Core/String.h"
#include <nvrhi/nvrhi.h>
#include <glm/glm.hpp>

class FShadowMapPass
{
public:
    struct FDesc
    {
        uint32_t ShadowMapSize = 2048;
        nvrhi::Format DepthFormat = nvrhi::Format::D32;
    };

    struct FShadowConstants
    {
        float ModelMatrix[16];
        float LightViewProj[16];
    };

    struct FMeshDrawItem
    {
        nvrhi::BufferHandle VertexBuffer;
        nvrhi::BufferHandle IndexBuffer;
        uint32_t IndexCount;
    };

    struct FRenderDesc
    {
        glm::mat4 LightViewProj;
        glm::mat4 ModelMatrix;
        const FMeshDrawItem* MeshDrawItems;
        uint32_t MeshDrawItemCount;
    };

    struct FInstancedMeshDrawItem
    {
        nvrhi::BufferHandle VertexBuffer;
        nvrhi::BufferHandle IndexBuffer;
        nvrhi::BufferHandle InstanceBuffer;
        uint32_t IndexCount = 0;
        uint32_t InstanceCount = 0;
    };

    struct FInstancedRenderDesc
    {
        glm::mat4 LightViewProj;
        const FInstancedMeshDrawItem* InstancedItems = nullptr;
        uint32_t InstancedItemCount = 0;
    };

    bool Initialize(nvrhi::IDevice* InDevice, const FString& InShaderDataDir, const FDesc& Desc);
    void Render(nvrhi::ICommandList* CmdList, const FRenderDesc& Desc);
    void RenderInstanced(nvrhi::ICommandList* CmdList, const FInstancedRenderDesc& Desc);
    void Shutdown();

    nvrhi::TextureHandle GetShadowMapTexture() const
    {
        return ShadowMapTexture;
    }
    nvrhi::SamplerHandle GetShadowSampler() const
    {
        return ShadowSampler;
    }

    FShadowMapPass() = default;
    ~FShadowMapPass()
    {
        Shutdown();
    }
    FShadowMapPass(const FShadowMapPass&) = delete;
    FShadowMapPass& operator=(const FShadowMapPass&) = delete;

private:
    nvrhi::IDevice* Device = nullptr;
    nvrhi::ShaderHandle ShadowVS;
    nvrhi::InputLayoutHandle ShadowInputLayout;
    nvrhi::BindingLayoutHandle ShadowBindingLayout;
    nvrhi::GraphicsPipelineHandle ShadowPipeline;
    nvrhi::ShaderHandle ShadowInstancedVS;
    nvrhi::BindingLayoutHandle ShadowInstancedBindingLayout;
    nvrhi::GraphicsPipelineHandle ShadowInstancedPipeline;
    nvrhi::TextureHandle ShadowMapTexture;
    nvrhi::FramebufferHandle ShadowMapFramebuffer;
    nvrhi::BufferHandle ShadowConstantsBuffer;
    nvrhi::SamplerHandle ShadowSampler;
    uint32_t ShadowMapSize = 2048;
    bool bIsInitialized = false;
};
