// Copyright 2026 HLVM Engine
//
// MIT License

#include "Renderer/RenderGraph/FRenderGraph.h"
#include "Core/Log.h"

DECLARE_LOG_CATEGORY(LogRenderGraph)

void FRenderGraph::ImportTexture(const FString& Name, nvrhi::TextureHandle Texture)
{
    ImportedTextures[Name] = Texture;
}

void FRenderGraph::AddPass(const FPassDesc& Desc)
{
    FPassNode Node;
    Node.Name = Desc.Name;
    Node.Inputs = Desc.Inputs;
    Node.Outputs = Desc.Outputs;
    Node.Execute = Desc.Execute;
    Passes.push_back(std::move(Node));
}

bool FRenderGraph::Compile()
{
    bCompiled = false;
    ExecutionOrder.clear();

    if (Passes.empty())
    {
        bCompiled = true;
        return true;
    }

    // =====================================================================
    // Build resource -> producer map
    // =====================================================================
    TMap<FString, uint32_t> ResourceToProducer;

    // Imported textures are considered "produced" by pass index INVALID (we'll handle separately)
    for (const auto& Pair : ImportedTextures)
    {
        ResourceToProducer[Pair.first] = static_cast<uint32_t>(-1);
    }

    for (uint32_t i = 0; i < Passes.size(); ++i)
    {
        for (const auto& Output : Passes[i].Outputs)
        {
            // Only record the FIRST producer of a resource.
            // Later passes that also write to it will be checked for WAW hazards.
            auto It = ResourceToProducer.find(Output);
            if (It == ResourceToProducer.end())
            {
                ResourceToProducer[Output] = i;
            }
        }
    }

    // =====================================================================
    // Validate: every input must have a producer
    // =====================================================================
    for (uint32_t i = 0; i < Passes.size(); ++i)
    {
        for (const auto& Input : Passes[i].Inputs)
        {
            auto It = ResourceToProducer.find(Input);
            if (It == ResourceToProducer.end())
            {
                HLVM_LOG(LogRenderGraph, err,
                    TXT("FRenderGraph: Pass '{}' requires input '{}' which is not produced by any pass or imported"),
                    *Passes[i].Name, *Input);
                return false;
            }
        }
    }

    // =====================================================================
    // Build adjacency list and in-degree array for Kahn's algorithm
    // =====================================================================
    const uint32_t N = static_cast<uint32_t>(Passes.size());
    TVector<TVector<uint32_t>> Adjacency(N);
    TVector<uint32_t> InDegree(N, 0);

    for (uint32_t ConsumerIdx = 0; ConsumerIdx < N; ++ConsumerIdx)
    {
        for (const auto& Input : Passes[ConsumerIdx].Inputs)
        {
            auto It = ResourceToProducer.find(Input);
            if (It != ResourceToProducer.end() && It->second != static_cast<uint32_t>(-1))
            {
                uint32_t ProducerIdx = It->second;
                Adjacency[ProducerIdx].push_back(ConsumerIdx);
                ++InDegree[ConsumerIdx];
            }
        }
    }

    // =====================================================================
    // Kahn's topological sort
    // =====================================================================
    TVector<uint32_t> Queue;
    Queue.reserve(N);
    for (uint32_t i = 0; i < N; ++i)
    {
        if (InDegree[i] == 0)
        {
            Queue.push_back(i);
        }
    }

    TVector<uint32_t> Sorted;
    Sorted.reserve(N);

    size_t QueueRead = 0;
    while (QueueRead < Queue.size())
    {
        uint32_t U = Queue[QueueRead++];
        Sorted.push_back(U);

        for (uint32_t V : Adjacency[U])
        {
            if (--InDegree[V] == 0)
            {
                Queue.push_back(V);
            }
        }
    }

    // =====================================================================
    // Cycle detection
    // =====================================================================
    if (Sorted.size() != N)
    {
        HLVM_LOG(LogRenderGraph, err, TXT("FRenderGraph: Cycle detected in pass dependencies"));
        return false;
    }

    // =====================================================================
    // WAW hazard detection: two passes write to same output (no consumer between them)
    // We allow WAW if the second pass reads the first pass's output, forming a chain.
    // Otherwise it's ambiguous.
    // =====================================================================
    TMap<FString, uint32_t> OutputWriter;
    for (uint32_t Idx : Sorted)
    {
        for (const auto& Output : Passes[Idx].Outputs)
        {
            auto It = OutputWriter.find(Output);
            if (It != OutputWriter.end())
            {
                // Check if the later pass consumes the earlier pass's output
                bool bIsChained = false;
                for (const auto& Input : Passes[Idx].Inputs)
                {
                    if (Input == Output)
                    {
                        bIsChained = true;
                        break;
                    }
                }
                if (!bIsChained)
                {
                    HLVM_LOG(LogRenderGraph, err,
                        TXT("FRenderGraph: Write-after-write hazard on '{}'. Pass '{}' and Pass '{}' both write to it without a read chain."),
                        *Output, *Passes[It->second].Name, *Passes[Idx].Name);
                    return false;
                }
            }
            OutputWriter[Output] = Idx;
        }
    }

    // =====================================================================
    // Store execution order
    // =====================================================================
    ExecutionOrder.reserve(N);
    for (uint32_t Idx : Sorted)
    {
        ExecutionOrder.push_back(Passes[Idx].Name);
    }

    bCompiled = true;
    HLVM_LOG(LogRenderGraph, info,
        TXT("FRenderGraph: Compiled {} passes, execution order: {}"),
        N,
        *FString::Join(ExecutionOrder, [](const FString& Name) { return Name.ToTCharCStr(); }, TXT(" -> ")));
    return true;
}

void FRenderGraph::Execute(nvrhi::ICommandList* CmdList) const
{
    if (!bCompiled)
    {
        HLVM_LOG(LogRenderGraph, err, TXT("FRenderGraph::Execute called before Compile()"));
        return;
    }

    if (!CmdList)
    {
        HLVM_LOG(LogRenderGraph, err, TXT("FRenderGraph::Execute called with null command list"));
        return;
    }

    // Build name -> index lookup
    TMap<FString, uint32_t> NameToIndex;
    for (uint32_t i = 0; i < Passes.size(); ++i)
    {
        NameToIndex[Passes[i].Name] = i;
    }

    for (const auto& PassName : ExecutionOrder)
    {
        auto It = NameToIndex.find(PassName);
        if (It != NameToIndex.end())
        {
            const auto& Pass = Passes[It->second];
            if (Pass.Execute)
            {
                Pass.Execute(CmdList);
            }
        }
    }
}

void FRenderGraph::Clear()
{
    Passes.clear();
    ImportedTextures.clear();
    ExecutionOrder.clear();
    bCompiled = false;
}
