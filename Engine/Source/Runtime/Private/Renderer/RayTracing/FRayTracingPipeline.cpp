// Copyright 2026 HLVM Engine
//
// MIT License

#include "Renderer/RayTracing/FRayTracingPipeline.h"
#include "Core/Log.h"

DECLARE_LOG_CATEGORY(LogRenderer)

bool FRayTracingPipeline::ValidateShaders()
{
    if (!RayGenShader)
    {
        HLVM_LOG(LogRenderer, err, TXT("FRayTracingPipeline: RayGen shader is null"));
        return false;
    }
    if (!ClosestHitShader)
    {
        HLVM_LOG(LogRenderer, err, TXT("FRayTracingPipeline: ClosestHit shader is null"));
        return false;
    }
    if (!MissShader)
    {
        HLVM_LOG(LogRenderer, err, TXT("FRayTracingPipeline: Miss shader is null"));
        return false;
    }
    return true;
}

bool FRayTracingPipeline::InitializeFromLibrary(
    nvrhi::IDevice* InDevice,
    nvrhi::ShaderLibraryHandle ShaderLibrary,
    const FString& InRayGenName,
    const FString& InClosestHitName,
    const FString& InMissName,
    const FString& InShadowMissName)
{
    Device = InDevice;
    RayGenName = InRayGenName;
    ClosestHitName = InClosestHitName;
    MissName = InMissName;
    ShadowMissName = InShadowMissName;

    if (!ShaderLibrary)
    {
        HLVM_LOG(LogRenderer, err, TXT("FRayTracingPipeline: Shader library is null"));
        return false;
    }

    RayGenShader = ShaderLibrary->getShader(RayGenName, nvrhi::ShaderType::RayGeneration);
    ClosestHitShader = ShaderLibrary->getShader(ClosestHitName, nvrhi::ShaderType::ClosestHit);
    MissShader = ShaderLibrary->getShader(MissName, nvrhi::ShaderType::Miss);

    if (!ShadowMissName.IsEmpty())
    {
        ShadowMissShader = ShaderLibrary->getShader(ShadowMissName, nvrhi::ShaderType::Miss);
    }

    if (!ValidateShaders())
    {
        return false;
    }

    bIsInitialized = true;
    HLVM_LOG(LogRenderer, info, TXT("FRayTracingPipeline: Shaders loaded from library (RG={}, CH={}, M={}, SM={})"),
             *RayGenName, *ClosestHitName, *MissName,
             ShadowMissName.IsEmpty() ? TXT("none") : *ShadowMissName);
    return true;
}

bool FRayTracingPipeline::InitializeFromShaders(
    nvrhi::IDevice* InDevice,
    nvrhi::ShaderHandle InRayGen,
    nvrhi::ShaderHandle InClosestHit,
    nvrhi::ShaderHandle InMiss,
    nvrhi::ShaderHandle InShadowMiss)
{
    Device = InDevice;
    RayGenShader = InRayGen;
    ClosestHitShader = InClosestHit;
    MissShader = InMiss;
    ShadowMissShader = InShadowMiss;

    // Store names for shader table (best effort from shader reflection if available)
    RayGenName = TXT("RayGen");
    ClosestHitName = TXT("ClosestHit");
    MissName = TXT("Miss");
    if (ShadowMissShader)
    {
        ShadowMissName = TXT("ShadowMiss");
    }

    if (!ValidateShaders())
    {
        return false;
    }

    bIsInitialized = true;
    HLVM_LOG(LogRenderer, info, TXT("FRayTracingPipeline: Shaders set directly (RG, CH, M{}, )"),
             ShadowMissShader ? TXT(", SM") : TXT(""));
    return true;
}

FBindingLayoutBuilder& FRayTracingPipeline::CreateBindingLayout(nvrhi::ShaderType Visibility)
{
    LayoutBuilder = std::make_unique<FBindingLayoutBuilder>();
    LayoutBuilder->SetVisibility(Visibility);
    bUsingExternalLayout = false;
    return *LayoutBuilder;
}

void FRayTracingPipeline::SetBindingLayout(nvrhi::BindingLayoutHandle ExternalLayout)
{
    BindingLayout = ExternalLayout;
    bUsingExternalLayout = true;
    LayoutBuilder.reset();
}

void FRayTracingPipeline::SetBindlessLayout(nvrhi::BindingLayoutHandle InBindlessLayout)
{
    BindlessLayout = InBindlessLayout;
    bHasBindlessLayout = true;
}

void FRayTracingPipeline::AddBindingLayout(nvrhi::BindingLayoutHandle InLayout)
{
    if (InLayout)
    {
        AdditionalBindingLayouts.push_back(InLayout);
    }
}

