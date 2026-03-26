/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#include "CommonMinimal.h"
#include "Renderer/Scene3D/Assimp/AssimpSceneObject.h"

FAssimpSceneObject::FAssimpSceneObject(const FPath& Path)
{
	// Reference: http://assimp.sourceforge.net/lib_html/threading.html
	// The C-API is thread safe.
	
	const FString PathStr = FString(Path.c_str());
	HLVM_LOG(LogScene3D, debug, TXT("Reading {}"), PathStr);
	
	// Import scene with real-time quality preset and graph optimization
	m_Scene = aiImportFile(
		PathStr.ToCharCStr(), 
		aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_OptimizeGraph
	);
	
	// Check for errors
	HLVM_ENSURE_F(m_Scene != nullptr, 
		TXT("Failed to load Assimp file: {} with Error: {}"), 
		FString(Path.c_str()),
		FString(aiGetErrorString()));
}

FAssimpSceneObject::~FAssimpSceneObject()
{
	if (m_Scene != nullptr)
	{
		aiReleaseImport(m_Scene);
		m_Scene = nullptr;
	}
}
