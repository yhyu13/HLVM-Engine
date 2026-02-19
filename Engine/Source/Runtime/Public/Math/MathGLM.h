/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Wrap up GLM types using 'using' keyword for convenience
using FVec2 = glm::vec2;
using FVec3 = glm::vec3;
using FVec4 = glm::vec4;
using FMat3 = glm::mat3;
using FMat4 = glm::mat4;

using FInt2 = glm::ivec2;
using FUInt2 = glm::uvec2;
using FInt3 = glm::ivec3;
using FUInt3 = glm::uvec3;

// Define a mathematical structure using GLM types
struct FTransform
{
	FVec3 position;
	FVec3 scale;
	FVec3 rotation; // Euler angles in radians

	// Method to get the transformation matrix
	FMat4 getTransformationMatrix() const
	{
		// Start with an identity matrix
		FMat4 transform = FMat4(1.0f);

		// Apply translation
		transform = glm::translate(transform, position);

		// Apply rotation (using Euler angles)
		transform = glm::rotate(transform, rotation.x, FVec3(1.0f, 0.0f, 0.0f)); // Rotate around X axis
		transform = glm::rotate(transform, rotation.y, FVec3(0.0f, 1.0f, 0.0f)); // Rotate around Y axis
		transform = glm::rotate(transform, rotation.z, FVec3(0.0f, 0.0f, 1.0f)); // Rotate around Z axis

		// Apply scale
		transform = glm::scale(transform, scale);

		return transform;
	}
};

class FMath
{
public:
	template <typename T>
	static T Max(T a, T b)
	{
		return a > b ? a : b;
	}

	template <typename T, typename U, typename V>
	static T Max(U a, V b)
	{
		return S_C(T, a) > S_C(T, b) ? S_C(T, a) : S_C(T, b);
	}

	template <typename T>
	static T Min(T a, T b)
	{
		return a < b ? a : b;
	}

	template <typename T, typename U, typename V>
	static T Min(U a, V b)
	{
		return S_C(T, a) < S_C(T, b) ? S_C(T, a) : S_C(T, b);
	}

	// Clamp
	template <typename T>
	static T Clamp(T value, T min, T max)
	{
		return FMath::Max(FMath::Min(value, max), min);
	}

	template <typename T, typename U, typename V>
	static T Clamp(T value, U min, V max)
	{
		return FMath::Max(FMath::Min(value, max), min);
	}

	// Lerp
	template <typename T>
	static T Lerp(T a, T b, float t)
	{
		return a + (b - a) * t;
	}

	template <typename T, typename U, typename V>
	static T Lerp(U a, V b, float t)
	{
		return S_C(T, a) + (S_C(T, b) - S_C(T, a)) * t;
	}

	// abs
	template <typename T>
	static T Abs(T value)
	{
		return value < 0 ? -value : value;
	}
};

namespace
{
	// Math type print to FString
	HLVM_INLINE_FUNC FString ToString(const FVec2& vec)
	{
		return FString::Format(TXT("FVec2(%f, %f)"), vec.x, vec.y);
	}

	HLVM_INLINE_FUNC FString ToString(const FVec3& vec)
	{
		return FString::Format(TXT("FVec3(%f, %f, %f)"), vec.x, vec.y, vec.z);
	}

	HLVM_INLINE_FUNC FString ToString(const FVec4& vec)
	{
		return FString::Format(TXT("FVec4(%f, %f, %f, %f)"), vec.x, vec.y, vec.z, vec.w);
	}

	HLVM_INLINE_FUNC FString ToString(const FMat3& mat)
	{
		return FString::Format(TXT("FMat3(%f, %f, %f,\n%f, %f, %f,\n%f, %f, %f)"), mat[0][0], mat[0][1], mat[0][2], mat[1][0], mat[1][1], mat[1][2], mat[2][0], mat[2][1], mat[2][2]);
	}

	HLVM_INLINE_FUNC FString ToString(const FMat4& mat)
	{
		return FString::Format(TXT("FMat4(%f, %f, %f, %f,\n%f, %f, %f, %f,\n%f, %f, %f, %f,\n%f, %f, %f, %f)"), mat[0][0], mat[0][1], mat[0][2], mat[0][3], mat[1][0], mat[1][1], mat[1][2], mat[1][3], mat[2][0], mat[2][1], mat[2][2], mat[2][3], mat[3][0], mat[3][1], mat[3][2], mat[3][3]);
	}

	HLVM_INLINE_FUNC FString ToString(const FInt2& vec)
	{
		return FString::Format(TXT("FInt2(%d, %d)"), vec.x, vec.y);
	}

	HLVM_INLINE_FUNC FString ToString(const FInt3& vec)
	{
		return FString::Format(TXT("FInt3(%d, %d, %d)"), vec.x, vec.y, vec.z);
	}

	HLVM_INLINE_FUNC FString ToString(const FUInt2& vec)
	{
		return FString::Format(TXT("FUInt2(%u, %u)"), vec.x, vec.y);
	}

	HLVM_INLINE_FUNC FString ToString(const FUInt3& vec)
	{
		return FString::Format(TXT("FUInt3(%u, %u, %u)"), vec.x, vec.y, vec.z);
	}

	HLVM_INLINE_FUNC FString ToString(const FTransform& transform)
	{
		return FString::Format(TXT("FTransform(%s, %s, %s)"), ToString(transform.position).c_str(), ToString(transform.scale).c_str(), ToString(transform.rotation).c_str());
	}
}
