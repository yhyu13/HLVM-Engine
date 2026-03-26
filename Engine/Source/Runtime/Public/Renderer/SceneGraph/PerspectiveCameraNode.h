/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#pragma once

#include "Renderer/SceneGraph/FNode.h"
#include "Math/MathGLM.h"

/**
 * @brief Perspective camera for 3D scene rendering
 *
 * A camera that uses perspective projection with field of view,
 * aspect ratio, and near/far clipping planes.
 * Inherits from FNode for transform hierarchy support.
 */
class FPerspectiveCameraNode : public FNode
{
public:
	/**
	 * @brief Default constructor
	 */
	FPerspectiveCameraNode();

	/**
	 * @brief Construct with camera name
	 * @param Name Camera name
	 */
	explicit FPerspectiveCameraNode(const FString& Name);

	/**
	 * @brief Construct with position
	 * @param Name Camera name
	 * @param Position Initial position
	 */
	FPerspectiveCameraNode(const FString& Name, const FVec3& Position);

	/**
	 * @brief Construct with position and rotation
	 * @param Name Camera name
	 * @param Position Initial position
	 * @param Rotation Initial rotation (Euler angles in degrees)
	 */
	FPerspectiveCameraNode(const FString& Name, const FVec3& Position, const FVec3& Rotation);

	/**
	 * @brief Full constructor with all parameters
	 * @param Name Camera name
	 * @param Position Initial position
	 * @param Rotation Initial rotation (Euler angles in degrees)
	 * @param Scale Initial scale
	 * @param FovY Field of view in radians
	 * @param Aspect Aspect ratio (width/height)
	 * @param Near Near clipping plane
	 * @param Far Far clipping plane
	 */
	FPerspectiveCameraNode(
		const FString& Name,
		const FVec3&   Position,
		const FVec3&   Rotation,
		const FVec3&   Scale,
		float		   FovY,
		float		   Aspect,
		float		   Near,
		float		   Far);

	/**
	 * @brief Update world transform and camera matrices
	 *
	 * Override to update camera matrices after transform updates.
	 */
	virtual void UpdateWorldTransform() override;

	// Camera properties
	/**
	 * @brief Get field of view in radians
	 * @return Field of view
	 */
	float GetFov() const { return FovY; }

	/**
	 * @brief Get aspect ratio (width/height)
	 * @return Aspect ratio
	 */
	float GetAspectRatio() const { return AspectRatio; }

	/**
	 * @brief Get near clipping plane
	 * @return Near plane distance
	 */
	float GetNearPlane() const { return NearPlane; }

	/**
	 * @brief Get far clipping plane
	 * @return Far plane distance
	 */
	float GetFarPlane() const { return FarPlane; }

	/**
	 * @brief Set field of view
	 * @param NewFov Field of view in radians
	 */
	void SetFovY(float NewFov);

	/**
	 * @brief Set aspect ratio
	 * @param NewAspect Aspect ratio (width/height)
	 */
	void SetAspectRatio(float NewAspect);
	void SetAspectRatio(TUINT32 Width, TUINT32 Height);

	/**
	 * @brief Set near clipping plane
	 * @param NewNear Near plane distance
	 */
	void SetNearPlane(float NewNear);

	/**
	 * @brief Set far clipping plane
	 * @param NewFar Far plane distance
	 */
	void SetFarPlane(float NewFar);

	// Camera matrices
	/**
	 * @brief Get projection matrix
	 * @return Projection matrix (camera-space → clip-space)
	 */
	const FMat4& GetProjectionMatrix() const { return ProjectionMatrix; }

	/**
	 * @brief Get view matrix
	 * @return View matrix (world-space → camera-space)
	 */
	const FMat4& GetViewMatrix() const { return ViewMatrix; }

	/**
	 * @brief Get view-projection matrix
	 * @return Combined view-projection matrix (world-space → clip-space)
	 */
	const FMat4& GetViewProjectionMatrix() const { return ViewProjectionMatrix; }

	/**
	 * @brief Get camera matrix (inverse world transform)
	 * @return Camera matrix (world origin in camera space)
	 */
	const FMat4& GetCameraMatrix() const { return CameraMatrix; }

	// Camera vectors
	/**
	 * @brief Compute forward vector
	 * @return Normalized forward vector
	 */
	FVec3 ComputeForward() const;

	/**
	 * @brief Compute right vector
	 * @return Normalized right vector
	 */
	FVec3 ComputeRight() const;

	/**
	 * @brief Compute up vector
	 * @return Normalized up vector
	 */
	FVec3 ComputeUp() const;

	/**
	 * @brief Set camera position and orientation to look at a target point
	 * @param EyePosition Camera position in world space
	 * @param TargetPosition Point to look at
	 * @param WorldUp Up vector (usually (0,1,0))
	 */
	void MoveToAndLookAt(const FVec3& EyePosition,
		const FVec3&				  TargetPosition,
		const FVec3&				  WorldUp = FVec3(0.0f, 1.0f, 0.0f));

protected:
	/**
	 * @brief Update camera matrices based on current transform
	 */
	void UpdateCameraMatrices();

protected:
	// Projection parameters
	float FovY{ glm::radians(45.0f) }; // Field of view in radians
	float AspectRatio{ 1.778f };	   // Aspect ratio (width/height)
	float NearPlane{ 0.1f };		   // Near clipping plane
	float FarPlane{ 100.0f };		   // Far clipping plane

	// Computed matrices
	FMat4 ProjectionMatrix{ 1.0f };		// Camera-space → clip-space
	FMat4 ViewMatrix{ 1.0f };			// World-space → camera-space
	FMat4 ViewProjectionMatrix{ 1.0f }; // Combined: world-space → clip-space
	FMat4 CameraMatrix{ 1.0f };			// Inverse world transform
};
