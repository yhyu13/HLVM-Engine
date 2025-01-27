/**
* Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Wrap up GLM types using 'using' keyword for convenience
using FVec2 = glm::vec2;
using FVec3 = glm::vec3;
using FVec4 = glm::vec4;
using FMat3 = glm::mat3;
using FMat4 = glm::mat4;

// Define a mathematical structure using GLM types
struct Transform {
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
