/**
* Copyright (c) 2025. MIT License. All rights reserved.
*/

#include "Test.h"
#include "Core/Log.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

static void testVectorAddition() {
	glm::vec3 v1(1.0f, 2.0f, 3.0f);
	glm::vec3 v2(4.0f, 5.0f, 6.0f);
	glm::vec3 result = v1 + v2;

	std::cout << "Vector Addition: (" << result.x << ", " << result.y << ", " << result.z << ")\n";
	// Expected output: (5.0, 7.0, 9.0)
}

static void testDotProduct() {
	glm::vec3 v1(1.0f, 2.0f, 3.0f);
	glm::vec3 v2(4.0f, 5.0f, 6.0f);
	float dot = glm::dot(v1, v2);

	std::cout << "Dot Product: " << dot << "\n";
	// Expected output: 32.0
}

static void testMatrixMultiplication() {
	glm::mat4 m1(1.0f); // Identity matrix
	glm::mat4 m2(2.0f); // Matrix with all elements set to 2.0

	glm::mat4 result = m1 * m2;

	std::cout << "Matrix Multiplication:\n";
	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			std::cout << result[i][j] << " ";
		}
		std::cout << "\n";
	}
	// Expected output: All elements should be 2.0
}

static void testMatrixTranspose() {
	glm::mat3 m(1.0f, 2.0f, 3.0f,
		4.0f, 5.0f, 6.0f,
		7.0f, 8.0f, 9.0f);

	glm::mat3 transposed = glm::transpose(m);

	std::cout << "Matrix Transpose:\n";
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
			std::cout << transposed[i][j] << " ";
		}
		std::cout << "\n";
	}
	// Expected output:
	// 1.0 4.0 7.0
	// 2.0 5.0 8.0
	// 3.0 6.0 9.0
}

static void testTranslationMatrix() {
	glm::vec3 translation(1.0f, 2.0f, 3.0f);
	glm::mat4 trans = glm::translate(glm::mat4(1.0f), translation);

	std::cout << "Translation Matrix:\n";
	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			std::cout << trans[i][j] << " ";
		}
		std::cout << "\n";
	}
	// Expected output:
	// 1.0 0.0 0.0 1.0
	// 0.0 1.0 0.0 2.0
	// 0.0 0.0 1.0 3.0
	// 0.0 0.0 0.0 1.0
}

static void testRotationMatrix() {
	float angle = glm::radians(90.0f); // Rotate 90 degrees around the Z-axis
	glm::mat4 rot = glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0.0f, 0.0f, 1.0f));

	std::cout << "Rotation Matrix:\n";
	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			std::cout << rot[i][j] << " ";
		}
		std::cout << "\n";
	}
	// Expected output:
	// 0.0 -1.0 0.0 0.0
	// 1.0  0.0 0.0 0.0
	// 0.0  0.0 1.0 0.0
	// 0.0  0.0 0.0 1.0
}

static void testNormalizeVector() {
	glm::vec3 v(3.0f, 4.0f, 0.0f);
	glm::vec3 normalized = glm::normalize(v);

	std::cout << "Normalized Vector: (" << normalized.x << ", " << normalized.y << ", " << normalized.z << ")\n";
	// Expected output: (0.6, 0.8, 0.0)
}

static void testCrossProduct() {
	glm::vec3 v1(1.0f, 0.0f, 0.0f);
	glm::vec3 v2(0.0f, 1.0f, 0.0f);
	glm::vec3 cross = glm::cross(v1, v2);

	std::cout << "Cross Product: (" << cross.x << ", " << cross.y << ", " << cross.z << ")\n";
	// Expected output: (0.0, 0.0, 1.0)
}

/*
	<test method>
*/
RECORD_BOOL(glm_test)
{
	testVectorAddition();
	testDotProduct();
	testMatrixMultiplication();
	testMatrixTranspose();
	testTranslationMatrix();
	testRotationMatrix();
	testNormalizeVector();
	testCrossProduct();

	return true;
}
