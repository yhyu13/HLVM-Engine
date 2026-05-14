#include "Image/FRenderPassDumper.h"
#include "Image/FImageDump.h"
#include "Core/Log.h"
#include <cstring>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image_wrapper.h>

DECLARE_LOG_CATEGORY(LogImageDump)

FRenderPassDumper::Options::Options() {
    envVarName = "HLVM_DUMP_RT";
}

FRenderPassDumper::FRenderPassDumper(const Options& options)
    : mOptions(options) {
}

FRenderPassDumper::~FRenderPassDumper() {
    Shutdown();
}

void FRenderPassDumper::Shutdown() {
    mStagingTexture = nullptr;
    mSourceTexture = nullptr;
    mDevice = nullptr;
    mPixelBuffer.clear();
    mWidth = 0;
    mHeight = 0;
}

void FRenderPassDumper::Initialize(nvrhi::IDevice* device, nvrhi::Format format) {
    mDevice = device;
    mFormat = format;

    // Initialize maxFrames from Options
    mMaxFrames = mOptions.maxFrames;

    // Parse env vars
    const char* envVal = getenv(mOptions.envVarName);
    mEnabled = (envVal != nullptr && envVal[0] != '\0');

    if (mEnabled) {
        const char* framesEnv = getenv("HLVM_DUMP_FRAMES");
        if (framesEnv) {
            int frames = atoi(framesEnv);
            if (frames > 0 && frames <= 60) {
                mMaxFrames = frames;  // Override from env var
            }
        }
        HLVM_LOG(LogImageDump, info, TXT("Frame dump enabled: {} frames"), mMaxFrames);
    }
}

void FRenderPassDumper::SetTestName(const FString& name) {
    mTestName = name;
}

void FRenderPassDumper::BeginDump(nvrhi::IDevice* device, nvrhi::ITexture* sourceTexture, uint32_t width, uint32_t height) {
    if (!mEnabled) return;

    mSourceTexture = sourceTexture;

    if (!mStagingTexture || mWidth != width || mHeight != height) {
        nvrhi::TextureDesc desc;
        desc.dimension = nvrhi::TextureDimension::Texture2D;
        desc.width = width;
        desc.height = height;
        desc.format = mFormat;
        desc.isRenderTarget = false;
        desc.isUAV = false;
        desc.isTypeless = false;
        desc.initialState = nvrhi::ResourceStates::CopyDest;
        desc.keepInitialState = false;
        desc.debugName = "FrameDumpStaging";
        mStagingTexture = device->createStagingTexture(desc, nvrhi::CpuAccessMode::Read);
        mWidth = width;
        mHeight = height;
        mPixelBuffer.resize(static_cast<size_t>(width) * height * 4);
    }
}

void FRenderPassDumper::PrepareCopy(nvrhi::ICommandList* cmdList) {
    if (!mEnabled || !mStagingTexture || !mSourceTexture) {
        return;
    }

    // Transition source to CopySource state
    cmdList->setTextureState(mSourceTexture, nvrhi::AllSubresources,
        nvrhi::ResourceStates::CopySource);

    // Copy from source to staging
    nvrhi::TextureSlice slice = {};
    slice.width = mWidth;
    slice.height = mHeight;
    slice.depth = 1;
    cmdList->copyTexture(mStagingTexture.Get(), slice, mSourceTexture, slice);
}

