/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#pragma once

#include "Math/MathGLM.h"
#include "Platform/FileSystem/Path.h"
#include "Renderer/Material/IMaterial.h"
#include "Renderer/RHI/Object/Texture.h"
#include <memory>

/**
 * @brief Physically Based Rendering material
 *
 * Simplified PBR material with metallic-roughness workflow.
 * Supports texture maps and constant color factors.
 */
class FPBRMaterial : public IMaterial
{
public:
	FPBRMaterial() = default;

	explicit FPBRMaterial(const FString& InName)
	{
		Name = InName;
	}

	~FPBRMaterial() override = default;

	//! Check if material has texture of specified type
	bool HasTexture(ETextureType Type) const override
	{
		switch (Type)
		{
			case ETextureType::Albedo:
				return !AlbedoTexture.empty();
			case ETextureType::Normal:
				return !NormalTexture.empty();
			case ETextureType::Metallic:
				return !MetallicTexture.empty();
			case ETextureType::Roughness:
				return !RoughnessTexture.empty();
			case ETextureType::AmbientOcclusion:
				return !AOTexture.empty();
			default:
				return false;
			case ETextureType::Count:
				return false;
		}
	}

	//! Get texture path for specified type
	FPath GetTexturePath(ETextureType Type) const override
	{
		switch (Type)
		{
			case ETextureType::Albedo:
				return AlbedoTexture;
			case ETextureType::Normal:
				return NormalTexture;
			case ETextureType::Metallic:
				return MetallicTexture;
			case ETextureType::Roughness:
				return RoughnessTexture;
			case ETextureType::AmbientOcclusion:
				return AOTexture;
			default:
				return FPath();
			case ETextureType::Count:
				return FPath();
		}
	}

	//! Get texture name for specified type
	FString GetTextureName(ETextureType Type) const override
	{
		switch (Type)
		{
			case ETextureType::Albedo:
				return AlbedoTextureName;
			case ETextureType::Normal:
				return NormalTextureName;
			case ETextureType::Metallic:
				return MetallicTextureName;
			case ETextureType::Roughness:
				return RoughnessTextureName;
			case ETextureType::AmbientOcclusion:
				return AOTextureName;
			default:
				return FString();
			case ETextureType::Count:
				return FString();
		}
	}

	//! Common PBR properties
	FVec3 GetAlbedoColor() const override
	{
		return AlbedoColor;
	}

	//! Get metallic factor
	float GetMetallic() const override
	{
		return Metallic;
	}

	//! Get roughness factor
	float GetRoughness() const override
	{
		return Roughness;
	}

	//! Set texture path for a specific type
	void SetTexture(const FString& TextureName, const FPath& TexturePath, ETextureType Type)
	{
		HLVM_ENSURE_F(static_cast<size_t>(Type) < static_cast<size_t>(ETextureType::Count),
			TXT("Invalid texture type"));

		switch (Type)
		{
			case ETextureType::Albedo:
				AlbedoTexture = TexturePath;
				AlbedoTextureName = TextureName;
				break;
			case ETextureType::Normal:
				NormalTexture = TexturePath;
				NormalTextureName = TextureName;
				break;
			case ETextureType::Metallic:
				MetallicTexture = TexturePath;
				MetallicTextureName = TextureName;
				break;
			case ETextureType::Roughness:
				RoughnessTexture = TexturePath;
				RoughnessTextureName = TextureName;
				break;
			case ETextureType::AmbientOcclusion:
				AOTexture = TexturePath;
				AOTextureName = TextureName;
				break;
			default:
				break;
			case ETextureType::Count:
				break;
		}
	}

	//! Check if material has albedo texture
	bool HasAlbedoTexture() const
	{
		return !AlbedoTexture.empty();
	}

	//! Check if material has normal texture
	bool HasNormalTexture() const
	{
		return !NormalTexture.empty();
	}

	//! Check if material has metallic texture
	bool HasMetallicTexture() const
	{
		return !MetallicTexture.empty();
	}

	//! Check if material has roughness texture
	bool HasRoughnessTexture() const
	{
		return !RoughnessTexture.empty();
	}

	//! Check if material has AO texture
	bool HasAOTexture() const
	{
		return !AOTexture.empty();
	}

	//! Check if material has GPU texture of specified type
	bool HasGPUTexture(ETextureType Type) const
	{
		switch (Type)
		{
			case ETextureType::Albedo:
				return AlbedoGPUTexture.GetTextureHandle() != nullptr;
			case ETextureType::Normal:
				return NormalGPUTexture.GetTextureHandle() != nullptr;
			case ETextureType::Metallic:
				return MetallicGPUTexture.GetTextureHandle() != nullptr;
			case ETextureType::Roughness:
				return RoughnessGPUTexture.GetTextureHandle() != nullptr;
			case ETextureType::AmbientOcclusion:
				return AOGPUTexture.GetTextureHandle() != nullptr;
			default:
				return false;
			case ETextureType::Count:
				return false;
		}
	}

	//! Get GPU texture for specified type
	FTexture& GetGPUTexture(ETextureType Type)
	{
		switch (Type)
		{
			case ETextureType::Albedo:
				return AlbedoGPUTexture;
			case ETextureType::Normal:
				return NormalGPUTexture;
			case ETextureType::Metallic:
				return MetallicGPUTexture;
			case ETextureType::Roughness:
				return RoughnessGPUTexture;
			case ETextureType::AmbientOcclusion:
				return AOGPUTexture;
			case ETextureType::Count:
				break;
		}
		static FTexture NullTexture;
		return NullTexture;
	}

	//! Load GPU texture from path using KTX loader
	//! Returns true on success
	bool LoadTexture(ETextureType Type, nvrhi::IDevice* Device, nvrhi::ICommandList* CommandList, class FTextureCache* TextureCache = nullptr);

public:
	//! Albedo/Diffuse color (RGB)
	FVec3 AlbedoColor = FVec3(1.0f);

	//! Metallic factor (0.0 = dielectric, 1.0 = metal)
	float Metallic = 0.0f;

	//! Roughness factor (0.0 = smooth, 1.0 = rough)
	float Roughness = 1.0f;

	//! Emissive color (RGB); can be HDR for area lights
	FVec3 EmissiveColor = FVec3(0.0f);

	//! Texture paths
	FPath AlbedoTexture;
	FPath NormalTexture;
	FPath MetallicTexture;
	FPath RoughnessTexture;
	FPath AOTexture;

	//! Texture names (for debugging/logging)
	FString AlbedoTextureName;
	FString NormalTextureName;
	FString MetallicTextureName;
	FString RoughnessTextureName;
	FString AOTextureName;

	//! GPU textures (loaded from texture paths)
	FTexture AlbedoGPUTexture;
	FTexture NormalGPUTexture;
	FTexture MetallicGPUTexture;
	FTexture RoughnessGPUTexture;
	FTexture AOGPUTexture;
};
