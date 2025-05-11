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

using FIntVec2 = glm::ivec2;
using FUIntVec2 = glm::uvec2;
using FIntVec3 = glm::ivec3;
using FUIntVec3 = glm::uvec3;

// Define a mathematical structure using GLM types
struct FTransform {
	FVec3 position;
	FVec3 scale;
	FVec3 rotation; // Euler angles in radians

	// Method to get the transformation matrix
	FMat4 getTransformationMatrix() const {
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

class FMath {
public:
	template <typename T>
	static T Max(T a, T b) {
		return a > b ? a : b;
	}

	template <typename T>
	static T Min(T a, T b) {
		return a < b ? a : b;
	}

	// Clamp
	template <typename T>
	static T Clamp(T value, T min, T max) {
		return FMath::Max(FMath::Min(value, max), min);
	}

	// Lerp
	template <typename T>
	static T Lerp(T a, T b, float t) {
		return a + (b - a) * t;
	}
};
