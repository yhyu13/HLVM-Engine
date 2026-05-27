/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#include "CommonMinimal.h"
#include "Renderer/Scene3D/Scene3DLoader.h"
#include "Renderer/Material/PBRMaterial.h"
#include "Renderer/Mesh/StaticMesh.h"

/**
 * @brief Recursively traverse scene graph and load mesh data
 *
 * @param SceneData Scene data object being populated
 * @param SceneName Name of the scene
 * @param SceneDir Directory containing the scene file
 * @param AIScene Assimp scene pointer
 * @param Node Assimp node being processed
 * @param ParentTr Parent transformation matrix
 * @param LoadingContext Asset loading context
 */
void FScene3DLoader::RecurseLoad(
	FScene3DNode&		 SceneData,
	const FString&		 SceneName,
	const FPath&		 SceneDir,
	const aiScene*		 AIScene,
	const aiNode*		 Node,
	const aiMatrix4x4&	 ParentTr,
	const AssetLoadingContext& LoadingContext)
{
	auto Level = LoadingContext.Level;

	// Accumulate transformations down the hierarchy
	const aiMatrix4x4 ChildTr = ParentTr * Node->mTransformation;

	// Inverse transpose for normal transformation
	aiMatrix3x3 NormalTr = aiMatrix3x3(ChildTr);
	NormalTr.Inverse().Transpose();

	// Process all meshes in this node
	for (uint32_t i = 0; i < Node->mNumMeshes; ++i)
	{
		// Extract mesh
		const aiMesh* AIMesh = AIScene->mMeshes[Node->mMeshes[i]];

		// Extract surface material
		const aiMaterial* AIMaterial = AIScene->mMaterials[AIMesh->mMaterialIndex];

		// Create PBR material
		auto PBRMaterial = std::make_shared<FPBRMaterial>();
		if (auto Material = LoadingContext.MaterialMaterialMap.Find(AIMaterial);
			Material && SPC1<FPBRMaterial>(*Material))
		{
			PBRMaterial = SPC1<FPBRMaterial>(*Material);
		}
		else
		{
			LoadingContext.MaterialMaterialMap.Add(AIMaterial, PBRMaterial);
			aiString TexPath;
			// Albedo texture or color (glTF uses aiTextureType_BASE_COLOR, not aiTextureType_DIFFUSE)
			if (AI_SUCCESS == AIMaterial->GetTexture(aiTextureType_BASE_COLOR, 0, &TexPath))
			{
				const FString Filename = FString(TexPath.C_Str());
				PBRMaterial->SetTexture(Filename, SceneDir / Filename, IMaterial::ETextureType::Albedo);
			}
			else if (aiColor4D BaseColorFactor; AI_SUCCESS == AIMaterial->Get(AI_MATKEY_BASE_COLOR, BaseColorFactor))
			{
				PBRMaterial->AlbedoColor = FVec3(BaseColorFactor.r, BaseColorFactor.g, BaseColorFactor.b);
			}

			// Metallic texture or factor
			if (AI_SUCCESS == AIMaterial->GetTexture(aiTextureType_METALNESS, 0, &TexPath))
			{
				const FString Filename = FString(TexPath.C_Str());
				PBRMaterial->SetTexture(Filename, SceneDir / Filename, IMaterial::ETextureType::Metallic);
			}
			else if (ai_real MetallicFactor; AI_SUCCESS == AIMaterial->Get(AI_MATKEY_METALLIC_FACTOR, MetallicFactor))
			{
				PBRMaterial->Metallic = MetallicFactor;
			}

			// Roughness texture or factor
			if (AI_SUCCESS == AIMaterial->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, 0, &TexPath))
			{
				const FString Filename = FString(TexPath.C_Str());
				PBRMaterial->SetTexture(Filename, SceneDir / Filename, IMaterial::ETextureType::Roughness);
			}
			else if (ai_real RoughnessFactor; AI_SUCCESS == AIMaterial->Get(AI_MATKEY_ROUGHNESS_FACTOR, RoughnessFactor))
			{
				PBRMaterial->Roughness = RoughnessFactor;
			}

			// Normal texture
			if (AI_SUCCESS == AIMaterial->GetTexture(aiTextureType_NORMALS, 0, &TexPath))
			{
				const FString Filename = FString(TexPath.C_Str());
				PBRMaterial->SetTexture(Filename, SceneDir / Filename, IMaterial::ETextureType::Normal);
			}

			// Ambient Occlusion texture
			if (AI_SUCCESS == AIMaterial->GetTexture(aiTextureType_LIGHTMAP, 0, &TexPath))
			{
				const FString Filename = FString(TexPath.C_Str());
				PBRMaterial->SetTexture(Filename, SceneDir / Filename, IMaterial::ETextureType::AmbientOcclusion);
			}
		}

		auto MeshData = std::make_shared<FStaticMesh>();
		if (auto Mesh = LoadingContext.MeshMeshMap.Find(AIMesh);
			Mesh && SPC1<FStaticMesh>(*Mesh))
		{
			MeshData = SPC1<FStaticMesh>(*Mesh);
		}
		else
		// Create mesh data
		{
			LoadingContext.MeshMeshMap.Add(AIMesh, MeshData);
			// Reserve space for efficiency
			MeshData->ReserveVertices(AIMesh->mNumVertices);
			MeshData->ReserveIndices(AIMesh->mNumFaces * 3);

			// Store world transform for LOCAL-space geometry support
			// Note: aiMatrix4x4 is row-major, glm::mat4 is column-major, need transpose
			MeshData->WorldTransform = glm::mat4(
				ChildTr[0][0], ChildTr[0][1], ChildTr[0][2], ChildTr[0][3],
				ChildTr[1][0], ChildTr[1][1], ChildTr[1][2], ChildTr[1][3],
				ChildTr[2][0], ChildTr[2][1], ChildTr[2][2], ChildTr[2][3],
				ChildTr[3][0], ChildTr[3][1], ChildTr[3][2], ChildTr[3][3]
			);

			// Process vertices (LOCAL space - no transform applied)
			for (uint32_t t = 0; t < AIMesh->mNumVertices; ++t)
			{
				// Position in LOCAL space (world transform applied by shader)
				const aiVector3D AIPosition = AIMesh->mVertices[t];

				// Normal in LOCAL space
				const aiVector3D AINormal = AIMesh->HasNormals()
					? AIMesh->mNormals[t]
					: aiVector3D(0, 1, 0);

				// Get texture coordinates
				const aiVector3D AITexCoord = AIMesh->HasTextureCoords(0)
					? AIMesh->mTextureCoords[0][t]
					: aiVector3D(0, 0, 0);

				// Tangent in LOCAL space
				const aiVector3D AITangent = AIMesh->HasTangentsAndBitangents()
					? AIMesh->mTangents[t]
					: aiVector3D(1, 0, 0);

				// Add vertex (simple FP32 format)
				MeshData->AddVertex(FVertex(
					FVec3(AIPosition.x, AIPosition.y, AIPosition.z),
					FVec3(AINormal.x, AINormal.y, AINormal.z),
					FVec2(AITexCoord.x, AITexCoord.y),
					FVec3(AITangent.x, AITangent.y, AITangent.z)));
			}

			// Process faces (indices)
			for (uint32_t j = 0; j < AIMesh->mNumFaces; ++j)
			{
				const aiFace* AIFace = &AIMesh->mFaces[j];

				// Ensure triangle topology
				HLVM_ENSURE_F(AIFace->mNumIndices == 3,
					TXT("Triangle mesh {} has {} indices instead of 3!"),
					FString(Node->mName.data), AIFace->mNumIndices);

				MeshData->AddTriangle(AIFace->mIndices[0], AIFace->mIndices[1], AIFace->mIndices[2]);
			}
		}

		// Create mesh object
		{
			const FString MeshName = FString::Format(TXT("{}_{}_{}"),
				SceneName, Level, FString(AIMesh->mName.C_Str()));

			auto Mesh = MeshData;
			Mesh->SetName(MeshName);
			PBRMaterial->SetName(MeshName);
			// Add to scene hierarchy
			SceneData.MeshTree.emplace_back(Level, Mesh);
			SceneData.MeshMultiMaterialMap[Mesh].Add(PBRMaterial);
			SceneData.MaterialOneMeshMap[PBRMaterial] = (Mesh);
		}
	}

	// Recurse to children
	for (uint32_t i = 0; i < Node->mNumChildren; ++i)
	{
		// Reset Level on each child, since each child can recurse, thus change level index
		LoadingContext.Level = Level + 1;
		RecurseLoad(SceneData, SceneName, SceneDir, AIScene, Node->mChildren[i], ChildTr, LoadingContext);
	}
}

