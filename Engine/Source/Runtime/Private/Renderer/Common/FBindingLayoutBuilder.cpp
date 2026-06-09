// Copyright 2026 HLVM Engine
//
// MIT License

#include "Renderer/Common/FBindingLayoutBuilder.h"
#include "Core/Log.h"

DECLARE_LOG_CATEGORY(LogBindingLayout)

// --------------------------------------------------------------------------
// FBindingLayoutBuilder
// --------------------------------------------------------------------------

FBindingLayoutBuilder::FBindingLayoutBuilder()
{
    nvrhi::VulkanBindingOffsets Offsets;
    Offsets.setConstantBufferOffset(0)
           .setShaderResourceOffset(0)
           .setSamplerOffset(0)
           .setUnorderedAccessViewOffset(0);
    Desc.setBindingOffsets(Offsets);
}

FBindingLayoutBuilder& FBindingLayoutBuilder::SetVisibility(nvrhi::ShaderType Visibility)
{
    Desc.setVisibility(Visibility);
    return *this;
}

FBindingLayoutBuilder& FBindingLayoutBuilder::SetBindingOffsets(const nvrhi::VulkanBindingOffsets& Offsets)
{
    Desc.setBindingOffsets(Offsets);
    return *this;
}

FBindingLayoutBuilder& FBindingLayoutBuilder::SetBindingOffsets(
    uint32_t ConstantBufferOffset,
    uint32_t ShaderResourceOffset,
    uint32_t SamplerOffset,
    uint32_t UnorderedAccessViewOffset)
{
    nvrhi::VulkanBindingOffsets Offsets;
    Offsets.setConstantBufferOffset(ConstantBufferOffset)
           .setShaderResourceOffset(ShaderResourceOffset)
           .setSamplerOffset(SamplerOffset)
           .setUnorderedAccessViewOffset(UnorderedAccessViewOffset);
    Desc.setBindingOffsets(Offsets);
    return *this;
}

FBindingLayoutBuilder& FBindingLayoutBuilder::AddConstantBuffer(uint32_t RegisterIndex)
{
    Desc.addItem(nvrhi::BindingLayoutItem::ConstantBuffer(BRegShift + RegisterIndex));
    return *this;
}

FBindingLayoutBuilder& FBindingLayoutBuilder::AddTextureSRV(uint32_t RegisterIndex)
{
    Desc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(TRegShift + RegisterIndex));
    return *this;
}

FBindingLayoutBuilder& FBindingLayoutBuilder::AddTextureUAV(uint32_t RegisterIndex)
{
    Desc.addItem(nvrhi::BindingLayoutItem::Texture_UAV(URegShift + RegisterIndex));
    return *this;
}

FBindingLayoutBuilder& FBindingLayoutBuilder::AddStructuredBufferSRV(uint32_t RegisterIndex)
{
    Desc.addItem(nvrhi::BindingLayoutItem::StructuredBuffer_SRV(TRegShift + RegisterIndex));
    return *this;
}

FBindingLayoutBuilder& FBindingLayoutBuilder::AddStructuredBufferUAV(uint32_t RegisterIndex)
{
    Desc.addItem(nvrhi::BindingLayoutItem::StructuredBuffer_UAV(URegShift + RegisterIndex));
    return *this;
}

FBindingLayoutBuilder& FBindingLayoutBuilder::AddRawBufferSRV(uint32_t RegisterIndex)
{
    Desc.addItem(nvrhi::BindingLayoutItem::RawBuffer_SRV(TRegShift + RegisterIndex));
    return *this;
}

FBindingLayoutBuilder& FBindingLayoutBuilder::AddRawBufferUAV(uint32_t RegisterIndex)
{
    Desc.addItem(nvrhi::BindingLayoutItem::RawBuffer_UAV(URegShift + RegisterIndex));
    return *this;
}

FBindingLayoutBuilder& FBindingLayoutBuilder::AddSampler(uint32_t RegisterIndex)
{
    Desc.addItem(nvrhi::BindingLayoutItem::Sampler(SRegShift + RegisterIndex));
    return *this;
}

FBindingLayoutBuilder& FBindingLayoutBuilder::AddTypedBufferSRV(uint32_t RegisterIndex)
{
    Desc.addItem(nvrhi::BindingLayoutItem::TypedBuffer_SRV(TRegShift + RegisterIndex));
    return *this;
}

