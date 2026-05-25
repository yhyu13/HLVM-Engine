// Copyright 2026 HLVM Engine
//
// MIT License

#pragma once

#include "Core/String.h"
#include "Core/Log.h"
#include <nvrhi/nvrhi.h>
#include <functional>

/**
 * @brief Minimal render graph for pass dependency validation and ordered execution
 *
 * FRenderGraph provides:
 * - Declarative pass registration with named input/output textures
 * - Dependency validation (all inputs must be produced or imported)
 * - Cycle detection
 * - Topological sort for execution ordering
 *
 * NOT provided (future work):
 * - Automatic barrier inference
 * - Transient texture allocation
 * - Resource aliasing
 * - Multi-queue scheduling
 *
 * Usage:
 *   FRenderGraph Graph;
 *   Graph.ImportTexture("GBufferDiffuse", DiffuseTex);
 *   Graph.AddPass({"Lighting", {"GBufferDiffuse"}, {"HDR"}, [&](CmdList){ ... }});
 *   Graph.AddPass({"ToneMap", {"HDR"}, {"SDR"}, [&](CmdList){ ... }});
 *   if (Graph.Compile()) { Graph.Execute(CmdList); }
 */
class FRenderGraph
{
public:
    struct FPassDesc
    {
        FString Name;
        TVector<FString> Inputs;
        TVector<FString> Outputs;
        std::function<void(nvrhi::ICommandList*)> Execute;
    };

    FRenderGraph() = default;

    /**
     * @brief Declare an external texture that is produced outside the graph
     *
     * Imported textures satisfy input requirements without being produced by a pass.
     */
    void ImportTexture(const FString& Name, nvrhi::TextureHandle Texture);

    /**
     * @brief Register a pass with the graph
     */
    void AddPass(const FPassDesc& Desc);

    /**
     * @brief Validate dependencies, detect cycles, and compute execution order
     * @return true if compilation succeeded, false if validation failed
     */
    bool Compile();

    /**
     * @brief Execute all passes in compiled order
     */
    void Execute(nvrhi::ICommandList* CmdList) const;

    /**
     * @brief Clear all passes, imports, and compiled state
     */
    void Clear();

    bool IsCompiled() const { return bCompiled; }
    const TVector<FString>& GetExecutionOrder() const { return ExecutionOrder; }
    uint32_t GetPassCount() const { return static_cast<uint32_t>(Passes.size()); }

private:
    struct FPassNode
    {
        FString Name;
        TVector<FString> Inputs;
        TVector<FString> Outputs;
        std::function<void(nvrhi::ICommandList*)> Execute;
    };

    TVector<FPassNode> Passes;
    TMap<FString, nvrhi::TextureHandle> ImportedTextures;
    TVector<FString> ExecutionOrder;
    bool bCompiled = false;
};
