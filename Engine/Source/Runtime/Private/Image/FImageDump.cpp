#include "Image/FImageDump.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write_wrapper.h>
#include <vector>
#include <chrono>
#include <ctime>
#include <cstring>
#include <algorithm>
#include <cmath>

bool FImageDump::DumpToPNG(const FString& filename, int width, int height, const float* rgbaData) {
    // Convert float RGBA [0,1] to uint8 RGBA [0,255]
    std::vector<uint8_t> pixels(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
    for (int i = 0; i < width * height; i++) {
        size_t idx = static_cast<size_t>(i) * 4;
        pixels[idx + 0] = static_cast<uint8_t>(std::clamp(rgbaData[i * 4 + 0] * 255.0f, 0.0f, 255.0f));
        pixels[idx + 1] = static_cast<uint8_t>(std::clamp(rgbaData[i * 4 + 1] * 255.0f, 0.0f, 255.0f));
        pixels[idx + 2] = static_cast<uint8_t>(std::clamp(rgbaData[i * 4 + 2] * 255.0f, 0.0f, 255.0f));
        pixels[idx + 3] = 255;
    }

    int stride = width * 4;
    // stb_image_write needs const char*, FString uses TCHAR (char8_t on Linux)
    std::string narrowFilename(filename.begin(), filename.end());
    int result = stbi_write_png(narrowFilename.c_str(), width, height, 4, pixels.data(), stride);
    return result != 0;
}

FString FImageDump::GenerateTimestampedFilename(const FString& directory) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm tm = {};
#if defined(_WIN32) || defined(_WIN64)
    localtime_s(&tm, &time_t);
#else
    localtime_r(&time_t, &tm);
#endif

    char buffer[128];
    snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d_%02d-%02d-%02d_%03lld.png",
        tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
        tm.tm_hour, tm.tm_min, tm.tm_sec, static_cast<long long>(ms.count()));

    return FPath::Combine(directory, FString(buffer));
}

bool FImageDump::DumpTestPattern(const FString& filename, int width, int height) {
    // Create a test pattern: horizontal gradient from red (left) to green (center) to blue (right)
    std::vector<uint8_t> pixels(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            size_t idx = (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4;
            float t = static_cast<float>(x) / static_cast<float>(width);

            if (t < 0.5f) {
                // Left half: red gradient (1,0,0) to (0,1,0)
                float localT = t * 2.0f;
                pixels[idx + 0] = static_cast<uint8_t>((1.0f - localT) * 255.0f);
                pixels[idx + 1] = static_cast<uint8_t>(localT * 255.0f);
                pixels[idx + 2] = 0;
            } else {
                // Right half: green gradient (0,1,0) to (0,0,1)
                float localT = (t - 0.5f) * 2.0f;
                pixels[idx + 0] = 0;
                pixels[idx + 1] = static_cast<uint8_t>((1.0f - localT) * 255.0f);
                pixels[idx + 2] = static_cast<uint8_t>(localT * 255.0f);
            }
            pixels[idx + 3] = 255;
        }
    }

    int stride = width * 4;
    std::string narrowFilename(filename.begin(), filename.end());
    int result = stbi_write_png(narrowFilename.c_str(), width, height, 4, pixels.data(), stride);
    return result != 0;
}