FBindingLayoutBuilder& FBindingLayoutBuilder::AddTypedBufferUAV(uint32_t RegisterIndex)
{
    Desc.addItem(nvrhi::BindingLayoutItem::TypedBuffer_UAV(URegShift + RegisterIndex));
    return *this;
}

FBindingLayoutBuilder& FBindingLayoutBuilder::AddRayTracingAccelStruct(uint32_t RegisterIndex)
{
    Desc.addItem(nvrhi::BindingLayoutItem::RayTracingAccelStruct(TRegShift + RegisterIndex));
    return *this;
}

FBindingLayoutBuilder& FBindingLayoutBuilder::AddVolatileConstantBuffer(uint32_t RegisterIndex)
{
    Desc.addItem(nvrhi::BindingLayoutItem::VolatileConstantBuffer(BRegShift + RegisterIndex));
    return *this;
}

FBindingLayoutBuilder& FBindingLayoutBuilder::AddPushConstant(uint32_t RegisterIndex, uint32_t Size)
{
    Desc.addItem(nvrhi::BindingLayoutItem::PushConstants(BRegShift + RegisterIndex, Size));
    return *this;
}

FBindingLayoutBuilder& FBindingLayoutBuilder::AddItem(const nvrhi::BindingLayoutItem& Item)
{
    Desc.addItem(Item);
    return *this;
}

nvrhi::BindingLayoutDesc FBindingLayoutBuilder::Build() const
{
    return Desc;
}

uint32_t FBindingLayoutBuilder::GetItemCount() const
{
    return static_cast<uint32_t>(Desc.bindings.size());
}

void FBindingLayoutBuilder::Reset()
{
    Desc.bindings.clear();
}

// --------------------------------------------------------------------------
// FBindingSetBuilder
// --------------------------------------------------------------------------

FBindingSetBuilder& FBindingSetBuilder::SetConstantBuffer(uint32_t RegisterIndex, nvrhi::IBuffer* Buffer)
{
    Desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(BRegShift + RegisterIndex, Buffer));
    return *this;
}

FBindingSetBuilder& FBindingSetBuilder::SetConstantBuffer(uint32_t RegisterIndex, nvrhi::BufferHandle Buffer)
{
    Desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(BRegShift + RegisterIndex, Buffer));
    return *this;
}

FBindingSetBuilder& FBindingSetBuilder::SetTextureSRV(uint32_t RegisterIndex, nvrhi::ITexture* Texture)
{
    Desc.addItem(nvrhi::BindingSetItem::Texture_SRV(TRegShift + RegisterIndex, Texture));
    return *this;
}

FBindingSetBuilder& FBindingSetBuilder::SetTextureSRV(uint32_t RegisterIndex, nvrhi::TextureHandle Texture)
{
    Desc.addItem(nvrhi::BindingSetItem::Texture_SRV(TRegShift + RegisterIndex, Texture));
    return *this;
}

FBindingSetBuilder& FBindingSetBuilder::SetTextureUAV(uint32_t RegisterIndex, nvrhi::ITexture* Texture)
{
    Desc.addItem(nvrhi::BindingSetItem::Texture_UAV(URegShift + RegisterIndex, Texture));
    return *this;
}

FBindingSetBuilder& FBindingSetBuilder::SetTextureUAV(uint32_t RegisterIndex, nvrhi::TextureHandle Texture)
{
    Desc.addItem(nvrhi::BindingSetItem::Texture_UAV(URegShift + RegisterIndex, Texture));
    return *this;
}

FBindingSetBuilder& FBindingSetBuilder::SetStructuredBufferSRV(uint32_t RegisterIndex, nvrhi::IBuffer* Buffer)
{
    Desc.addItem(nvrhi::BindingSetItem::StructuredBuffer_SRV(TRegShift + RegisterIndex, Buffer));
    return *this;
}

FBindingSetBuilder& FBindingSetBuilder::SetStructuredBufferSRV(uint32_t RegisterIndex, nvrhi::BufferHandle Buffer)
{
    Desc.addItem(nvrhi::BindingSetItem::StructuredBuffer_SRV(TRegShift + RegisterIndex, Buffer));
    return *this;
}

FBindingSetBuilder& FBindingSetBuilder::SetStructuredBufferUAV(uint32_t RegisterIndex, nvrhi::IBuffer* Buffer)
{
    Desc.addItem(nvrhi::BindingSetItem::StructuredBuffer_UAV(URegShift + RegisterIndex, Buffer));
    return *this;
}

