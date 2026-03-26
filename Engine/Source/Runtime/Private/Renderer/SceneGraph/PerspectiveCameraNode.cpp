/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#include "Renderer/SceneGraph/PerspectiveCameraNode.h"
#include "Core/Log.h"
#include "Math/MathGLM.h"

FPerspectiveCameraNode::FPerspectiveCameraNode()
	: FNode(TXT("PerspectiveCamera"))
{
	UpdateCameraMatrices();
}

FPerspectiveCameraNode::FPerspectiveCameraNode(const FString& InName)
	: FNode(InName)
{
	UpdateCameraMatrices();
}

FPerspectiveCameraNode::FPerspectiveCameraNode(const FString& InName, const FVec3& InPosition)
	: FNode(InName, InPosition, FVec3(0.0f), FVec3(1.0f))
{
	UpdateCameraMatrices();
}

FPerspectiveCameraNode::FPerspectiveCameraNode(const FString& InName, const FVec3& InPosition, const FVec3& InRotation)
	: FNode(InName, InPosition, InRotation, FVec3(1.0f))
{
	UpdateCameraMatrices();
}

FPerspectiveCameraNode::FPerspectiveCameraNode(
	const FString& InName,
	const FVec3&   InPosition,
	const FVec3&   InRotation,
	const FVec3&   InScale,
	float		   InFov,
	float		   InAspect,
	float		   InNear,
	float		   InFar)
	: FNode(InName, InPosition, InRotation, InScale)
	, FovY(InFov)
	, AspectRatio(InAspect)
	, NearPlane(InNear)
	, FarPlane(InFar)
{
	UpdateCameraMatrices();
}

void FPerspectiveCameraNode::UpdateWorldTransform()
{
	// Call parent update
	FNode::UpdateWorldTransform();

	// Always update camera matrices after world transform update
	UpdateCameraMatrices();
}

void FPerspectiveCameraNode::SetFovY(float NewFov)
{
	FovY = NewFov;
	UpdateCameraMatrices();
}

void FPerspectiveCameraNode::SetAspectRatio(float NewAspect)
{
	AspectRatio = NewAspect;
	UpdateCameraMatrices();
}

void FPerspectiveCameraNode::SetAspectRatio(std::uint32_t Width, std::uint32_t Height)
{
	HLVM_ASSERT(Width > 0 && Height > 0);
	SetAspectRatio(static_cast<float>(Width) / static_cast<float>(Height));
}

void FPerspectiveCameraNode::SetNearPlane(float NewNear)
{
	NearPlane = NewNear;
	UpdateCameraMatrices();
}

void FPerspectiveCameraNode::SetFarPlane(float NewFar)
{
	FarPlane = NewFar;
	UpdateCameraMatrices();
}

FVec3 FPerspectiveCameraNode::ComputeForward() const
{
	// Get rotation in radians (FNode stores Euler angles in radians)
	FVec3 rotationRad = GetRotation();
	// Convert Euler angles to quaternion
	FQuat RotQuat = glm::quat(rotationRad);
	// Forward is -Z axis in OpenGL/Vulkan convention
	return glm::normalize(RotQuat * FVec3(0.0f, 0.0f, -1.0f));
}

FVec3 FPerspectiveCameraNode::ComputeRight() const
{
	FVec3 Forward = ComputeForward();
	FVec3 Up = ComputeUp();
	return glm::normalize(glm::cross(Forward, Up));
}

FVec3 FPerspectiveCameraNode::ComputeUp() const
{
	// Get rotation in radians (FNode stores Euler angles in radians)
	FVec3 rotationRad = GetRotation();
	// Convert Euler angles to quaternion
	FQuat RotQuat = glm::quat(rotationRad);
	// Up is +Y axis
	return glm::normalize(RotQuat * FVec3(0.0f, 1.0f, 0.0f));
}

// 在 PerspectiveCameraNode.cpp 中实现
void FPerspectiveCameraNode::MoveToAndLookAt(const FVec3& EyePosition,
	const FVec3&										  TargetPosition,
	const FVec3&										  WorldUp)
{
	// 计算相机坐标系的基向量 (right hand)
	FVec3 Forward = glm::normalize(TargetPosition - EyePosition);
	FVec3 Right = glm::normalize(glm::cross(Forward, WorldUp));
	FVec3 Up = glm::cross(Right, Forward);

	// 构造旋转矩阵（注意相机坐标系的 forward 是 -Z）
	FMat3 RotationMatrix;
	RotationMatrix[0] = Right;
	RotationMatrix[1] = Up;
	RotationMatrix[2] = -Forward;

	// 转换为四元数
	FQuat Rotation = glm::quat_cast(RotationMatrix);

	// 设置变换
	SetPosition(EyePosition);
	SetRotation(glm::eulerAngles(Rotation));
}

void FPerspectiveCameraNode::UpdateCameraMatrices()
{
	// Get decomposed transform for camera position
	auto Decomposed = GetDecomposedTransform();

	// Compute camera vectors
	FVec3 Forward = ComputeForward();
	FVec3 Up = ComputeUp();

	// Compute target position (look at point)
	FVec3 Target = Decomposed.Translation + Forward;

	// Update matrices
	ViewMatrix = glm::lookAt(Decomposed.Translation, Target, Up);
	ProjectionMatrix = glm::perspective(FovY, AspectRatio, NearPlane, FarPlane);
	ViewProjectionMatrix = ProjectionMatrix * ViewMatrix;
	CameraMatrix = glm::inverse(GetWorldTransform());

	HLVM_LOG(LogSceneGraph, debug, TXT("Camera '{}' matrices updated"), GetName());
}
