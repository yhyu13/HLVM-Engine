#pragma once

#include "Renderer/RHI/Object/Texture.h"
#include "Renderer/RHI/RHICommon.h"
#include "Core/String.h"

#include <ktx.h>
#include <string>

class FKTXTextureLoader
{
public:
    // Load KTX2 texture from file, returns true on success
    // Path - filesystem path to the KTX2 file
    // Device - NVRHI device
    // CommandList - NVRHI command list for upload
    // OutTexture - Output texture (filled on success)
    static bool LoadKTX2Texture(
        const std::string& Path,
        nvrhi::IDevice* Device,
        nvrhi::ICommandList* CommandList,
        FTexture& OutTexture);
};