FBindingSetBuilder& FBindingSetBuilder::SetStructuredBufferUAV(uint32_t RegisterIndex, nvrhi::BufferHandle Buffer)
{
    Desc.addItem(nvrhi::BindingSetItem::StructuredBuffer_UAV(URegShift + RegisterIndex, Buffer));
    return *this;
}

FBindingSetBuilder& FBindingSetBuilder::SetRawBufferSRV(uint32_t RegisterIndex, nvrhi::IBuffer* Buffer)
{
    Desc.addItem(nvrhi::BindingSetItem::RawBuffer_SRV(TRegShift + RegisterIndex, Buffer));
    return *this;
}

FBindingSetBuilder& FBindingSetBuilder::SetRawBufferSRV(uint32_t RegisterIndex, nvrhi::BufferHandle Buffer)
{
    Desc.addItem(nvrhi::BindingSetItem::RawBuffer_SRV(TRegShift + RegisterIndex, Buffer));
    return *this;
}

FBindingSetBuilder& FBindingSetBuilder::SetRawBufferUAV(uint32_t RegisterIndex, nvrhi::IBuffer* Buffer)
{
    Desc.addItem(nvrhi::BindingSetItem::RawBuffer_UAV(URegShift + RegisterIndex, Buffer));
    return *this;
}

FBindingSetBuilder& FBindingSetBuilder::SetRawBufferUAV(uint32_t RegisterIndex, nvrhi::BufferHandle Buffer)
{
    Desc.addItem(nvrhi::BindingSetItem::RawBuffer_UAV(URegShift + RegisterIndex, Buffer));
    return *this;
}

FBindingSetBuilder& FBindingSetBuilder::SetTypedBufferSRV(uint32_t RegisterIndex, nvrhi::IBuffer* Buffer)
{
    Desc.addItem(nvrhi::BindingSetItem::TypedBuffer_SRV(TRegShift + RegisterIndex, Buffer));
    return *this;
}

FBindingSetBuilder& FBindingSetBuilder::SetTypedBufferSRV(uint32_t RegisterIndex, nvrhi::BufferHandle Buffer)
{
    Desc.addItem(nvrhi::BindingSetItem::TypedBuffer_SRV(TRegShift + RegisterIndex, Buffer));
    return *this;
}

FBindingSetBuilder& FBindingSetBuilder::SetTypedBufferUAV(uint32_t RegisterIndex, nvrhi::IBuffer* Buffer)
{
    Desc.addItem(nvrhi::BindingSetItem::TypedBuffer_UAV(URegShift + RegisterIndex, Buffer));
    return *this;
}

FBindingSetBuilder& FBindingSetBuilder::SetTypedBufferUAV(uint32_t RegisterIndex, nvrhi::BufferHandle Buffer)
{
    Desc.addItem(nvrhi::BindingSetItem::TypedBuffer_UAV(URegShift + RegisterIndex, Buffer));
    return *this;
}

FBindingSetBuilder& FBindingSetBuilder::SetSampler(uint32_t RegisterIndex, nvrhi::ISampler* Sampler)
{
    Desc.addItem(nvrhi::BindingSetItem::Sampler(SRegShift + RegisterIndex, Sampler));
    return *this;
}

FBindingSetBuilder& FBindingSetBuilder::SetSampler(uint32_t RegisterIndex, nvrhi::SamplerHandle Sampler)
{
    Desc.addItem(nvrhi::BindingSetItem::Sampler(SRegShift + RegisterIndex, Sampler));
    return *this;
}

FBindingSetBuilder& FBindingSetBuilder::SetRayTracingAccelStruct(uint32_t RegisterIndex, nvrhi::rt::IAccelStruct* AS)
{
    Desc.addItem(nvrhi::BindingSetItem::RayTracingAccelStruct(TRegShift + RegisterIndex, AS));
    return *this;
}

FBindingSetBuilder& FBindingSetBuilder::SetRayTracingAccelStruct(uint32_t RegisterIndex, nvrhi::rt::AccelStructHandle AS)
{
    Desc.addItem(nvrhi::BindingSetItem::RayTracingAccelStruct(TRegShift + RegisterIndex, AS));
    return *this;
}

FBindingSetBuilder& FBindingSetBuilder::AddItem(const nvrhi::BindingSetItem& Item)
{
    Desc.addItem(Item);
    return *this;
}

nvrhi::BindingSetDesc FBindingSetBuilder::Build() const
{
    return Desc;
}

