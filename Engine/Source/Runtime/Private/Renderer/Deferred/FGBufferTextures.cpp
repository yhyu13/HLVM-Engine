// Copyright 2026 HLVM Engine
// 
// MIT License
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "Renderer/Deferred/FGBufferTextures.h"
#include "Core/Log.h"
#include <nvrhi/utils.h>

DECLARE_LOG_CATEGORY(LogRenderer)

FGBufferTextures::~FGBufferTextures()
{
    // Clear handles to release GPU resources
    ShadedColorTexture = nullptr;
    MotionVectorsTexture = nullptr;
    GBufferFramebuffer = nullptr;
    for (size_t i = 0; i < static_cast<size_t>(EGBufferTexture::Count); ++i)
    {
        Textures[i] = nullptr;
    }
    Device = nullptr;
}

nvrhi::TextureHandle FGBufferTextures::GetTexture(EGBufferTexture Type) const
{
    size_t index = static_cast<size_t>(Type);
    if (index >= static_cast<size_t>(EGBufferTexture::Count))
    {
        return nullptr;
    }
    return Textures[index];
}

bool FGBufferTextures::Create(
    nvrhi::IDevice* InDevice,
    uint32_t InWidth,
    uint32_t InHeight,
    uint32_t InSampleCount,
    bool InbEnableMotionVectors,
    bool InbUseReverseProjection)
{
    if (bIsCreated)
    {
        HLVM_LOG(LogRenderer, warn, TXT("FGBufferTextures already created"));
        return true;
    }

    if (!InDevice)
    {
        HLVM_LOG(LogRenderer, err, TXT("FGBufferTextures::Create: device is null"));
        return false;
    }

    Device = InDevice;
    Width = InWidth;
    Height = InHeight;
    SampleCount = InSampleCount;
    bEnableMotionVectors = InbEnableMotionVectors;
    bUseReverseProjection = InbUseReverseProjection;

    // Create depth texture (D32)
    {
        nvrhi::TextureDesc depthDesc;
        depthDesc.width = Width;
        depthDesc.height = Height;
        depthDesc.sampleCount = SampleCount;
        depthDesc.format = nvrhi::Format::D32;
        depthDesc.debugName = "GBuffer_Depth";
        depthDesc.initialState = nvrhi::ResourceStates::DepthWrite;
        depthDesc.keepInitialState = true;
        depthDesc.isRenderTarget = true;
        depthDesc.setDimension(nvrhi::TextureDimension::Texture2D);

        Textures[static_cast<size_t>(EGBufferTexture::Depth)] = Device->createTexture(depthDesc);
        if (!Textures[static_cast<size_t>(EGBufferTexture::Depth)])
        {
            HLVM_LOG(LogRenderer, critical, TXT("Failed to create GBuffer depth texture"));
            return false;
        }
    }

    // Create GBufferDiffuse (RGBA8 - diffuse RGB + opacity)
    {
        nvrhi::TextureDesc desc;
        desc.width = Width;
        desc.height = Height;
        desc.sampleCount = SampleCount;
        desc.format = nvrhi::Format::RGBA8_UNORM;
        desc.debugName = "GBuffer_Diffuse";
        desc.initialState = nvrhi::ResourceStates::RenderTarget;
        desc.keepInitialState = true;
        desc.isRenderTarget = true;
        desc.setDimension(nvrhi::TextureDimension::Texture2D);

        Textures[static_cast<size_t>(EGBufferTexture::GBufferDiffuse)] = Device->createTexture(desc);
        if (!Textures[static_cast<size_t>(EGBufferTexture::GBufferDiffuse)])
        {
            HLVM_LOG(LogRenderer, critical, TXT("Failed to create GBuffer diffuse texture"));
            return false;
        }
    }

    // Create GBufferSpecular (RGBA8 - specular F0 RGB + occlusion)
    {
        nvrhi::TextureDesc desc;
        desc.width = Width;
        desc.height = Height;
        desc.sampleCount = SampleCount;
        desc.format = nvrhi::Format::RGBA8_UNORM;
        desc.debugName = "GBuffer_Specular";
        desc.initialState = nvrhi::ResourceStates::RenderTarget;
        desc.keepInitialState = true;
        desc.isRenderTarget = true;
        desc.setDimension(nvrhi::TextureDimension::Texture2D);

        Textures[static_cast<size_t>(EGBufferTexture::GBufferSpecular)] = Device->createTexture(desc);
        if (!Textures[static_cast<size_t>(EGBufferTexture::GBufferSpecular)])
        {
            HLVM_LOG(LogRenderer, critical, TXT("Failed to create GBuffer specular texture"));
            return false;
        }
    }

    // Create GBufferNormals (RGBA8 - shading normal RGB + roughness)
    {
        nvrhi::TextureDesc desc;
        desc.width = Width;
        desc.height = Height;
        desc.sampleCount = SampleCount;
        desc.format = nvrhi::Format::RGBA8_UNORM;
        desc.debugName = "GBuffer_Normals";
        desc.initialState = nvrhi::ResourceStates::RenderTarget;
        desc.keepInitialState = true;
        desc.isRenderTarget = true;
        desc.setDimension(nvrhi::TextureDimension::Texture2D);

        Textures[static_cast<size_t>(EGBufferTexture::GBufferNormals)] = Device->createTexture(desc);
        if (!Textures[static_cast<size_t>(EGBufferTexture::GBufferNormals)])
        {
            HLVM_LOG(LogRenderer, critical, TXT("Failed to create GBuffer normals texture"));
            return false;
        }
    }

    // Create GBufferEmissive (RGBA8 - emissive RGB + unused)
    {
        nvrhi::TextureDesc desc;
        desc.width = Width;
        desc.height = Height;
        desc.sampleCount = SampleCount;
        desc.format = nvrhi::Format::RGBA8_UNORM;
        desc.debugName = "GBuffer_Emissive";
        desc.initialState = nvrhi::ResourceStates::RenderTarget;
        desc.keepInitialState = true;
        desc.isRenderTarget = true;
        desc.setDimension(nvrhi::TextureDimension::Texture2D);

        Textures[static_cast<size_t>(EGBufferTexture::GBufferEmissive)] = Device->createTexture(desc);
        if (!Textures[static_cast<size_t>(EGBufferTexture::GBufferEmissive)])
        {
            HLVM_LOG(LogRenderer, critical, TXT("Failed to create GBuffer emissive texture"));
            return false;
        }
    }

    // Create optional motion vectors texture
    if (bEnableMotionVectors)
    {
        nvrhi::TextureDesc desc;
        desc.width = Width;
        desc.height = Height;
        desc.sampleCount = 1;
        desc.format = nvrhi::Format::RG16_FLOAT;
        desc.debugName = "GBuffer_MotionVectors";
        desc.initialState = nvrhi::ResourceStates::RenderTarget;
        desc.keepInitialState = true;
        desc.isRenderTarget = true;
        desc.setDimension(nvrhi::TextureDimension::Texture2D);

        MotionVectorsTexture = Device->createTexture(desc);
        if (!MotionVectorsTexture)
        {
            HLVM_LOG(LogRenderer, warn, TXT("Failed to create motion vectors texture"));
            // Non-fatal - motion vectors are optional
        }
    }

    // Create framebuffer
    if (!CreateFramebuffer())
    {
        HLVM_LOG(LogRenderer, critical, TXT("Failed to create GBuffer framebuffer"));
        return false;
    }

    bIsCreated = true;
    HLVM_LOG(LogRenderer, info, TXT("FGBufferTextures created: {}x{}"), Width, Height);
    return true;
}

