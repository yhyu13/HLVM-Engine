/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 *
 * STB Texture Loader Test
 *
 * Tests PNG/JPG texture loading using stb_image via FSTBTextureLoader.
 * This test validates CPU-side image loading without requiring Vulkan device.
 */

#include "Test.h"
#include "Renderer/Texture/STBTextureLoader.h"
#include "Renderer/RHI/Object/Texture.h"
#include "Core/Log.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image_wrapper.h"

DECLARE_LOG_CATEGORY(LogTest)

RECORD(stb_texture_loader)
{
    HLVM_LOG(LogTest, info, TXT("=== STB Texture Loader Test ==="));

    // Test 1: Local PNG
    {
        HLVM_LOG(LogTest, info, TXT("\n[Test 1] Loading local PNG: white.png"));
        const FPath TexturePath = TXT("/home/hangyu5/Documents/Gitrepo-My/HLVM-Engine/Engine/Source/Runtime/Test/TestSTBTextureLoader_Data/white.png");

        if (!FPath::Exists(TexturePath))
        {
            HLVM_LOG(LogTest, err, TXT("Local PNG file not found"));
            return;
        }

        int width, height, channels;
        stbi_uc* imageData = stbi_load(TexturePath.string().c_str(), &width, &height, &channels, STBI_default);

        if (imageData == nullptr)
        {
            HLVM_LOG(LogTest, err, TXT("Failed to load local PNG: {}"), *FString(stbi_failure_reason()));
            return;
        }

        HLVM_LOG(LogTest, info, TXT("Local PNG loaded: %dx%d channels=%d"), width, height, channels);

        if (width != 4 || height != 4 || channels != 3)
        {
            HLVM_LOG(LogTest, err, TXT("Unexpected local PNG format"));
            stbi_image_free(imageData);
            return;
        }

        stbi_image_free(imageData);
        HLVM_LOG(LogTest, info, TXT("[Test 1] PASSED"));
    }

    // Test 2: Sponza PNG
    {
        HLVM_LOG(LogTest, info, TXT("\n[Test 2] Loading Sponza PNG"));
        const FPath SponzaPngPath = TXT("/home/hangyu5/Documents/Gitrepo-My/HLVM-Engine/Samples/Assets/Sponza/glTF/16275776544635328252.png");

        if (!FPath::Exists(SponzaPngPath))
        {
            HLVM_LOG(LogTest, err, TXT("Sponza PNG file not found"));
            return;
        }

        int png_w, png_h, png_c;
        stbi_uc* png_data = stbi_load(SponzaPngPath.string().c_str(), &png_w, &png_h, &png_c, STBI_rgb_alpha);
        if (png_data == nullptr)
        {
            HLVM_LOG(LogTest, err, TXT("Failed to load Sponza PNG: {}"), *FString(stbi_failure_reason()));
            return;
        }
        HLVM_LOG(LogTest, info, TXT("Sponza PNG loaded: %dx%d channels=%d"), png_w, png_h, png_c);
        stbi_image_free(png_data);
        HLVM_LOG(LogTest, info, TXT("[Test 2] PASSED"));
    }

    // Test 3: Sponza JPG (same path that fails in TestSponzaDeferred)
    {
        HLVM_LOG(LogTest, info, TXT("\n[Test 3] Loading Sponza JPG (the failing one)"));
        const FPath SponzaJpgPath = TXT("/home/hangyu5/Documents/Gitrepo-My/HLVM-Engine/Samples/Assets/Sponza/glTF/15295713303328085182.jpg");

        if (!FPath::Exists(SponzaJpgPath))
        {
            HLVM_LOG(LogTest, err, TXT("Sponza JPG file not found"));
            return;
        }

        int jpg_w, jpg_h, jpg_c;
        stbi_uc* jpg_data = stbi_load(SponzaJpgPath.string().c_str(), &jpg_w, &jpg_h, &jpg_c, STBI_rgb_alpha);
        if (jpg_data == nullptr)
        {
            HLVM_LOG(LogTest, err, TXT("Failed to load Sponza JPG: {}"), *FString(stbi_failure_reason()));
            return;
        }
        HLVM_LOG(LogTest, info, TXT("Sponza JPG loaded: %dx%d channels=%d"), jpg_w, jpg_h, jpg_c);
        stbi_image_free(jpg_data);
        HLVM_LOG(LogTest, info, TXT("[Test 3] PASSED"));
    }

    HLVM_LOG(LogTest, info, TXT("\n=== STB Texture Loader Test ALL PASSED ==="));
}
