// Copyright 2026 HLVM Engine
//
// MIT License

#pragma once

#include <nvrhi/nvrhi.h>

/**
 * @brief Centralized, shift-aware binding layout builder
 *
 * FBindingLayoutBuilder eliminates manual Vulkan binding math errors by
 * accepting logical HLSL register indices (b0, t1, s2, etc.) and computing
 * the correct SPIR-V binding automatically using the shader compiler's
 * register shift constants.
 *
 * ShaderMake/NVRHI shift convention:
 *   --tRegShift 0   → SRV  textures/buffers  start at binding 0
 *   --sRegShift 128 → samplers             start at binding 128
 *   --bRegShift 256 → constant buffers     start at binding 256
 *   --uRegShift 384 → UAVs                 start at binding 384
 *
 * Usage:
 *   FBindingLayoutBuilder Builder;
 *   Builder.SetVisibility(nvrhi::ShaderType::Compute)
 *          .AddConstantBuffer(0)      // register(b0) → binding 256
 *          .AddTextureSRV(0)          // register(t0) → binding 0
 *          .AddSampler(0);            // register(s0) → binding 128
 *   nvrhi::BindingLayoutDesc Desc = Builder.Build();
 */
class FBindingLayoutBuilder
{
public:
    static constexpr uint32_t TRegShift = 0;
    static constexpr uint32_t SRegShift = 128;
    static constexpr uint32_t BRegShift = 256;
    static constexpr uint32_t URegShift = 384;

    FBindingLayoutBuilder();

    /**
     * @brief Set shader visibility for all bindings in this layout
     */
    FBindingLayoutBuilder& SetVisibility(nvrhi::ShaderType Visibility);

    /**
     * @brief Set custom Vulkan binding offsets (default: all zeros)
     *
     * The default offsets match ShaderMake's shift configuration:
     *   CBV offset = 0, SRV offset = 0, Sampler offset = 0, UAV offset = 0
     *
     * If your shader compiler uses different shifts, set them here.
     */
    FBindingLayoutBuilder& SetBindingOffsets(const nvrhi::VulkanBindingOffsets& Offsets);
    FBindingLayoutBuilder& SetBindingOffsets(
        uint32_t ConstantBufferOffset,
        uint32_t ShaderResourceOffset,
        uint32_t SamplerOffset,
        uint32_t UnorderedAccessViewOffset);

    /**
     * @brief Add a constant buffer (register bN → binding BRegShift + N)
     */
    FBindingLayoutBuilder& AddConstantBuffer(uint32_t RegisterIndex);

    /**
     * @brief Add a texture SRV (register tN → binding TRegShift + N)
     */
    FBindingLayoutBuilder& AddTextureSRV(uint32_t RegisterIndex);

    /**
     * @brief Add a texture UAV (register uN → binding URegShift + N)
     */
    FBindingLayoutBuilder& AddTextureUAV(uint32_t RegisterIndex);

    /**
     * @brief Add a structured buffer SRV (register tN → binding TRegShift + N)
     */
    FBindingLayoutBuilder& AddStructuredBufferSRV(uint32_t RegisterIndex);

    /**
     * @brief Add a structured buffer UAV (register uN → binding URegShift + N)
     */
    FBindingLayoutBuilder& AddStructuredBufferUAV(uint32_t RegisterIndex);

    /**
     * @brief Add a raw buffer SRV (register tN → binding TRegShift + N)
     */
    FBindingLayoutBuilder& AddRawBufferSRV(uint32_t RegisterIndex);

    /**
     * @brief Add a raw buffer UAV (register uN → binding URegShift + N)
     */
    FBindingLayoutBuilder& AddRawBufferUAV(uint32_t RegisterIndex);

    /**
     * @brief Add a sampler (register sN → binding SRegShift + N)
     */
    FBindingLayoutBuilder& AddSampler(uint32_t RegisterIndex);

    /**
     * @brief Add a typed buffer SRV (register tN → binding TRegShift + N)
     */
    FBindingLayoutBuilder& AddTypedBufferSRV(uint32_t RegisterIndex);

    /**
     * @brief Add a typed buffer UAV (register uN → binding URegShift + N)
     */
    FBindingLayoutBuilder& AddTypedBufferUAV(uint32_t RegisterIndex);

    /**
     * @brief Add a ray tracing acceleration structure SRV (register tN → binding TRegShift + N)
     */
    FBindingLayoutBuilder& AddRayTracingAccelStruct(uint32_t RegisterIndex);

    /**
     * @brief Add a custom/volatile constant buffer (register bN → binding BRegShift + N)
     */
    FBindingLayoutBuilder& AddVolatileConstantBuffer(uint32_t RegisterIndex);

    /**
     * @brief Add a push constant (register bN → binding BRegShift + N)
     */
    FBindingLayoutBuilder& AddPushConstant(uint32_t RegisterIndex, uint32_t Size);

    /**
     * @brief Add a raw BindingLayoutItem (advanced use — bypasses shift math)
     *
     * Prefer the typed Add* methods. This is for edge cases only.
     */
    FBindingLayoutBuilder& AddItem(const nvrhi::BindingLayoutItem& Item);

    /**
     * @brief Build the final BindingLayoutDesc
     */
    [[nodiscard]] nvrhi::BindingLayoutDesc Build() const;

