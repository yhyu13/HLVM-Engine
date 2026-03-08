/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_CXX2A
#define GLM_FORCE_CTOR_INIT
#define GLM_FORCE_AVX2
#define GLM_FORCE_INLINE
#define GLM_FORCE_SWIZZLE
#include <glm/glm.hpp>
#include <glm/gtc/packing.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define PI_F 3.141592653589f
#define PI2_F 6.28318530718f

#define DEFAULT_IP glm::packed_lowp
#define DEFAULT_FP glm::aligned_lowp
#define DEFAULT_DP glm::aligned_lowp
// Wrap up GLM types using 'using' keyword for convenience
using FVec2 = glm::vec<2, glm::f32, DEFAULT_FP>;
using FVec3 = glm::vec<3, glm::f32, DEFAULT_FP>;
using FVec4 = glm::vec<4, glm::f32, DEFAULT_FP>;
using FVec2D = glm::vec<2, glm::f64, DEFAULT_DP>;
using FVec3D = glm::vec<3, glm::f64, DEFAULT_DP>;
using FVec4D = glm::vec<4, glm::f64, DEFAULT_DP>;

using FQuat = glm::qua<glm::f32, DEFAULT_FP>;
using FQuatD = glm::qua<glm::f64, DEFAULT_DP>;

using FMat3 = glm::mat<3, 3, glm::f32, DEFAULT_FP>;
using FMat4 = glm::mat<4, 4, glm::f32, DEFAULT_FP>;
using FMat3D = glm::mat<3, 3, glm::f64, DEFAULT_DP>;
using FMat4D = glm::mat<4, 4, glm::f64, DEFAULT_DP>;

using FInt2 = glm::vec<2, glm::i32, DEFAULT_IP>;
using FUInt2 = glm::vec<2, glm::u32, DEFAULT_IP>;
using FInt3 = glm::vec<3, glm::i32, DEFAULT_IP>;
using FUInt3 = glm::vec<3, glm::u32, DEFAULT_IP>;
using FInt2L = glm::vec<2, glm::i64, DEFAULT_IP>;
using FUInt2L = glm::vec<2, glm::u64, DEFAULT_IP>;
using FInt3L = glm::vec<3, glm::i64, DEFAULT_IP>;
using FUInt3L = glm::vec<3, glm::u64, DEFAULT_IP>;
#undef DEFAULT_IP
#undef DEFAULT_FP
#undef DEFAULT_DP

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
		return FString::Format(TXT("FVec2({:f}, {:f})"), vec.x, vec.y);
	}

	HLVM_INLINE_FUNC FString ToString(const FVec3& vec)
	{
		return FString::Format(TXT("FVec3({:f}, {:f}, {:f})"), vec.x, vec.y, vec.z);
	}

	HLVM_INLINE_FUNC FString ToString(const FVec4& vec)
	{
		return FString::Format(TXT("FVec4({:f}, {:f}, {:f}, {:f})"), vec.x, vec.y, vec.z, vec.w);
	}

	HLVM_INLINE_FUNC FString ToString(const FMat3& mat)
	{
		return FString::Format(TXT("FMat3({:f}, {:f}, {:f},\n{:f}, {:f}, {:f},\n{:f}, {:f}, {:f})"), mat[0][0], mat[0][1], mat[0][2], mat[1][0], mat[1][1], mat[1][2], mat[2][0], mat[2][1], mat[2][2]);
	}

	HLVM_INLINE_FUNC FString ToString(const FMat4& mat)
	{
		return FString::Format(TXT("FMat4({:f}, {:f}, {:f}, {:f},\n{:f}, {:f}, {:f}, {:f},\n{:f}, {:f}, {:f}, {:f},\n{:f}, {:f}, {:f}, {:f})"), mat[0][0], mat[0][1], mat[0][2], mat[0][3], mat[1][0], mat[1][1], mat[1][2], mat[1][3], mat[2][0], mat[2][1], mat[2][2], mat[2][3], mat[3][0], mat[3][1], mat[3][2], mat[3][3]);
	}

	HLVM_INLINE_FUNC FString ToString(const FInt2& vec)
	{
		return FString::Format(TXT("FInt2({:d}, {:d})"), vec.x, vec.y);
	}

	HLVM_INLINE_FUNC FString ToString(const FInt3& vec)
	{
		return FString::Format(TXT("FInt3({:d}, {:d}, {:d})"), vec.x, vec.y, vec.z);
	}

	HLVM_INLINE_FUNC FString ToString(const FUInt2& vec)
	{
		return FString::Format(TXT("FUInt2({:d}, {:d})"), vec.x, vec.y);
	}

	HLVM_INLINE_FUNC FString ToString(const FUInt3& vec)
	{
		return FString::Format(TXT("FUInt3({:d}, {:d}, {:d})"), vec.x, vec.y, vec.z);
	}
}