bool FRayTracingPipeline::FinalizePipeline(uint32_t MaxPayloadSize, uint32_t MaxAttributeSize)
{
    if (!bIsInitialized)
    {
        HLVM_LOG(LogRenderer, err, TXT("FRayTracingPipeline: Initialize*() must be called before FinalizePipeline()"));
        return false;
    }

    if (!bUsingExternalLayout)
    {
        if (!LayoutBuilder)
        {
            HLVM_LOG(LogRenderer, err, TXT("FRayTracingPipeline: CreateBindingLayout() must be called before FinalizePipeline()"));
            return false;
        }
        BindingLayout = Device->createBindingLayout(LayoutBuilder->Build());
        if (!BindingLayout)
        {
            HLVM_LOG(LogRenderer, err, TXT("FRayTracingPipeline: Failed to create binding layout"));
            return false;
        }
    }

    nvrhi::rt::PipelineDesc PipelineDesc;
    PipelineDesc.globalBindingLayouts = { BindingLayout };
    for (const auto& Layout : AdditionalBindingLayouts)
    {
        PipelineDesc.globalBindingLayouts.push_back(Layout);
    }
    if (bHasBindlessLayout && BindlessLayout)
    {
        PipelineDesc.globalBindingLayouts.push_back(BindlessLayout);
    }
    PipelineDesc.shaders = {
        { "", RayGenShader, nullptr },
        { "", MissShader, nullptr }
    };

    if (ShadowMissShader)
    {
        PipelineDesc.shaders.push_back({ "", ShadowMissShader, nullptr });
    }

    // Add default hit group
    PipelineDesc.hitGroups.push_back({
        "HitGroup",
        ClosestHitShader,
        nullptr, nullptr, nullptr, false
    });

    // Add additional hit groups
    for (const auto& HG : HitGroups)
    {
        nvrhi::ShaderHandle AnyHitShader = nullptr;
        if (!HG.AnyHitEntry.IsEmpty())
        {
            HLVM_LOG(LogRenderer, warn, TXT("FRayTracingPipeline: Per-hit-group any-hit shaders not yet supported"));
        }

        {
            nvrhi::rt::PipelineHitGroupDesc HitGroupDesc;
            HitGroupDesc.setExportName(std::string(reinterpret_cast<const char*>(*HG.Name)));
            HitGroupDesc.setClosestHitShader(ClosestHitShader.Get());
            if (AnyHitShader)
                HitGroupDesc.setAnyHitShader(AnyHitShader.Get());
            PipelineDesc.hitGroups.push_back(HitGroupDesc);
        }
    }

    PipelineDesc.maxPayloadSize = MaxPayloadSize;
    PipelineDesc.maxAttributeSize = MaxAttributeSize;

    Pipeline = Device->createRayTracingPipeline(PipelineDesc);
    if (!Pipeline)
    {
        HLVM_LOG(LogRenderer, err, TXT("FRayTracingPipeline: Failed to create ray tracing pipeline"));
        return false;
    }

    bPipelineFinalized = true;
    HLVM_LOG(LogRenderer, info, TXT("FRayTracingPipeline: Pipeline finalized (payload={}, attr={})"),
             MaxPayloadSize, MaxAttributeSize);
    return true;
}

bool FRayTracingPipeline::BuildShaderTable()
{
    if (!bPipelineFinalized)
    {
        HLVM_LOG(LogRenderer, err, TXT("FRayTracingPipeline: FinalizePipeline() must be called before BuildShaderTable()"));
        return false;
    }

    ShaderTable = Pipeline->createShaderTable();
    if (!ShaderTable)
    {
        HLVM_LOG(LogRenderer, err, TXT("FRayTracingPipeline: Failed to create shader table"));
        return false;
    }

    ShaderTable->setRayGenerationShader(RayGenName);
    ShaderTable->addHitGroup("HitGroup");
    ShaderTable->addMissShader(MissName);

    if (ShadowMissShader)
    {
        ShaderTable->addMissShader(ShadowMissName);
    }

    bShaderTableBuilt = true;
    HLVM_LOG(LogRenderer, info, TXT("FRayTracingPipeline: Shader table built"));
    return true;
}

void FRayTracingPipeline::AddHitGroup(const FString& Name, const FString& ClosestHitEntry, const FString& AnyHitEntry)
{
    HitGroups.push_back({ Name, ClosestHitEntry, AnyHitEntry });
}

void FRayTracingPipeline::DispatchRays(nvrhi::ICommandList* CmdList, const FDispatchDesc& Desc)
{
    if (!bShaderTableBuilt)
    {
        HLVM_LOG(LogRenderer, err, TXT("FRayTracingPipeline: BuildShaderTable() must be called before DispatchRays()"));
        return;
    }

    nvrhi::rt::State State;
    State.setShaderTable(ShaderTable.Get());

    CmdList->setRayTracingState(State);

    nvrhi::rt::DispatchRaysArguments Args;
    Args.width = Desc.Width;
    Args.height = Desc.Height;
    Args.depth = Desc.Depth;

    CmdList->dispatchRays(Args);
}

void FRayTracingPipeline::DispatchRays(nvrhi::ICommandList* CmdList, uint32_t Width, uint32_t Height, uint32_t Depth)
{
    FDispatchDesc Desc{};
    Desc.Width = Width;
    Desc.Height = Height;
    Desc.Depth = Depth;
    DispatchRays(CmdList, Desc);
}

