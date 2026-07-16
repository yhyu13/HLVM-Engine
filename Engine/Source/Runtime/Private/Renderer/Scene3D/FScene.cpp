// Copyright 2026 HLVM Engine
//
// MIT License

#include "Renderer/Scene3D/FScene.h"

#include "Core/Log.h"
#include "Renderer/Common/FLightLoader.h"
#include "Renderer/Scene3D/Scene3DLoader.h"

DECLARE_LOG_CATEGORY(LogScene)

std::shared_ptr<FScene> FScene::LoadFromFile(const FPath& ScenePath)
{
    auto Scene = std::make_shared<FScene>();
    Scene->SourcePath = ScenePath;
    Scene->SceneName  = FString(ScenePath.stem().string().c_str());

    Scene->SceneNode = FScene3DLoader::LoadFromFile(ScenePath);
    if (!Scene->SceneNode)
    {
        HLVM_LOG(LogScene, err, TXT("FScene::LoadFromFile: failed to load geometry from {}"), *Scene->SourcePath);
        return nullptr;
    }

    FPath LightsPath = ScenePath.parent_path() / FPath(TXT("Lights.json"));
    std::string Error;
    if (!Renderer::LoadLightsFromJSONFile(LightsPath.string(), Scene->Lights, Error))
    {
        HLVM_LOG(LogScene, warn,
            TXT("FScene::LoadFromFile: could not load lights from {} ({})"),
            *FString(LightsPath.string().c_str()),
            *FString(Error.c_str()));
    }
    else
    {
        HLVM_LOG(LogScene, info,
            TXT("FScene::LoadFromFile: loaded {} lights from {}"),
            Scene->Lights.size(),
            *FString(LightsPath.string().c_str()));
    }

    return Scene;
}