uint32_t FBindingSetBuilder::GetItemCount() const
{
    return static_cast<uint32_t>(Desc.bindings.size());
}

void FBindingSetBuilder::Reset()
{
    Desc.bindings.clear();
}

bool FBindingSetBuilder::ValidateAgainstLayout(const nvrhi::BindingSetDesc& SetDesc, const nvrhi::BindingLayoutDesc& LayoutDesc)
{
    if (SetDesc.bindings.size() != LayoutDesc.bindings.size())
    {
        HLVM_LOG(LogBindingLayout, err, TXT("BindingSet/Layout mismatch: set has {} items, layout expects {}"),
            static_cast<uint32_t>(SetDesc.bindings.size()),
            static_cast<uint32_t>(LayoutDesc.bindings.size()));
        return false;
    }

    for (size_t i = 0; i < SetDesc.bindings.size(); ++i)
    {
        const auto& SetItem = SetDesc.bindings[i];
        const auto& LayoutItem = LayoutDesc.bindings[i];

        if (SetItem.slot != LayoutItem.slot)
        {
            HLVM_LOG(LogBindingLayout, err, TXT("BindingSet/Layout mismatch at item {}: set slot={}, layout slot={}"),
                static_cast<uint32_t>(i), SetItem.slot, LayoutItem.slot);
            return false;
        }

        // Type compatibility check — map set-item type to layout resource type
        bool bTypeMatch = false;
        switch (LayoutItem.type)
        {
            case nvrhi::ResourceType::ConstantBuffer:
                bTypeMatch = (SetItem.type == nvrhi::ResourceType::ConstantBuffer);
                break;
            case nvrhi::ResourceType::VolatileConstantBuffer:
                bTypeMatch = (SetItem.type == nvrhi::ResourceType::VolatileConstantBuffer);
                break;
            case nvrhi::ResourceType::Texture_SRV:
                bTypeMatch = (SetItem.type == nvrhi::ResourceType::Texture_SRV);
                break;
            case nvrhi::ResourceType::Texture_UAV:
                bTypeMatch = (SetItem.type == nvrhi::ResourceType::Texture_UAV);
                break;
            case nvrhi::ResourceType::StructuredBuffer_SRV:
                bTypeMatch = (SetItem.type == nvrhi::ResourceType::StructuredBuffer_SRV);
                break;
            case nvrhi::ResourceType::StructuredBuffer_UAV:
                bTypeMatch = (SetItem.type == nvrhi::ResourceType::StructuredBuffer_UAV);
                break;
            case nvrhi::ResourceType::RawBuffer_SRV:
                bTypeMatch = (SetItem.type == nvrhi::ResourceType::RawBuffer_SRV);
                break;
            case nvrhi::ResourceType::RawBuffer_UAV:
                bTypeMatch = (SetItem.type == nvrhi::ResourceType::RawBuffer_UAV);
                break;
            case nvrhi::ResourceType::TypedBuffer_SRV:
                bTypeMatch = (SetItem.type == nvrhi::ResourceType::TypedBuffer_SRV);
                break;
            case nvrhi::ResourceType::TypedBuffer_UAV:
                bTypeMatch = (SetItem.type == nvrhi::ResourceType::TypedBuffer_UAV);
                break;
            case nvrhi::ResourceType::Sampler:
                bTypeMatch = (SetItem.type == nvrhi::ResourceType::Sampler);
                break;
            case nvrhi::ResourceType::RayTracingAccelStruct:
                bTypeMatch = (SetItem.type == nvrhi::ResourceType::RayTracingAccelStruct);
                break;
            case nvrhi::ResourceType::PushConstants:
                bTypeMatch = (SetItem.type == nvrhi::ResourceType::PushConstants);
                break;
            case nvrhi::ResourceType::SamplerFeedbackTexture_UAV:
                bTypeMatch = (SetItem.type == nvrhi::ResourceType::SamplerFeedbackTexture_UAV);
                break;
            case nvrhi::ResourceType::None:
            case nvrhi::ResourceType::Count:
                bTypeMatch = false;
                break;
            default:
                bTypeMatch = false;
                break;
        }

        if (!bTypeMatch)
        {
            HLVM_LOG(LogBindingLayout, err, TXT("BindingSet/Layout type mismatch at item {} (slot={}): set type={}, layout type={}"),
                static_cast<uint32_t>(i), SetItem.slot,
                static_cast<uint32_t>(SetItem.type), static_cast<uint32_t>(LayoutItem.type));
            return false;
        }
    }

    return true;
}