bool FGBufferTextures::CreateFramebuffer()
{
    nvrhi::FramebufferDesc Desc;
    
    // Add color attachments in order: Diffuse, Specular, Normals, Emissive
    nvrhi::FramebufferAttachment DiffuseAttach;
    DiffuseAttach.setTexture(Textures[static_cast<size_t>(EGBufferTexture::GBufferDiffuse)].Get());
    Desc.addColorAttachment(DiffuseAttach);
    
    nvrhi::FramebufferAttachment SpecularAttach;
    SpecularAttach.setTexture(Textures[static_cast<size_t>(EGBufferTexture::GBufferSpecular)].Get());
    Desc.addColorAttachment(SpecularAttach);
    
    nvrhi::FramebufferAttachment NormalsAttach;
    NormalsAttach.setTexture(Textures[static_cast<size_t>(EGBufferTexture::GBufferNormals)].Get());
    Desc.addColorAttachment(NormalsAttach);
    
    nvrhi::FramebufferAttachment EmissiveAttach;
    EmissiveAttach.setTexture(Textures[static_cast<size_t>(EGBufferTexture::GBufferEmissive)].Get());
    Desc.addColorAttachment(EmissiveAttach);
    
    // Set depth attachment
    nvrhi::FramebufferAttachment DepthAttach;
    DepthAttach.setTexture(Textures[static_cast<size_t>(EGBufferTexture::Depth)].Get());
    Desc.setDepthAttachment(DepthAttach);

    GBufferFramebuffer = Device->createFramebuffer(Desc);
    if (!GBufferFramebuffer)
    {
        HLVM_LOG(LogRenderer, critical, TXT("Failed to create GBuffer framebuffer"));
        return false;
    }

    return true;
}

