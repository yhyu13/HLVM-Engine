/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#include "Renderer/SceneGraph/FNode.h"

FNode::FNode()
{
	HLVM_LOG(LogSceneGraph, trace, TXT("FNode default constructor"));
	UpdateLocalTransform();
}

FNode::FNode(const FString& InName)
	: Name(InName)
{
	HLVM_LOG(LogSceneGraph, trace, TXT("FNode constructor with name '{}'"), Name);
	UpdateLocalTransform();
}

FNode::FNode(const FString& InName, const FVec3& InPosition, const FVec3& InRotation, const FVec3& InScale)
	: Name(InName)
	, Position(InPosition)
	, Rotation(InRotation)
	, Scale(InScale)
{
	HLVM_LOG(LogSceneGraph, trace, TXT("FNode constructor with name '{}', position={}, rotation={}, scale={}"),
		InName, ::ToString(InPosition), ::ToString(InRotation), ::ToString(InScale));
	UpdateLocalTransform();
}

FNode::~FNode()
{
	HLVM_LOG(LogSceneGraph, trace, TXT("FNode destructor for '{}'"), Name);
	// Parent is a raw pointer, no need to reset - just clear reference
	// Children are destroyed via unique_ptr automatically
}

void FNode::UpdateWorldTransform()
{
	// If already being updated by an ancestor, skip to avoid infinite recursion
	if (IsUpdating)
	{
		return;
	}

	// Mark as in-progress
	IsUpdating = true;

	// Update local transform
	UpdateLocalTransform();

	// Get parent world transform
	if (Parent)
	{
		// Ensure parent is up to date first
		Parent->UpdateWorldTransform();
		WorldTransform = Parent->GetWorldTransform() * LocalTransform;
	}
	else
	{
		// Root node: world = local
		WorldTransform = LocalTransform;
	}

	// Mark as clean
	DirtyState = EDirtyState::Clean;

	HLVM_LOG(LogSceneGraph, trace, TXT("Updated world transform for '{}'"), Name);

	// Propagate to children - iterate after we're clean so children can use our fresh world transform
	for (auto& child : Children)
	{
		child->UpdateWorldTransform();
	}

	// Done
	IsUpdating = false;
}

void FNode::UpdateLocalTransform()
{
	// Create rotation matrix from Euler angles (pitch, yaw, roll)
	// GLM uses ZYX convention by default: roll (Z), pitch (X), yaw (Y)
	FMat4 rotationMatrix = glm::eulerAngleZYX(Rotation.z, Rotation.y, Rotation.x);

	// Create scale matrix
	FMat4 scaleMatrix = glm::scale(FMat4(1.0f), Scale);

	// Create translation matrix
	FMat4 translationMatrix = glm::translate(FMat4(1.0f), Position);

	// Combine: T * R * S (standard order for scene graphs)
	LocalTransform = translationMatrix * rotationMatrix * scaleMatrix;

	HLVM_LOG(LogSceneGraph, trace, TXT("Updated local transform for '{}': position={}, rotation={}, scale={}"),
		Name, ::ToString(Position), ::ToString(Rotation), ::ToString(Scale));
}

bool FNode::MarkDirty()
{
	if (DirtyState == EDirtyState::Dirty)
	{
		// Already dirty, no need to propagate
		return false;
	}

	DirtyState = EDirtyState::Dirty;
	HLVM_LOG(LogSceneGraph, trace, TXT("Mark node '{}' and children as dirty"), Name);

	// Recursively mark all children as dirty
	for (auto& child : Children)
	{
		if (!child->MarkDirty())
		{
			// Log
			HLVM_LOG(LogSceneGraph, trace, TXT("Child node '{}' already marked dirty"), child->GetName());
		}
#if SCENE_GRAPH_CLANG_17_COMPILE_FIX
		// YuHang : Re-ensure child dirty due to Clang 17 compile release mode ignore dirty set on child
		// for no apprant reason, must be compiler bug
		child->DirtyState = EDirtyState::Dirty;
#endif
	}

	return true;
}

