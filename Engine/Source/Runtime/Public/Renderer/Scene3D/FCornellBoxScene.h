// Copyright 2026 HLVM Engine
//
// MIT License

#pragma once

#include "Renderer/Common/FLightLoader.h"
#include "Renderer/Material/PBRMaterial.h"
#include "Renderer/Mesh/StaticMesh.h"
#include "Renderer/Scene3D/FScene.h"
#include "Renderer/Scene3D/Scene3DNode.h"

#include <memory>

/**
 * @brief Header-only helper that builds a procedural Cornell box FScene.
 *
 * The geometry mirrors the classic Cornell box: six axis-aligned quads forming
 * a closed room with a red left wall, a green right wall, and white floor,
 * ceiling, front and back walls.
 *
 * Lights are loaded from the provided JSON file (e.g. CornellBox_Lights.json)
 * via the existing FLightLoader.
 */
class FCornellBoxScene
{
public:
    /**
     * @brief Build a Cornell box scene.
     * @param LightsJsonPath Path to a JSON light-list file.
     * @return Shared FScene with geometry and lights, or nullptr on failure.
     */
    static std::shared_ptr<FScene> Build(const FPath& LightsJsonPath)
    {
        auto Scene = std::make_shared<FScene>();
        Scene->SceneName  = TXT("CornellBox");
        Scene->SourcePath = LightsJsonPath.parent_path();
        Scene->SceneNode  = std::make_shared<FScene3DNode>(TXT("CornellBox"));

        constexpr float E = 1.0f;

        const FVec3 White(0.85f, 0.85f, 0.85f);
        const FVec3 Red(0.8f, 0.1f, 0.1f);
        const FVec3 Green(0.1f, 0.8f, 0.1f);
        auto AddQuad = [&](const FString& Name,
                           const FVec3& A, const FVec3& B,
                           const FVec3& C, const FVec3& D,
                           const FVec3& Normal,
                           const FVec3& Albedo,
                           const FVec3& Emissive = FVec3(0.0f, 0.0f, 0.0f))
        {
            auto Mesh = std::make_shared<FStaticMesh>(Name);

            // Tangent points along AB; value is normalized for completeness.
            const FVec3 Tangent = (B.x - A.x) != 0.0f ? FVec3(1.0f, 0.0f, 0.0f)
                                  : (B.y - A.y) != 0.0f ? FVec3(0.0f, 1.0f, 0.0f)
                                                        : FVec3(0.0f, 0.0f, 1.0f);

            Mesh->AddVertex(FVertex(A, Normal, FVec2(0.0f, 0.0f), Tangent));
            Mesh->AddVertex(FVertex(B, Normal, FVec2(1.0f, 0.0f), Tangent));
            Mesh->AddVertex(FVertex(C, Normal, FVec2(1.0f, 1.0f), Tangent));
            Mesh->AddVertex(FVertex(D, Normal, FVec2(0.0f, 1.0f), Tangent));

            Mesh->AddTriangle(0, 1, 2);
            Mesh->AddTriangle(0, 2, 3);

            auto Material = std::make_shared<FPBRMaterial>(Name);
            Material->AlbedoColor  = Albedo;
            Material->Metallic     = 0.0f;
            Material->Roughness    = 1.0f;
            Material->EmissiveColor = Emissive;

            Scene->SceneNode->MeshTree.emplace_back(0u, Mesh);
            Scene->SceneNode->MeshMultiMaterialMap[Mesh].push_back(Material);
        };

        // Floor (y = -E), normal +Y
        AddQuad(TXT("Floor"),
                FVec3(-E, -E,  E), FVec3( E, -E,  E),
                FVec3( E, -E, -E), FVec3(-E, -E, -E),
                FVec3(0.0f, 1.0f, 0.0f), White);

        // Ceiling (y = E), normal -Y
        AddQuad(TXT("Ceiling"),
                FVec3(-E,  E, -E), FVec3( E,  E, -E),
                FVec3( E,  E,  E), FVec3(-E,  E,  E),
                FVec3(0.0f, -1.0f, 0.0f), White);

        // Front wall (z = E), normal -Z
        AddQuad(TXT("FrontWall"),
                FVec3(-E, -E,  E), FVec3(-E,  E,  E),
                FVec3( E,  E,  E), FVec3( E, -E,  E),
                FVec3(0.0f, 0.0f, -1.0f), White);

        // Back wall (z = -E), normal +Z
        AddQuad(TXT("BackWall"),
                FVec3(-E, -E, -E), FVec3( E, -E, -E),
                FVec3( E,  E, -E), FVec3(-E,  E, -E),
                FVec3(0.0f, 0.0f, 1.0f), White);

        // Left wall (x = -E), normal +X, red
        AddQuad(TXT("LeftWall"),
                FVec3(-E, -E, -E), FVec3(-E, -E,  E),
                FVec3(-E,  E,  E), FVec3(-E,  E, -E),
                FVec3(1.0f, 0.0f, 0.0f), Red);

        // Right wall (x = E), normal -X, green
        AddQuad(TXT("RightWall"),
                FVec3( E, -E,  E), FVec3( E, -E, -E),
                FVec3( E,  E, -E), FVec3( E,  E,  E),
                FVec3(-1.0f, 0.0f, 0.0f), Green);

        // Load lights from JSON. An empty light list is not fatal for geometry,
        // but the GI pass needs at least one light to produce a visible result.
        std::string Error;
        Renderer::LoadLightsFromJSONFile(LightsJsonPath.string(), Scene->Lights, Error);

        return Scene;
    }
};
