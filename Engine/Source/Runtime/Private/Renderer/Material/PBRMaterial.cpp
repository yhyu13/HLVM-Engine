/**
 * PBRMaterial.cpp
 * GPU texture loading implementation for FPBRMaterial.
 */

#include "Renderer/Material/PBRMaterial.h"
#include "Renderer/Texture/KTXTextureLoader.h"
#include "Renderer/Texture/STBTextureLoader.h"
#include "Renderer/Texture/TextureCache.h"
#include "Renderer/Texture/AsyncTextureLoader.h"
#include "AssetManager/AssetLoader.h"
#include "Platform/FileSystem/FileSystem.h"

#include "Core/Log.h"

DECLARE_LOG_CATEGORY(LogMaterial)

bool FPBRMaterial::LoadTexture(ETextureType Type, nvrhi::IDevice* Device, nvrhi::ICommandList* CommandList, FTextureCache* TextureCache)
{
	if (!Device || !CommandList)
	{
		HLVM_LOG(LogMaterial, err, TXT("FPBRMaterial::LoadTexture: Device or CommandList is null"));
		return false;
	}

	FPath  TexturePath;
	FTexture* GPUTexture = nullptr;

	switch (Type)
	{
		case ETextureType::Albedo:
			TexturePath = AlbedoTexture;
			GPUTexture  = &AlbedoGPUTexture;
			break;
		case ETextureType::Normal:
			TexturePath = NormalTexture;
			GPUTexture  = &NormalGPUTexture;
			break;
		case ETextureType::Metallic:
			TexturePath = MetallicTexture;
			GPUTexture  = &MetallicGPUTexture;
			break;
		case ETextureType::Roughness:
			TexturePath = RoughnessTexture;
			GPUTexture  = &RoughnessGPUTexture;
			break;
		case ETextureType::AmbientOcclusion:
			TexturePath = AOTexture;
			GPUTexture  = &AOGPUTexture;
			break;
		case ETextureType::Count:
		default:
			HLVM_LOG(LogMaterial, warn, TXT("FPBRMaterial::LoadTexture: Invalid texture type"));
			return false;
	}

	if (TexturePath.empty())
	{
		HLVM_LOG(LogMaterial, debug, TXT("FPBRMaterial::LoadTexture: No path for texture type {}"), static_cast<int>(Type));
		return false;
	}

	// Check if already loaded
	if (GPUTexture->GetTextureHandle() != nullptr)
	{
		HLVM_LOG(LogMaterial, debug, TXT("FPBRMaterial::LoadTexture: Texture already loaded for type {}"), static_cast<int>(Type));
		return true;
	}

	// Check texture cache
	FPath AbsolutePath = FPath::Absolute(TexturePath);
	FTextureCache* EffectiveCache = TextureCache ? TextureCache : FAsyncTextureLoader::GetTextureCache();
	nvrhi::TextureHandle CachedTexture = EffectiveCache ? EffectiveCache->GetTexture(AbsolutePath) : nullptr;
	if (CachedTexture)
	{
		if (GPUTexture->InitializeFromHandle(CachedTexture, Device))
		{
			HLVM_LOG(LogMaterial, info, TXT("FPBRMaterial::LoadTexture: Cache hit for {}"), *AbsolutePath);
			return true;
		}
	}

	// Use KTX loader for .ktx files
	FString Extension = FPath::GetExtension(TexturePath);
	if (Extension == ".ktx" || Extension == ".KTX")
	{
		// Try KTX2 version first (ktx2/ subdirectory) since KTX1 ASTC is not supported
		FPath KTX2Path = TexturePath.parent_path() / "ktx2" / (TexturePath.stem().string() + ".ktx2");
		if (std::filesystem::exists(KTX2Path.string()))
		{
			HLVM_LOG(LogMaterial, info, TXT("FPBRMaterial::LoadTexture: Loading KTX2 texture: {}"), *KTX2Path);
			if (FKTXTextureLoader::LoadKTX2Texture(KTX2Path.string(), Device, CommandList, *GPUTexture))
			{
				HLVM_LOG(LogMaterial, info, TXT("FPBRMaterial::LoadTexture: Successfully loaded {}x{} KTX2 texture"),
					GPUTexture->GetWidth(), GPUTexture->GetHeight());
				if (EffectiveCache) { EffectiveCache->Insert(AbsolutePath, GPUTexture->GetTextureHandle()); }
				return true;
			}
			else
			{
				HLVM_LOG(LogMaterial, err, TXT("FPBRMaterial::LoadTexture: Failed to load KTX2 texture: {}"), *KTX2Path);
				return false;
			}
		}

		// Fallback to original .ktx path
		HLVM_LOG(LogMaterial, info, TXT("FPBRMaterial::LoadTexture: Loading KTX texture: {}"), *TexturePath);
		if (FKTXTextureLoader::LoadKTX2Texture(TexturePath.string(), Device, CommandList, *GPUTexture))
		{
			HLVM_LOG(LogMaterial, info, TXT("FPBRMaterial::LoadTexture: Successfully loaded {}x{} texture"),
				GPUTexture->GetWidth(), GPUTexture->GetHeight());
			if (EffectiveCache) { EffectiveCache->Insert(AbsolutePath, GPUTexture->GetTextureHandle()); }
			return true;
		}
		else
		{
			HLVM_LOG(LogMaterial, err, TXT("FPBRMaterial::LoadTexture: Failed to load KTX texture: {}"), *TexturePath);
			return false;
		}
	}

	// Use STB loader for PNG/JPG/BMP/TGA files
	if (Extension == ".png" || Extension == ".PNG" ||
		Extension == ".jpg" || Extension == ".JPG" ||
		Extension == ".jpeg" || Extension == ".JPEG" ||
		Extension == ".bmp" || Extension == ".BMP" ||
		Extension == ".tga" || Extension == ".TGA")
	{
		HLVM_LOG(LogMaterial, info, TXT("FPBRMaterial::LoadTexture: Loading STB texture: {}"), *TexturePath);

		// Use direct file path loading - stb_image works with file paths even when
		// memory-based loading fails for JPEG (due to STBI_NO_JPEG in vcpkg stb_image v2.28)
		if (FSTBTextureLoader::LoadTexture(TexturePath.string(), Device, CommandList, *GPUTexture))
		{
			HLVM_LOG(LogMaterial, info, TXT("FPBRMaterial::LoadTexture: Successfully loaded {}x{} texture"),
				GPUTexture->GetWidth(), GPUTexture->GetHeight());
			if (EffectiveCache) { EffectiveCache->Insert(AbsolutePath, GPUTexture->GetTextureHandle()); }
			return true;
		}
		else
		{
			HLVM_LOG(LogMaterial, err, TXT("FPBRMaterial::LoadTexture: Failed to decode texture: {}"), *TexturePath);
			return false;
		}
	}

	HLVM_LOG(LogMaterial, warn, TXT("FPBRMaterial::LoadTexture: Unsupported texture format: {}"), *Extension);
	return false;
}
