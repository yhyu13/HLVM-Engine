/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#pragma once

#include "Math/MathGLM.h"
#include "Core/Container/ContainerDefinition.h"
#include "Definition/TypeDefinition.h"
#include "Platform/FileSystem/Path.h"

/**
 * @brief Base interface for all material types
 *
 * Common material properties and texture operations.
 * Texture type enum moved from FPBRMaterial to interface.
 */
class IMaterial
{
public:
	virtual ~IMaterial() = default;

	//! Texture type enumeration
	enum class ETextureType : uint8_t
	{
		Albedo = 0,
		Normal,
		Metallic,
		Roughness,
		AmbientOcclusion,
		Count
	};

	//! Get material name
	const FString& GetName() const { return Name; }
	void		   SetName(const FString& InName) { this->Name = InName; }

	//! Check if material has texture of specified type
	virtual bool HasTexture(ETextureType Type) const = 0;

	//! Get texture path for specified type
	virtual FPath GetTexturePath(ETextureType Type) const = 0;

	//! Get texture name for specified type
	virtual FString GetTextureName(ETextureType Type) const = 0;

	//! Common PBR properties
	virtual FVec3 GetAlbedoColor() const = 0;

	//! Get metallic factor
	virtual float GetMetallic() const = 0;

	//! Get roughness factor
	virtual float GetRoughness() const = 0;

protected:
	FString Name;
};
