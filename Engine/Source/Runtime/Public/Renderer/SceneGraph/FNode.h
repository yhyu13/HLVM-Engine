/**
* Copyright (c) 2026. MIT License. All rights reserved.
*/

#pragma once

#include "SceneGraphDefinition.h"
#include "Renderer/Scene3D/Scene3DNode.h"

#include <memory>
#include <type_traits>

/**
 * @brief Hierarchical scene graph node with transform hierarchy
 *
 * A scene graph node that supports parent-child relationships,
 * transform hierarchy with dirty flag optimization, and scene data
 * storage. Uses SceneNode3D (FScene3DNode) as shared ptr member
 * as specified in AI_task.md Goal 5.
 */
class FNode
{
public:
	/**
	 * @brief Dirty state for transform optimization
	 */
	enum class EDirtyState
	{
		Clean, //!< Transform is up to date
		Dirty  //!< Transform needs recomputation
	};

	/**
	 * @brief Decomposed transform components
	 */
	struct FDecomposedTransform
	{
		FVec3 Translation{ 0.0f, 0.0f, 0.0f };
		FVec3 Rotation{ 0.0f, 0.0f, 0.0f }; //!< Euler angles in radians
		FVec3 Scale{ 1.0f, 1.0f, 1.0f };
		FVec3 Skew{ 0.0f, 0.0f, 0.0f };
		FVec4 Perspective{ 0.0f, 0.0f, 0.0f, 1.0f };

		/**
		 * @brief Convert to string for debugging
		 */
		FString ToString() const
		{
			return FString::Format(
				TXT("FDecomposedTransform(Translation={}, Rotation={}, Scale={})"),
				::ToString(Translation),
				::ToString(Rotation),
				::ToString(Scale));
		}
	};

public:
	//! Default constructor
	FNode();

	/**
	 * @brief Construct with node name
	 * @param Name Node name
	 */
	explicit FNode(const FString& Name);

	/**
	 * @brief Construct with transform
	 * @param Name Node name
	 * @param Position Initial position
	 * @param Rotation Initial rotation (Euler angles in radians)
	 * @param Scale Initial scale
	 */
	FNode(const FString& Name, const FVec3& Position, const FVec3& Rotation, const FVec3& Scale);

	//! Destructor
	virtual ~FNode();

	//! Explicitly declare copy constructor/assignment
	FNode(const FNode&) = default;
	FNode& operator=(const FNode&) = default;

	/**
	 * @brief Update world transform based on parent and local transform
	 *
	 * Recursively updates dirty nodes. Uses IsUpdating flag to prevent
	 * infinite recursion when propagating to children.
	 */
	virtual void UpdateWorldTransform();

	/**
	 * @brief Update local transform from position, rotation, scale
	 */
	void UpdateLocalTransform();

	/**
	 * @brief Mark node and all children as dirty
	 *
	 * Forces recomputation of transforms on next UpdateWorldTransform()
	 */
	bool MarkDirty();

	/**
	 * @brief Add a child node
	 * @tparam ChildType Type of child node (must inherit from FNode)
	 * @tparam Args Argument types for child constructor
	 * @param args Arguments for child constructor
	 * @return Reference to the created child
	 */
	template <typename ChildType, typename... Args>
	ChildType& AddChild(Args&&... args);

	/**
	 * @brief Remove this node from its parent
	 * @return true if successfully removed, false if no parent
	 */
	bool RemoveFromParent();

	// Getters
	const FString&						   GetName() const { return Name; }
	const FVec3&						   GetPosition() const { return Position; }
	const FVec3&						   GetRotation() const { return Rotation; }
	const FVec3&						   GetScale() const { return Scale; }
	const FMat4&						   GetLocalTransform() const { return LocalTransform; }
	const FMat4&						   GetWorldTransform() const { return WorldTransform; }
	TNullablePtr<FNode>					   GetParent() const { return Parent; }
	const TVector<std::unique_ptr<FNode>>& GetChildren() const { return Children; }
	std::shared_ptr<FScene3DNode>		   GetSceneData() const { return SceneData; }
	EDirtyState							   GetDirtyState() const { return DirtyState; }

	// Setters
	void SetName(const FString& NewName) { Name = NewName; }
	void SetPosition(const FVec3& NewPosition);
	void SetRotation(const FVec3& NewRotation);
	void SetScale(const FVec3& NewScale);
	void SetSceneData(std::shared_ptr<FScene3DNode> NewSceneData) { SceneData = std::move(NewSceneData); }

	/**
	 * @brief Get decomposed transform from world matrix
	 * @return Decomposed transform components
	 */
	FDecomposedTransform GetDecomposedTransform() const;

	/**
	 * @brief Check if node has children
	 * @return true if has children, false otherwise
	 */
	bool HasChildren() const { return !Children.empty(); }

	/**
	 * @brief Get number of children
	 * @return Child count
	 */
	TSIZE NumChildren() const { return Children.size(); }

	/**
	 * @brief Convert to string for debugging
	 * @return String representation
	 */
	FString ToString() const;

protected:
	//! Node name
	FString Name{ TXT("UnnamedNode") };

	//! Transform components
	FVec3 Position{ 0.0f, 0.0f, 0.0f };
	FVec3 Rotation{ 0.0f, 0.0f, 0.0f }; //!< Euler angles in radians
	FVec3 Scale{ 1.0f, 1.0f, 1.0f };

	//! Computed transforms
	FMat4 LocalTransform{ 1.0f };
	FMat4 WorldTransform{ 1.0f };

	//! Dirty state optimization
	EDirtyState DirtyState{ EDirtyState::Dirty };

	//! In-progress flag to prevent infinite recursion during UpdateWorldTransform
	bool IsUpdating{ false };

	//! Hierarchy
	//! Parent pointer (non-owning) - safe because parent owns child via Children vector
	TNullablePtr<FNode>				Parent{ nullptr };
	TVector<std::unique_ptr<FNode>> Children;

	//! Scene data (per AI_task.md Goal 5 requirement)
	std::shared_ptr<FScene3DNode> SceneData;
};

// Template method implementations (must be in header)
template <typename ChildType, typename... Args>
ChildType& FNode::AddChild(Args&&... args)
{
	static_assert(std::is_base_of_v<FNode, ChildType>,
		"ChildType must inherit from FNode");

	auto childNode = std::make_unique<ChildType>(std::forward<Args>(args)...);

	// Establish parent reference using raw pointer
	// Parent is non-owning - child is owned by parent's Children vector (unique_ptr)
	// Raw pointer is safe because parent outlives child (parent owns child)
	childNode->Parent = this;

	// Initialize child's dirty state to match parent's state
	// This ensures proper propagation when UpdateWorldTransform is called
	// If parent is Clean, child is Clean. If parent is Dirty, child's world
	// transform will be computed when parent's UpdateWorldTransform propagates.
	if (DirtyState == EDirtyState::Clean)
	{
		childNode->DirtyState = EDirtyState::Clean;
	}

	ChildType& ref = *childNode;
	Children.push_back(std::move(childNode));

	HLVM_LOG(LogSceneGraph, debug, TXT("Added child '{}' to parent '{}'"),
		ref.GetName(), GetName());

	return ref;
}
