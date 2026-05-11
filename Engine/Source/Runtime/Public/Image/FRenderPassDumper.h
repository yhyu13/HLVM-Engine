#pragma once

#include "Core/String.h"
#include <nvrhi/nvrhi.h>
#include <cstdint>
#include <glm/glm.hpp>

class FRenderPassDumper {
public:
    // Configuration
    struct Options {
        const char* envVarName = nullptr;
        int maxFrames = 4;
        bool flipY = true;

        Options();
    };

    explicit FRenderPassDumper(const Options& options = Options());
    ~FRenderPassDumper();

    // Disable copy/move - owns resources
    FRenderPassDumper(const FRenderPassDumper&) = delete;
    FRenderPassDumper& operator=(const FRenderPassDumper&) = delete;
    FRenderPassDumper(FRenderPassDumper&&) = delete;
    FRenderPassDumper& operator=(FRenderPassDumper&&) = delete;

    // Initialize with device - parses env vars at init time
    void Initialize(nvrhi::IDevice* device, nvrhi::Format format);

    // Set test name for output directory
    void SetTestName(const FString& name);

    // Check if dumping is enabled for this frame
    bool IsEnabled() const { return mEnabled && mCurrentFrame < mMaxFrames; }

    // Get current dump frame number (1-indexed for display)
    int GetCurrentFrame() const { return mCurrentFrame + 1; }

    // Returns true if this is the last frame to dump
    bool IsLastFrame() const { return mCurrentFrame >= mMaxFrames - 1; }

    // Begin dump mode - creates staging texture if needed
    // Call before rendering the target frame
    // sourceTexture is stored for use in PrepareCopy
    void BeginDump(nvrhi::IDevice* device, nvrhi::ITexture* sourceTexture, uint32_t width, uint32_t height);

    // Phase 1: Prepare copy - call BEFORE closing caller's command list
    // Issues state transition and copyTexture on the provided command list
    // This MUST be called before the caller's close() + executeCommandList()
    void PrepareCopy(nvrhi::ICommandList* cmdList);

    // Phase 2: Readback and save - call AFTER executeCommandList of caller's command list
    // Only safe to call after GPU has finished the copy
    // Returns true if PNG was saved
    bool ReadbackAndSave();

    // Legacy compatibility - do both phases on separate command list (DO NOT USE with NVRHI immediate mode)
    // Kept for backwards compatibility but will fail if called while another command list is open
    bool EndDump();

private:
    void Shutdown();

    nvrhi::IDevice* mDevice = nullptr;
    nvrhi::StagingTextureHandle mStagingTexture;
    nvrhi::ITexture* mSourceTexture = nullptr;
    nvrhi::Format mFormat = nvrhi::Format::RGBA32_FLOAT;
    Options mOptions;
    int mMaxFrames = 4;  // Runtime max frames (can be overridden by env var)

    int mCurrentFrame = 0;
    bool mEnabled = false;
    uint32_t mWidth = 0;
    uint32_t mHeight = 0;

    FString mTestName;
    std::vector<float> mPixelBuffer;
    FString mLastFilename;

    // Calculate bytes per pixel based on format
    static constexpr size_t GetPixelSizeBytes(nvrhi::Format format) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch-enum"
        switch (format) {
            case nvrhi::Format::RGBA32_FLOAT: return 16;  // 4 × 4bytes
            case nvrhi::Format::RGBA16_FLOAT: return 8;   // 4 × 2bytes
            case nvrhi::Format::RGBA8_UNORM:  return 4;   // 4 × 1byte
            default: return 16;
        }
#pragma clang diagnostic pop
    }

    // Convert float16 (half) to float32
    static inline float Float16ToFloat32(uint16_t f16) {
        // IEEE 754 half-precision: 1 sign bit, 5 exponent bits, 10 mantissa bits
        uint32_t sign = (f16 >> 15) & 0x1;
        uint32_t exp = (f16 >> 10) & 0x1F;
        uint32_t mant = f16 & 0x3FF;

        if (exp == 0) {
            // Denormalized or zero
            return mant == 0 ? 0.0f : (sign ? -1.0f : 1.0f) * static_cast<float>(mant) / 1024.0f * 0.00006103515625f;
        } else if (exp == 31) {
            // Infinity or NaN
            return mant == 0 ? (sign ? -1.0f : 1.0f) * 1e18f : 0.0f;
        }

        // Normalized
        return (sign ? -1.0f : 1.0f) * (1.0f + static_cast<float>(mant) / 1024.0f) * glm::exp2(float(exp) - 15.0f);
    }
};