std::shared_ptr<FScene3DNode> FScene3DLoader::LoadFromFile(const FPath& ScenePath)
{
	// Load Assimp scene
	auto AssimpScene = FAssimpSceneObject::LoadFromFile(ScenePath);

	const FString Name = AssimpScene->GetScene()->mName.C_Str();
	FScene3DNode  SceneData(Name);

	const FPath	   SceneDir = FPath::GetParentPath(ScenePath);
	const aiScene* AIScene = AssimpScene->GetScene();
	const aiNode*  RootNode = AssimpScene->GetRoot();

	RecurseLoad(SceneData, Name, SceneDir, AIScene, RootNode);

	return std::make_shared<FScene3DNode>(std::move(SceneData));
}

std::shared_ptr<FScene3DNode> FScene3DLoader::LoadFromAssimp(
	const FPath&   ScenePath,
	const FString& SceneName)
{
	const FString ActualName = SceneName.empty() ? FPath::GetCleanFileName(ScenePath) : SceneName;

	// Load Assimp scene
	auto AssimpScene = FAssimpSceneObject::LoadFromFile(ScenePath);

	FScene3DNode SceneData(ActualName);

	const FPath	   SceneDir = FPath::GetParentPath(ScenePath);
	const aiScene* AIScene = AssimpScene->GetScene();
	const aiNode*  RootNode = AssimpScene->GetRoot();

	RecurseLoad(SceneData, ActualName, SceneDir, AIScene, RootNode);

	return std::make_shared<FScene3DNode>(std::move(SceneData));
}