bool FRenderPassDumper::ReadbackAndSave() {
    if (!mEnabled || mCurrentFrame >= mMaxFrames) {
        return false;
    }

    if (!mStagingTexture || !mSourceTexture) {
        HLVM_LOG(LogImageDump, err, TXT("FrameDumper: ReadbackAndSave called without BeginDump/PrepareCopy"));
        return false;
    }

    // Read back pixels
    nvrhi::TextureSlice slice = {};
    slice.width = mWidth;
    slice.height = mHeight;
    slice.depth = 1;

    size_t rowPitch = 0;
    void* mappedData = mDevice->mapStagingTexture(
        mStagingTexture, slice, nvrhi::CpuAccessMode::Read, &rowPitch);

    if (!mappedData) {
        HLVM_LOG(LogImageDump, err, TXT("FrameDumper: Failed to map staging texture"));
        return false;
    }

    uint8_t* srcRow = reinterpret_cast<uint8_t*>(mappedData);
    float* dst = mPixelBuffer.data();

    size_t pixelSizeBytes = GetPixelSizeBytes(mFormat);
    bool is16Float = (mFormat == nvrhi::Format::RGBA16_FLOAT);
    bool is8Unorm  = (mFormat == nvrhi::Format::RGBA8_UNORM);

    for (uint32_t y = 0; y < mHeight; y++) {
        uint8_t* srcRowBase;
        if (mOptions.flipY) {
            // Vulkan: row 0 at bottom, PNG: row 0 at top
            srcRowBase = srcRow + (mHeight - 1 - y) * rowPitch;
        } else {
            srcRowBase = srcRow + y * rowPitch;
        }
        for (uint32_t x = 0; x < mWidth; x++) {
            size_t dstIdx = (static_cast<size_t>(y) * mWidth + x) * 4;
            size_t srcIdx = x * pixelSizeBytes;

            if (is16Float) {
                // RGBA16_FLOAT - read as uint16_t and convert to float32
                uint16_t* src16 = reinterpret_cast<uint16_t*>(srcRowBase + srcIdx);
                dst[dstIdx + 0] = Float16ToFloat32(src16[0]);
                dst[dstIdx + 1] = Float16ToFloat32(src16[1]);
                dst[dstIdx + 2] = Float16ToFloat32(src16[2]);
                dst[dstIdx + 3] = Float16ToFloat32(src16[3]);
            } else if (is8Unorm) {
                // RGBA8_UNORM - read as uint8_t and normalize to [0,1]
                uint8_t* src = srcRowBase + srcIdx;
                dst[dstIdx + 0] = static_cast<float>(src[0]) / 255.0f;
                dst[dstIdx + 1] = static_cast<float>(src[1]) / 255.0f;
                dst[dstIdx + 2] = static_cast<float>(src[2]) / 255.0f;
                dst[dstIdx + 3] = static_cast<float>(src[3]) / 255.0f;
            } else {
                // RGBA32_FLOAT - read directly as float
                float* src = reinterpret_cast<float*>(srcRowBase + srcIdx);
                dst[dstIdx + 0] = src[0];
                dst[dstIdx + 1] = src[1];
                dst[dstIdx + 2] = src[2];
                dst[dstIdx + 3] = src[3];
            }
        }
    }

    mDevice->unmapStagingTexture(mStagingTexture);

    // Generate output path
    FString outputDir;
    const char* dirEnv = getenv("HLVM_DUMP_DIR");
    if (dirEnv && dirEnv[0] != '\0') {
        outputDir = FString(dirEnv);
    } else {
        outputDir = FString::Format(
            TXT("{}/../../Test/{}_Data"),
            *GExecutablePath, *mTestName);
    }

    FString baseFilename = FImageDump::GenerateTimestampedFilename(outputDir);
    mLastFilename = FString::Format(
        TXT("{}_frame{:04d}.png"),
        baseFilename.substr(0, baseFilename.length() - 4).c_str(),
        mCurrentFrame + 1);

    // Dump to PNG - cast uint32_t to int for API compatibility
    int imgWidth = static_cast<int>(mWidth);
    int imgHeight = static_cast<int>(mHeight);
    if (FImageDump::DumpToPNG(mLastFilename, imgWidth, imgHeight, mPixelBuffer.data())) {
        mCurrentFrame++;
        return true;
    }

    HLVM_LOG(LogImageDump, err, TXT("FrameDumper: Failed to dump to {}"), *mLastFilename);
    return false;
}

bool FRenderPassDumper::EndDump() {
    // Legacy compatibility method - DO NOT USE with NVRHI immediate mode
    // This creates its own command list and will fail if another command list is open
    if (!mEnabled || mCurrentFrame >= mMaxFrames) {
        return false;
    }

    if (!mStagingTexture || !mSourceTexture) {
        HLVM_LOG(LogImageDump, err, TXT("FrameDumper: EndDump called without BeginDump"));
        return false;
    }

    // Create our own command list for the copy operation
    // WARNING: This will fail if the caller still has a command list open!
    nvrhi::CommandListHandle copyCmdList = mDevice->createCommandList();
    copyCmdList->open();

    // Use PrepareCopy logic
    PrepareCopy(copyCmdList);

    copyCmdList->close();
    mDevice->executeCommandList(copyCmdList);

    // Then readback
    return ReadbackAndSave();
}

bool FRenderPassDumper::CompareAgainstReference(const FString& referencePath, float thresholdMSE) {
    if (mPixelBuffer.empty() || mWidth == 0 || mHeight == 0) {
        HLVM_LOG(LogImageDump, warn, TXT("Regression: No pixel data to compare"));
        return false;
    }

    // Load reference PNG via stb_image
    int refW = 0, refH = 0, refChannels = 0;
    std::string narrowPath(referencePath.begin(), referencePath.end());
    unsigned char* refData = stbi_load(narrowPath.c_str(), &refW, &refH, &refChannels, 4);  // force RGBA

    if (!refData) {
        HLVM_LOG(LogImageDump, warn, TXT("Regression: Reference image not found: {}"), *referencePath);
        return false;
    }

    if (refW != static_cast<int>(mWidth) || refH != static_cast<int>(mHeight)) {
        HLVM_LOG(LogImageDump, warn,
            TXT("Regression: Resolution mismatch: ref={}x{} vs dump={}x{}"),
            refW, refH, mWidth, mHeight);
        stbi_image_free(refData);
        return false;
    }

    // Compute MSE over RGB channels (skip alpha)
    double sse = 0.0;
    size_t pixelCount = static_cast<size_t>(mWidth) * mHeight;
    for (size_t i = 0; i < pixelCount; i++) {
        for (size_t c = 0; c < 3; c++) {
            float ref = refData[i * 4 + c] / 255.0f;
            float dump = mPixelBuffer[i * 4 + c];
            float diff = ref - dump;
            sse += static_cast<double>(diff) * static_cast<double>(diff);
        }
    }
    float mse = static_cast<float>(sse / static_cast<double>(pixelCount * 3));

    bool pass = mse < thresholdMSE;
    HLVM_LOG(LogImageDump, info,
        TXT("Regression: MSE={:.6f}, threshold={:.6f}, result={}"),
        mse, thresholdMSE, pass ? TXT("PASS") : TXT("FAIL"));

    stbi_image_free(refData);
    return pass;
}
