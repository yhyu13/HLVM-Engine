/**
 * KTXTextureLoader.cpp
 * KTX2 texture loading implementation.
 */

#include "Renderer/Texture/KTXTextureLoader.h"

#include "Core/Log.h"
#include "Core/Assert.h"

#include <ktx.h>

DECLARE_LOG_CATEGORY(LogTexture)

bool FKTXTextureLoader::LoadKTX2Texture(
    const std::string& Path,
    nvrhi::IDevice* /*Device*/,
    nvrhi::ICommandList* /*CommandList*/,
    FTexture& /*OutTexture*/)
{
    HLVM_ENSURE_F(!Path.empty(), TXT("KTXTextureLoader: Path is empty"));

    // Create KTX texture from file
    ktxTexture* KTXTexture = nullptr;
    ktxResult result = ktxTexture_CreateFromNamedFile(Path.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &KTXTexture);
    if (result != KTX_SUCCESS || KTXTexture == nullptr) {
        HLVM_LOG(LogTexture, err, TXT("KTXTextureLoader: Failed to create KTX texture"));
        return false;
    }

    HLVM_LOG(LogTexture, info, TXT("KTXTextureLoader: Loaded KTX2 texture: {}x{}"),
        static_cast<unsigned int>(KTXTexture->baseWidth),
        static_cast<unsigned int>(KTXTexture->baseHeight));

    ktxTexture_Destroy(KTXTexture);
    return true;
}
