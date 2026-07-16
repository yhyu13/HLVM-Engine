// Copyright 2026 HLVM Engine
//
// MIT License

#pragma once

#include "Core/String.h"
#include "Platform/FileSystem/Path.h"
#include "Renderer/Common/FLight.h"
#include "Renderer/Scene3D/Scene3DNode.h"

#include <memory>
#include <vector>

/**
 * @brief Lightweight CPU-side scene bundle: geometry node + lights.
 *
 * FScene does not own GPU handles; it is produced by loaders and consumed by
 * render passes / resource managers that upload the data they need.
 */
class FScene
{
public:
    //! Human-readable scene name (usually the source file stem).
    FString SceneName;

    //! Source file path, if any.
    FPath SourcePath;

    //! Scene geometry hierarchy.
    std::shared_ptr<FScene3DNode> SceneNode;

    //! Scene lights in the std430 FLight format used by the GPU.
    std::vector<Renderer::FLight> Lights;

    FScene() = default;
    ~FScene() = default;

    /**
     * @brief Load a scene from disk.
     *
     * Loads geometry via FScene3DLoader and, if a sibling "Lights.json" file
     * exists next to the scene file, parses it into the Lights vector.
     *
     * @param ScenePath Path to the scene asset (e.g. glTF/FBX).
     * @return Shared pointer to the scene, or nullptr if geometry loading fails.
     */
    static std::shared_ptr<FScene> LoadFromFile(const FPath& ScenePath);
};