void FRayTracingPipeline::DispatchRays(nvrhi::ICommandList* CmdList, const FDispatchDesc& Desc, nvrhi::BindingSetHandle BindingSet)
{
    if (!bShaderTableBuilt)
    {
        HLVM_LOG(LogRenderer, err, TXT("FRayTracingPipeline: BuildShaderTable() must be called before DispatchRays()"));
        return;
    }

    nvrhi::rt::State State;
    State.setShaderTable(ShaderTable.Get());
    if (BindingSet)
    {
        State.addBindingSet(BindingSet.Get());
    }

    CmdList->setRayTracingState(State);

    nvrhi::rt::DispatchRaysArguments Args;
    Args.width = Desc.Width;
    Args.height = Desc.Height;
    Args.depth = Desc.Depth;

    CmdList->dispatchRays(Args);
}

void FRayTracingPipeline::DispatchRays(nvrhi::ICommandList* CmdList, uint32_t Width, uint32_t Height, uint32_t Depth, nvrhi::BindingSetHandle BindingSet)
{
    FDispatchDesc Desc{};
    Desc.Width = Width;
    Desc.Height = Height;
    Desc.Depth = Depth;
    DispatchRays(CmdList, Desc, BindingSet);
}

void FRayTracingPipeline::DispatchRays(nvrhi::ICommandList* CmdList, const FDispatchDesc& Desc,
    nvrhi::BindingSetHandle BindingSet, nvrhi::IDescriptorTable* DescriptorTable)
{
    if (!bShaderTableBuilt)
    {
        HLVM_LOG(LogRenderer, err, TXT("FRayTracingPipeline: BuildShaderTable() must be called before DispatchRays()"));
        return;
    }

    nvrhi::rt::State State;
    State.setShaderTable(ShaderTable.Get());
    if (BindingSet)
    {
        State.addBindingSet(BindingSet.Get());
    }
    if (DescriptorTable)
    {
        State.addBindingSet(DescriptorTable);
    }

    CmdList->setRayTracingState(State);

    nvrhi::rt::DispatchRaysArguments Args;
    Args.width = Desc.Width;
    Args.height = Desc.Height;
    Args.depth = Desc.Depth;

    CmdList->dispatchRays(Args);
}

void FRayTracingPipeline::DispatchRays(nvrhi::ICommandList* CmdList, uint32_t Width, uint32_t Height, uint32_t Depth,
    nvrhi::BindingSetHandle BindingSet, nvrhi::IDescriptorTable* DescriptorTable)
{
    FDispatchDesc Desc{};
    Desc.Width = Width;
    Desc.Height = Height;
    Desc.Depth = Depth;
    DispatchRays(CmdList, Desc, BindingSet, DescriptorTable);
}

void FRayTracingPipeline::DispatchRays(nvrhi::ICommandList* CmdList, const FDispatchDesc& Desc,
    nvrhi::BindingSetHandle SRVBindingSet, nvrhi::BindingSetHandle UAVBindingSet)
{
    if (!bShaderTableBuilt)
    {
        HLVM_LOG(LogRenderer, err, TXT("FRayTracingPipeline: BuildShaderTable() must be called before DispatchRays()"));
        return;
    }

    nvrhi::rt::State State;
    State.setShaderTable(ShaderTable.Get());
    if (SRVBindingSet)
    {
        State.addBindingSet(SRVBindingSet.Get());
    }
    if (UAVBindingSet)
    {
        State.addBindingSet(UAVBindingSet.Get());
    }

    CmdList->setRayTracingState(State);

    nvrhi::rt::DispatchRaysArguments Args;
    Args.width = Desc.Width;
    Args.height = Desc.Height;
    Args.depth = Desc.Depth;

    CmdList->dispatchRays(Args);
}

void FRayTracingPipeline::DispatchRays(nvrhi::ICommandList* CmdList, uint32_t Width, uint32_t Height, uint32_t Depth,
    nvrhi::BindingSetHandle SRVBindingSet, nvrhi::BindingSetHandle UAVBindingSet)
{
    FDispatchDesc Desc{};
    Desc.Width = Width;
    Desc.Height = Height;
    Desc.Depth = Depth;
    DispatchRays(CmdList, Desc, SRVBindingSet, UAVBindingSet);
}

void FRayTracingPipeline::Shutdown()
{
    Pipeline = nullptr;
    ShaderTable = nullptr;
    BindingLayout = nullptr;
    BindlessLayout = nullptr;
    AdditionalBindingLayouts.clear();
    bHasBindlessLayout = false;
    RayGenShader = nullptr;
    ClosestHitShader = nullptr;
    MissShader = nullptr;
    ShadowMissShader = nullptr;
    LayoutBuilder.reset();
    HitGroups.clear();
    bIsInitialized = false;
    bPipelineFinalized = false;
    bShaderTableBuilt = false;
}
