#pragma once

#include "Core/String.h"
#include "Platform/FileSystem/Path.h"

class FImageDump {
public:
    // Dump RGBA float buffer to PNG
    // Returns true on success
    static bool DumpToPNG(const FString& filename, int width, int height, const float* rgbaData);

    // Generate timestamped filename in the given directory
    static FString GenerateTimestampedFilename(const FString& directory);

    // Dump a test pattern to verify stb_image_write works
    // Pattern: horizontal gradient from red to green to blue
    static bool DumpTestPattern(const FString& filename, int width, int height);
};
