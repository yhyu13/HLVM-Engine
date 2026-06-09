// Copyright 2026 HLVM Engine
//
// MIT License

#pragma once

#include "Core/String.h"
#include "Renderer/Common/FBindingLayoutBuilder.h"
#include <nvrhi/nvrhi.h>

/**
 * @brief Encapsulates a ray tracing pipeline: shaders, layout, pipeline object, shader table
 *
 * FRayTracingPipeline manages the boilerplate of creating a ray tracing pipeline:
 * - Loading ray generation, miss, and closest-hit shaders from a ShaderLibrary
 * - Creating the binding layout (optionally with FBindingLayoutBuilder)
 * - Creating the ray tracing pipeline state
 * - Managing the shader table with raygen/miss/hitgroup entries
 *
 * Usage (from NVRHI ShaderLibrary):
 *   FRayTracingPipeline RTPipeline;
 *   nvrhi::ShaderLibraryHandle ShaderLib = Device->createShaderLibrary(blob.data(), blob.size());
 *   RTPipeline.InitializeFromLibrary(Device, ShaderLib, "RayGen", "ClosestHit", "Miss", "ShadowMiss");
 *   RTPipeline.CreateBindingLayout()
 *       .AddConstantBuffer(0)          // b0
 *       .AddConstantBuffer(1)          // b1
 *       .AddRayTracingAccelStruct(0);  // t0
 *   RTPipeline.FinalizePipeline();
 *   RTPipeline.BuildShaderTable();
 *   RTPipeline.DispatchRays(CmdList, Width, Height);
 *
 * Usage (from individual shaders):
 *   FRayTracingPipeline RTPipeline;
 *   RTPipeline.InitializeFromShaders(Device, RayGen, ClosestHit, Miss, ShadowMiss);
 *   // ... same as above
 */
class FRayTracingPipeline
{
public:
    struct FDispatchDesc
    {
        uint32_t Width = 1;
        uint32_t Height = 1;
        uint32_t Depth = 1;
    };

    FRayTracingPipeline() = default;
    ~FRayTracingPipeline() { Shutdown(); }

    FRayTracingPipeline(const FRayTracingPipeline&) = delete;
    FRayTracingPipeline& operator=(const FRayTracingPipeline&) = delete;
    FRayTracingPipeline(FRayTracingPipeline&&) = delete;
    FRayTracingPipeline& operator=(FRayTracingPipeline&&) = delete;

    /**
     * @brief Initialize from an NVRHI ShaderLibrary handle
     *
     * Loads shaders by entry point name from a compiled .sblob library.
     *
     * @param InDevice NVRHI device
     * @param ShaderLibrary NVRHI shader library containing RT shaders
     * @param RayGenName Ray generation entry point
     * @param ClosestHitName Closest hit entry point
     * @param MissName Primary miss entry point
     * @param ShadowMissName Optional shadow miss entry point (empty if unused)
     * @return true if all shaders loaded successfully
     */
    bool InitializeFromLibrary(
        nvrhi::IDevice* InDevice,
        nvrhi::ShaderLibraryHandle ShaderLibrary,
        const FString& RayGenName,
        const FString& ClosestHitName,
        const FString& MissName,
        const FString& ShadowMissName = TXT(""));

    /**
     * @brief Initialize from pre-loaded individual shader handles
     *
     * Use when shaders are loaded individually via FShaderLibrary::LoadShader().
     *
     * @param InDevice NVRHI device
     * @param InRayGen Pre-loaded ray generation shader
     * @param InClosestHit Pre-loaded closest hit shader
     * @param InMiss Pre-loaded miss shader
     * @param InShadowMiss Optional pre-loaded shadow miss shader
     * @return true if all shader handles are valid
     */
    bool InitializeFromShaders(
        nvrhi::IDevice* InDevice,
        nvrhi::ShaderHandle InRayGen,
        nvrhi::ShaderHandle InClosestHit,
        nvrhi::ShaderHandle InMiss,
        nvrhi::ShaderHandle InShadowMiss = nullptr);

    /**
     * @brief Access the binding layout builder to declare resources
     *
     * Call this between Initialize*() and FinalizePipeline().
     * The builder is reset on each call to FinalizePipeline().
     */
    FBindingLayoutBuilder& CreateBindingLayout(nvrhi::ShaderType Visibility = nvrhi::ShaderType::All);

    /**
     * @brief Set an externally created binding layout (alternative to CreateBindingLayout)
     */
    void SetBindingLayout(nvrhi::BindingLayoutHandle ExternalLayout);

    /**
     * @brief Set a bindless layout for the pipeline
     *
     * The bindless layout is added to globalBindingLayouts alongside the regular
     * binding layout. Must be called before FinalizePipeline().
     */
    void SetBindlessLayout(nvrhi::BindingLayoutHandle InBindlessLayout);