bool FNode::RemoveFromParent()
{
	if (!Parent)
	{
		return false; // No parent to remove from
	}

	// Get parent pointer
	TNullablePtr<FNode> parentPtr = Parent;

	// Find this node in parent's children
	auto& parentChildren = parentPtr->Children;
	auto  it = std::find_if(parentChildren.begin(), parentChildren.end(),
		 [this](const std::unique_ptr<FNode>& childPtr) {
			 return childPtr.get() == this;
		 });

	if (it != parentChildren.end())
	{
		// Clear parent reference BEFORE erasing
		Parent = nullptr;

		// Remove from parent's children (this destroys *this via unique_ptr)
		parentChildren.erase(it);

		// Mark as dirty since world transform changed
		MarkDirty();

		HLVM_LOG(LogSceneGraph, debug, TXT("Removed node '{}' from parent '{}'"),
			Name, parentPtr->GetName());
		return true;
	}

	return false;
}

void FNode::SetPosition(const FVec3& NewPosition)
{
	if (Position == NewPosition)
	{
		return; // No change
	}

	Position = NewPosition;

	HLVM_LOG(LogSceneGraph, debug, TXT("Set position of '{}' to {}"), Name, ::ToString(NewPosition));
	MarkDirty();
}

void FNode::SetRotation(const FVec3& NewRotation)
{
	if (Rotation == NewRotation)
	{
		return; // No change
	}

	Rotation = NewRotation;
	MarkDirty();

	HLVM_LOG(LogSceneGraph, debug, TXT("Set rotation of '{}' to {}"), Name, ::ToString(NewRotation));
}

void FNode::SetScale(const FVec3& NewScale)
{
	if (Scale == NewScale)
	{
		return; // No change
	}

	Scale = NewScale;
	MarkDirty();

	HLVM_LOG(LogSceneGraph, debug, TXT("Set scale of '{}' to {}"), Name, ::ToString(NewScale));
}

void FNode::SetWorldPosition(const FVec3& NewWorldPosition)
{
	if (Parent)
	{
		FMat4 parentInverse = glm::inverse(Parent->GetWorldTransform());
		FVec4 localPos = parentInverse * FVec4(NewWorldPosition, 1.0f);
		SetPosition(FVec3(localPos.x, localPos.y, localPos.z));
	}
	else
	{
		SetPosition(NewWorldPosition);
	}
	HLVM_LOG(LogSceneGraph, debug, TXT("Set world position of '{}' to {}"), Name, ::ToString(NewWorldPosition));
}

void FNode::SetWorldRotation(const FVec3& NewWorldRotation)
{
	if (Parent)
	{
		Parent->UpdateWorldTransform();
		FQuat worldRotQuat = glm::quat(NewWorldRotation);
		FQuat parentWorldRot = glm::quat(Parent->GetWorldTransform());
		FQuat parentInverseRot = glm::inverse(parentWorldRot);
		FQuat localRotQuat = parentInverseRot * worldRotQuat;
		FVec3 localRot = glm::eulerAngles(localRotQuat);
		SetRotation(localRot);
	}
	else
	{
		SetRotation(NewWorldRotation);
	}
	HLVM_LOG(LogSceneGraph, debug, TXT("Set world rotation of '{}' to {}"), Name, ::ToString(NewWorldRotation));
}

void FNode::SetWorldScale(const FVec3& NewWorldScale)
{
	if (Parent)
	{
		Parent->UpdateWorldTransform();
		auto parentDecomp = Parent->GetDecomposedTransform();
		FVec3 parentScale = parentDecomp.Scale;
		FVec3 localScale;
		localScale.x = parentScale.x != 0.0f ? NewWorldScale.x / parentScale.x : NewWorldScale.x;
		localScale.y = parentScale.y != 0.0f ? NewWorldScale.y / parentScale.y : NewWorldScale.y;
		localScale.z = parentScale.z != 0.0f ? NewWorldScale.z / parentScale.z : NewWorldScale.z;
		SetScale(localScale);
	}
	else
	{
		SetScale(NewWorldScale);
	}
	HLVM_LOG(LogSceneGraph, debug, TXT("Set world scale of '{}' to {}"), Name, ::ToString(NewWorldScale));
}