    /**
     * @brief Get the number of items added so far
     */
    [[nodiscard]] uint32_t GetItemCount() const;

    /**
     * @brief Reset to empty state (reuse builder)
     */
    void Reset();

private:
    nvrhi::BindingLayoutDesc Desc;
};

/**
 * @brief Centralized, shift-aware binding set builder
 *
 * FBindingSetBuilder mirrors FBindingLayoutBuilder for constructing BindingSetDesc.
 * It uses the same shift constants so layout and set are guaranteed to match.
 *
 * Usage:
 *   FBindingSetBuilder Builder;
 *   Builder.SetConstantBuffer(0, ViewConstantsBuffer)  // register(b0)
 *          .SetTextureSRV(0, DiffuseTexture)           // register(t0)
 *          .SetSampler(0, LinearSampler);              // register(s0)
 *   nvrhi::BindingSetDesc Desc = Builder.Build();
 */
class FBindingSetBuilder
{
public:
    static constexpr uint32_t TRegShift = FBindingLayoutBuilder::TRegShift;
    static constexpr uint32_t SRegShift = FBindingLayoutBuilder::SRegShift;
    static constexpr uint32_t BRegShift = FBindingLayoutBuilder::BRegShift;
    static constexpr uint32_t URegShift = FBindingLayoutBuilder::URegShift;

    FBindingSetBuilder() = default;

    FBindingSetBuilder& SetConstantBuffer(uint32_t RegisterIndex, nvrhi::IBuffer* Buffer);
    FBindingSetBuilder& SetConstantBuffer(uint32_t RegisterIndex, nvrhi::BufferHandle Buffer);

    FBindingSetBuilder& SetTextureSRV(uint32_t RegisterIndex, nvrhi::ITexture* Texture);
    FBindingSetBuilder& SetTextureSRV(uint32_t RegisterIndex, nvrhi::TextureHandle Texture);

    FBindingSetBuilder& SetTextureUAV(uint32_t RegisterIndex, nvrhi::ITexture* Texture);
    FBindingSetBuilder& SetTextureUAV(uint32_t RegisterIndex, nvrhi::TextureHandle Texture);

    FBindingSetBuilder& SetStructuredBufferSRV(uint32_t RegisterIndex, nvrhi::IBuffer* Buffer);
    FBindingSetBuilder& SetStructuredBufferSRV(uint32_t RegisterIndex, nvrhi::BufferHandle Buffer);

    FBindingSetBuilder& SetStructuredBufferUAV(uint32_t RegisterIndex, nvrhi::IBuffer* Buffer);
    FBindingSetBuilder& SetStructuredBufferUAV(uint32_t RegisterIndex, nvrhi::BufferHandle Buffer);

    FBindingSetBuilder& SetRawBufferSRV(uint32_t RegisterIndex, nvrhi::IBuffer* Buffer);
    FBindingSetBuilder& SetRawBufferSRV(uint32_t RegisterIndex, nvrhi::BufferHandle Buffer);

    FBindingSetBuilder& SetRawBufferUAV(uint32_t RegisterIndex, nvrhi::IBuffer* Buffer);
    FBindingSetBuilder& SetRawBufferUAV(uint32_t RegisterIndex, nvrhi::BufferHandle Buffer);

    FBindingSetBuilder& SetTypedBufferSRV(uint32_t RegisterIndex, nvrhi::IBuffer* Buffer);
    FBindingSetBuilder& SetTypedBufferSRV(uint32_t RegisterIndex, nvrhi::BufferHandle Buffer);

    FBindingSetBuilder& SetTypedBufferUAV(uint32_t RegisterIndex, nvrhi::IBuffer* Buffer);
    FBindingSetBuilder& SetTypedBufferUAV(uint32_t RegisterIndex, nvrhi::BufferHandle Buffer);

    FBindingSetBuilder& SetSampler(uint32_t RegisterIndex, nvrhi::ISampler* Sampler);
    FBindingSetBuilder& SetSampler(uint32_t RegisterIndex, nvrhi::SamplerHandle Sampler);

    FBindingSetBuilder& SetRayTracingAccelStruct(uint32_t RegisterIndex, nvrhi::rt::IAccelStruct* AS);
    FBindingSetBuilder& SetRayTracingAccelStruct(uint32_t RegisterIndex, nvrhi::rt::AccelStructHandle AS);

    /**
     * @brief Add a raw BindingSetItem (advanced use — bypasses shift math)
     */
    FBindingSetBuilder& AddItem(const nvrhi::BindingSetItem& Item);

    /**
     * @brief Build the final BindingSetDesc
     */
    [[nodiscard]] nvrhi::BindingSetDesc Build() const;

    /**
     * @brief Get the number of items added so far
     */
    [[nodiscard]] uint32_t GetItemCount() const;

    /**
     * @brief Reset to empty state (reuse builder)
     */
    void Reset();

    /**
     * @brief Debug validation: verify a BindingSetDesc matches a BindingLayoutDesc.
     * Checks item count, slot alignment, and type compatibility.
     * Logs errors and returns false on mismatch.
     */
    static bool ValidateAgainstLayout(const nvrhi::BindingSetDesc& SetDesc, const nvrhi::BindingLayoutDesc& LayoutDesc);

private:
    nvrhi::BindingSetDesc Desc;
};