    /**
     * @brief Create the ray tracing pipeline from loaded shaders + binding layout
     * @param MaxPayloadSize Maximum ray payload size in bytes
     * @param MaxAttributeSize Maximum hit attribute size in bytes (default: 8 for barycentrics)
     * @return true if pipeline created successfully
     */
    bool FinalizePipeline(uint32_t MaxPayloadSize = 64, uint32_t MaxAttributeSize = 8);

    /**
     * @brief Build the shader table with current shader configuration
     *
     * Must be called after FinalizePipeline().
     * Automatically registers:
     * - Ray generation shader
     * - All miss shaders (including shadow miss if configured)
     * - Default hit group
     */
    bool BuildShaderTable();

    /**
     * @brief Add an additional hit group (must be called before FinalizePipeline)
     */
    void AddHitGroup(const FString& Name, const FString& ClosestHitEntry, const FString& AnyHitEntry = TXT(""));

    /**
     * @brief Dispatch rays using the built shader table
     */
    void DispatchRays(nvrhi::ICommandList* CmdList, const FDispatchDesc& Desc);

    /**
     * @brief Dispatch rays using the built shader table (convenience overload)
     */
    void DispatchRays(nvrhi::ICommandList* CmdList, uint32_t Width, uint32_t Height, uint32_t Depth = 1);

    /**
     * @brief Dispatch rays with a binding set attached
     *
     * Convenience overload that sets both the shader table and binding set
     * before dispatching. The binding set must be compatible with the pipeline's
     * binding layout.
     */
    void DispatchRays(nvrhi::ICommandList* CmdList, const FDispatchDesc& Desc, nvrhi::BindingSetHandle BindingSet);

    /**
     * @brief Dispatch rays with a binding set attached (convenience overload)
     */
    void DispatchRays(nvrhi::ICommandList* CmdList, uint32_t Width, uint32_t Height, uint32_t Depth, nvrhi::BindingSetHandle BindingSet);

    /**
     * @brief Dispatch rays with binding set + optional bindless descriptor table
     */
    void DispatchRays(nvrhi::ICommandList* CmdList, const FDispatchDesc& Desc,
        nvrhi::BindingSetHandle BindingSet, nvrhi::IDescriptorTable* DescriptorTable);

    /**
     * @brief Dispatch rays with binding set + optional bindless descriptor table (convenience)
     */
    void DispatchRays(nvrhi::ICommandList* CmdList, uint32_t Width, uint32_t Height, uint32_t Depth,
        nvrhi::BindingSetHandle BindingSet, nvrhi::IDescriptorTable* DescriptorTable);

    void Shutdown();

    // Accessors
    [[nodiscard]] nvrhi::rt::PipelineHandle    GetPipeline() const { return Pipeline; }
    [[nodiscard]] nvrhi::rt::ShaderTableHandle GetShaderTable() const { return ShaderTable; }
    [[nodiscard]] nvrhi::BindingLayoutHandle   GetBindingLayout() const { return BindingLayout; }
    [[nodiscard]] bool                         IsInitialized() const { return bIsInitialized; }
    [[nodiscard]] bool                         IsPipelineFinalized() const { return bPipelineFinalized; }
    [[nodiscard]] bool                         IsShaderTableBuilt() const { return bShaderTableBuilt; }

private:
    nvrhi::IDevice* Device = nullptr;

    // Shader names (for shader table registration)
    FString RayGenName;
    FString ClosestHitName;
    FString MissName;
    FString ShadowMissName;

    // Shaders
    nvrhi::ShaderHandle RayGenShader;
    nvrhi::ShaderHandle ClosestHitShader;
    nvrhi::ShaderHandle MissShader;
    nvrhi::ShaderHandle ShadowMissShader;

    // Pipeline objects
    nvrhi::rt::PipelineHandle    Pipeline;
    nvrhi::rt::ShaderTableHandle ShaderTable;
    nvrhi::BindingLayoutHandle   BindingLayout;
    nvrhi::BindingLayoutHandle   BindlessLayout;

    // Builder state
    TUniquePtr<FBindingLayoutBuilder> LayoutBuilder;
    bool                              bUsingExternalLayout = false;

    // Hit groups
    struct FHitGroupEntry
    {
        FString Name;
        FString ClosestHitEntry;
        FString AnyHitEntry;
    };
    TVector<FHitGroupEntry> HitGroups;

    // State flags
    bool bIsInitialized = false;
    bool bPipelineFinalized = false;
    bool bShaderTableBuilt = false;
    bool bHasBindlessLayout = false;

    bool ValidateShaders();
};