void FNode::SetWorldTransform(const FMat4& NewWorldTransform)
{
	FVec3 scale;
	FQuat rotation;
	FVec3 translation;
	FVec3 skew;
	FVec4 perspective;

	if (glm::decompose(NewWorldTransform, scale, rotation, translation, skew, perspective))
	{
		FVec3 worldRotEuler = glm::eulerAngles(rotation);

		if (Parent)
		{
			Parent->UpdateWorldTransform();
			FMat4 parentInverse = glm::inverse(Parent->GetWorldTransform());

			FVec4 localPos = parentInverse * FVec4(translation, 1.0f);

			FQuat parentWorldRot = glm::quat(Parent->GetWorldTransform());
			FQuat parentInverseRot = glm::inverse(parentWorldRot);
			FQuat localRotQuat = parentInverseRot * rotation;
			FVec3 localRotEuler = glm::eulerAngles(localRotQuat);

			auto parentDecomp = Parent->GetDecomposedTransform();
			FVec3 parentScale = parentDecomp.Scale;
			FVec3 localScale;
			localScale.x = parentScale.x != 0.0f ? scale.x / parentScale.x : scale.x;
			localScale.y = parentScale.y != 0.0f ? scale.y / parentScale.y : scale.y;
			localScale.z = parentScale.z != 0.0f ? scale.z / parentScale.z : scale.z;

			Position = FVec3(localPos.x, localPos.y, localPos.z);
			Rotation = localRotEuler;
			Scale = localScale;
		}
		else
		{
			Position = translation;
			Rotation = worldRotEuler;
			Scale = scale;
		}

		MarkDirty();
	}
	else
	{
		HLVM_LOG(LogSceneGraph, warn, TXT("Failed to decompose world transform for node '{}'"), Name);
	}
	HLVM_LOG(LogSceneGraph, debug, TXT("Set world transform of '{}'"), Name);
}

void FNode::SetLocalTransform(const FMat4& NewLocalTransform)
{
	FVec3 scale;
	FQuat rotation;
	FVec3 translation;
	FVec3 skew;
	FVec4 perspective;

	if (glm::decompose(NewLocalTransform, scale, rotation, translation, skew, perspective))
	{
		Position = translation;
		Rotation = glm::eulerAngles(rotation);
		Scale = scale;
		MarkDirty();
	}
	else
	{
		HLVM_LOG(LogSceneGraph, warn, TXT("Failed to decompose local transform for node '{}'"), Name);
	}
	HLVM_LOG(LogSceneGraph, debug, TXT("Set local transform of '{}'"), Name);
}


FNode::FDecomposedTransform FNode::GetDecomposedTransform() const
{
	FDecomposedTransform result;

	// Use GLM to decompose the world transform matrix
	FVec3 scale;
	FQuat rotation;
	FVec3 translation;
	FVec3 skew;
	FVec4 perspective;

	if (glm::decompose(WorldTransform, scale, rotation, translation, skew, perspective))
	{
		result.Translation = translation;

		// Convert quaternion to Euler angles (pitch, yaw, roll)
		// GLM returns angles in radians
		FVec3 euler = glm::eulerAngles(rotation);
		result.Rotation = euler;

		result.Scale = scale;
		result.Skew = skew;
		result.Perspective = perspective;
	}
	else
	{
		HLVM_LOG(LogSceneGraph, warn, TXT("Failed to decompose transform for node '{}'"), Name);
	}

	return result;
}

FString FNode::ToString() const
{
	FString parentName = Parent ? Parent->GetName() : TXT("None");

	return FString::Format(
		TXT("FNode('{}', Parent='{}', Children={}, Position={}, Rotation={}, Scale={}, Dirty={})"),
		Name,
		parentName,
		Children.size(),
		::ToString(Position),
		::ToString(Rotation),
		::ToString(Scale),
		DirtyState == EDirtyState::Dirty ? TXT("Dirty") : TXT("Clean"));
}
