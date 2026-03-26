/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#include "CommonMinimal.h"
#include "Renderer/Scene3D/Scene3DNode.h"

TVector<std::shared_ptr<IMesh>> FScene3DNode::GetAllMesh() const
{
	TVector<std::shared_ptr<IMesh>> Ret;
	Ret.reserve(MeshTree.Num());

	// Extract mesh from each mesh in hierarchy
	for (const auto& [Level, Mesh] : MeshTree)
	{
		if (Mesh)
		{
			Ret.emplace_back(Mesh);
		}
	}

	return Ret;
}

TVector<std::shared_ptr<FStaticMesh>> FScene3DNode::GetAllStaticMesh() const
{
	TVector<std::shared_ptr<FStaticMesh>> Ret;
	Ret.reserve(MeshTree.Num());

	// Extract mesh from each mesh in hierarchy
	for (const auto& [Level, Mesh] : MeshTree)
	{
		if (Mesh)
		{
			if (auto StaticMesh = std::static_pointer_cast<FStaticMesh>(Mesh))
			{
				Ret.emplace_back(StaticMesh);
			}
		}
	}

	return Ret;
}

TVector<std::shared_ptr<FPBRMaterial>> FScene3DNode::GetAllMaterial() const
{
	return MaterialOneMeshMap.Keys();
}