bool FGBufferTextures::CreateShadedColorTexture(nvrhi::IDevice* InDevice)
{
    if (!InDevice)
    {
        HLVM_LOG(LogRenderer, err, TXT("CreateShadedColorTexture: device is null"));
        return false;
    }

    if (!bIsCreated)
    {
        HLVM_LOG(LogRenderer, err, TXT("CreateShadedColorTexture: GBuffer not created yet"));
        return false;
    }

    if (ShadedColorTexture)
    {
        HLVM_LOG(LogRenderer, warn, TXT("ShadedColorTexture already created"));
        return true;
    }

nvrhi::TextureDesc desc;
desc.width = Width;
desc.height = Height;
desc.sampleCount = 1; // Final shaded color is never MSAA
desc.format = nvrhi::Format::RGBA16_FLOAT;  // HDR format for deferred lighting (Donut compatible)
desc.debugName = "ShadedColor";
desc.initialState = nvrhi::ResourceStates::UnorderedAccess;
desc.keepInitialState = true;
desc.isUAV = true;  // This texture is written by compute shader
desc.isRenderTarget = false;  // Not a render target
desc.setDimension(nvrhi::TextureDimension::Texture2D);

    ShadedColorTexture = InDevice->createTexture(desc);
    if (!ShadedColorTexture)
    {
        HLVM_LOG(LogRenderer, critical, TXT("Failed to create shaded color texture"));
        return false;
    }

    HLVM_LOG(LogRenderer, info, TXT("ShadedColorTexture created: {}x{}"), Width, Height);
    return true;
}

void FGBufferTextures::Clear(nvrhi::ICommandList* CommandList)
{
    if (!CommandList || !bIsCreated)
    {
        return;
    }

    // Clear depth to 1.0 (far plane / infinity)
    nvrhi::utils::ClearDepthStencilAttachment(
        CommandList,
        GBufferFramebuffer,
        1.0f,
        0u);

    // Clear GBuffer color attachments to zeros
    nvrhi::Color clearColor = nvrhi::Color(0.0f, 0.0f, 0.0f, 0.0f);
    
    for (size_t i = 1; i < static_cast<size_t>(EGBufferTexture::Count); ++i)
    {
        nvrhi::utils::ClearColorAttachment(
            CommandList,
            GBufferFramebuffer,
            static_cast<uint32_t>(i - 1),
            clearColor);
    }
}
