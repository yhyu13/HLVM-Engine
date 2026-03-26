/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#pragma once

#include "Math/MathGLM.h"
#include "Core/Log.h"
#include <assimp/Importer.hpp>
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

DECLARE_LOG_CATEGORY(LogScene3D)

/**
 * @brief Assimp helper utilities for matrix conversion and debugging
 */
class FAssimpHelper
{
public:
	/**
	 * @brief Convert Assimp matrix to GLM matrix
	 * @param From Assimp 4x4 matrix
	 * @return GLM 4x4 matrix
	 */
	inline static FMat4 Assimp2Glm(const aiMatrix4x4& From)
	{
		return FMat4(
			static_cast<float>(From.a1), static_cast<float>(From.b1), static_cast<float>(From.c1), static_cast<float>(From.d1),
			static_cast<float>(From.a2), static_cast<float>(From.b2), static_cast<float>(From.c2), static_cast<float>(From.d2),
			static_cast<float>(From.a3), static_cast<float>(From.b3), static_cast<float>(From.c3), static_cast<float>(From.d3),
			static_cast<float>(From.a4), static_cast<float>(From.b4), static_cast<float>(From.c4), static_cast<float>(From.d4)
		);
	}

	/**
	 * @brief Convert GLM matrix to Assimp matrix
	 * @param From GLM 4x4 matrix
	 * @return Assimp 4x4 matrix
	 */
	inline static aiMatrix4x4 Glm2Assimp(const FMat4& From)
	{
		return aiMatrix4x4(
			From[0][0], From[1][0], From[2][0], From[3][0],
			From[0][1], From[1][1], From[2][1], From[3][1],
			From[0][2], From[1][2], From[2][2], From[3][2],
			From[0][3], From[1][3], From[2][3], From[3][3]
		);
	}

	/**
	 * @brief Print mesh information for debugging
	 * @param Mesh Assimp mesh pointer
	 */
	inline static void ShowMesh(const aiMesh* Mesh)
	{
		constexpr uint32_t Max = 3;
		
		HLVM_LOG(LogScene3D, info, TXT("Mesh {}: {} vertices, {} faces, {} bones"), 
			FString(Mesh->mName.C_Str()), Mesh->mNumVertices, Mesh->mNumFaces, Mesh->mNumBones);

		// Print bone information
		for (uint32_t i = 0; i < Mesh->mNumBones && i < Max; ++i)
		{
			const aiBone* Bone = Mesh->mBones[i];
			HLVM_LOG(LogScene3D, info, TXT("  {}: {} weights; OffsetMatrix: [{}, ...]"), 
				FString(Bone->mName.C_Str()), Bone->mNumWeights, Bone->mOffsetMatrix[0][0]);
			
			for (uint32_t j = 0; j < Bone->mNumWeights && j < Max; ++j)
			{
				HLVM_LOG(LogScene3D, info, TXT("    {} {}"), 
					Bone->mWeights[i].mVertexId, Bone->mWeights[i].mWeight);
			}
			
			if (Bone->mNumWeights > Max)
			{
				HLVM_LOG(LogScene3D, info, TXT("    ..."));
			}
		}
		
		if (Mesh->mNumBones > Max)
		{
			HLVM_LOG(LogScene3D, info, TXT("  ..."));
		}
	}

	/**
	 * @brief Print animation information for debugging
	 * @param Anim Assimp animation pointer
	 */
	inline static void ShowAnimation(const aiAnimation* Anim)
	{
		constexpr uint32_t Max = 3;
		
		HLVM_LOG(LogScene3D, info, TXT("Animation: {} duration: {} ticks/sec: {} channels: {}"), 
			FString(Anim->mName.C_Str()), Anim->mDuration, Anim->mTicksPerSecond, Anim->mNumChannels);

		// Print animation channels
		for (uint32_t i = 0; i < Anim->mNumChannels && i < Max; ++i)
		{
			const aiNodeAnim* Channel = Anim->mChannels[i];
			
			HLVM_LOG(LogScene3D, info, TXT("    {} VQS keys: {} {} {}"), 
				FString(Channel->mNodeName.C_Str()), 
				Channel->mNumPositionKeys, 
				Channel->mNumRotationKeys, 
				Channel->mNumScalingKeys);

			// Position keys
			for (uint32_t j = 0; j < Channel->mNumPositionKeys && j < Max; ++j)
			{
				const aiVectorKey& Key = Channel->mPositionKeys[i];
				HLVM_LOG(LogScene3D, info, TXT("      V[{}]: {} : ({}, {}, {})"), 
					i, Key.mTime, Key.mValue[0], Key.mValue[1], Key.mValue[2]);
			}
			
			if (Channel->mNumPositionKeys > Max)
			{
				HLVM_LOG(LogScene3D, info, TXT("      ..."));
			}

			// Rotation keys
			for (uint32_t j = 0; j < Channel->mNumRotationKeys && j < Max; ++j)
			{
				const aiQuatKey& Key = Channel->mRotationKeys[i];
				HLVM_LOG(LogScene3D, info, TXT("      Q[{}]: {} : ({}, {}, {}, {})"), 
					i, Key.mTime, Key.mValue.w, Key.mValue.x, Key.mValue.y, Key.mValue.z);
			}
			
			if (Channel->mNumRotationKeys > Max)
			{
				HLVM_LOG(LogScene3D, info, TXT("      ..."));
			}

			// Scaling keys
			for (uint32_t j = 0; j < Channel->mNumScalingKeys && j < Max; ++j)
			{
				const aiVectorKey& Key = Channel->mScalingKeys[i];
				HLVM_LOG(LogScene3D, info, TXT("      S[{}]: {} : ({}, {}, {})"), 
					i, Key.mTime, Key.mValue[0], Key.mValue[1], Key.mValue[2]);
			}
			
			if (Channel->mNumScalingKeys > Max)
			{
				HLVM_LOG(LogScene3D, info, TXT("      ..."));
			}
		}

		if (Anim->mNumChannels > Max)
		{
			HLVM_LOG(LogScene3D, info, TXT("    ..."));
		}
	}

	/**
	 * @brief Print bone hierarchy for debugging
	 * @param Scene Assimp scene pointer
	 * @param Node Assimp node pointer
	 * @param Level Indentation level
	 */
	inline static void ShowBoneHierarchy(const aiScene* Scene, const aiNode* Node, int32_t Level = 0)
	{
		// Print indentation for hierarchy level
		FString Indent;
		for (int32_t i = 0; i < Level; ++i)
		{
			Indent += TXT("-");
		}
		Indent += TXT(">");

		// Print node name and transformation
		HLVM_LOG(LogScene3D, info, TXT("{}{} Transformation: [{} ...]"), 
			Indent, FString(Node->mName.C_Str()), Node->mTransformation[0][0]);

		// Recurse to children
		for (uint32_t i = 0; i < Node->mNumChildren; ++i)
		{
			ShowBoneHierarchy(Scene, Node->mChildren[i], Level + 1);
		}
	}
};
