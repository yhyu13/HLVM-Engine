/**
 * Copyright 2026 HLVM Engine
 *
 * MIT License
 *
 * TestRenderGraph - Unit tests for FRenderGraph dependency validation and sorting
 *
 * These tests validate the graph logic without requiring a Vulkan device.
 */

#include "Test.h"
#include "Renderer/RenderGraph/FRenderGraph.h"

DECLARE_LOG_CATEGORY(LogTest)

RECORD(TestRenderGraph_TopologicalSort, true)
{
    FRenderGraph Graph;
    Graph.AddPass({TXT("A"), {}, {TXT("TexA")}, nullptr});
    Graph.AddPass({TXT("B"), {TXT("TexA")}, {TXT("TexB")}, nullptr});
    Graph.AddPass({TXT("C"), {TXT("TexB")}, {TXT("TexC")}, nullptr});

    HLVM_ENSURE(Graph.Compile());
    HLVM_ENSURE(Graph.GetExecutionOrder().size() == 3);
    HLVM_ENSURE(Graph.GetExecutionOrder()[0] == FString(TXT("A")));
    HLVM_ENSURE(Graph.GetExecutionOrder()[1] == FString(TXT("B")));
    HLVM_ENSURE(Graph.GetExecutionOrder()[2] == FString(TXT("C")));

    HLVM_LOG(LogTest, info, TXT("TestRenderGraph_TopologicalSort passed"));
}

RECORD(TestRenderGraph_MissingInput, true)
{
    FRenderGraph Graph;
    Graph.AddPass({TXT("A"), {}, {TXT("TexA")}, nullptr});
    Graph.AddPass({TXT("B"), {TXT("MissingTex")}, {TXT("TexB")}, nullptr});

    HLVM_ENSURE(!Graph.Compile());
    HLVM_ENSURE(!Graph.IsCompiled());

    HLVM_LOG(LogTest, info, TXT("TestRenderGraph_MissingInput passed"));
}

RECORD(TestRenderGraph_CycleDetection, true)
{
    FRenderGraph Graph;
    Graph.AddPass({TXT("A"), {TXT("TexC")}, {TXT("TexA")}, nullptr});
    Graph.AddPass({TXT("B"), {TXT("TexA")}, {TXT("TexB")}, nullptr});
    Graph.AddPass({TXT("C"), {TXT("TexB")}, {TXT("TexC")}, nullptr});

    HLVM_ENSURE(!Graph.Compile());
    HLVM_ENSURE(!Graph.IsCompiled());

    HLVM_LOG(LogTest, info, TXT("TestRenderGraph_CycleDetection passed"));
}

RECORD(TestRenderGraph_OptionalPasses, true)
{
    // Core chain: A -> B
    // Optional C that reads A
    FRenderGraph Graph;
    Graph.AddPass({TXT("A"), {}, {TXT("TexA")}, nullptr});
    Graph.AddPass({TXT("B"), {TXT("TexA")}, {TXT("TexB")}, nullptr});

    // Without C
    HLVM_ENSURE(Graph.Compile());
    HLVM_ENSURE(Graph.GetExecutionOrder().size() == 2);
    Graph.Clear();

    // With C
    Graph.AddPass({TXT("A"), {}, {TXT("TexA")}, nullptr});
    Graph.AddPass({TXT("B"), {TXT("TexA")}, {TXT("TexB")}, nullptr});
    Graph.AddPass({TXT("C"), {TXT("TexA")}, {TXT("TexC")}, nullptr});

    HLVM_ENSURE(Graph.Compile());
    HLVM_ENSURE(Graph.GetExecutionOrder().size() == 3);
    // C must come after A, but B and C order relative to each other is arbitrary
    // (both depend only on A). We just verify A is first.
    HLVM_ENSURE(Graph.GetExecutionOrder()[0] == FString(TXT("A")));

    HLVM_LOG(LogTest, info, TXT("TestRenderGraph_OptionalPasses passed"));
}

RECORD(TestRenderGraph_ImportedTextures, true)
{
    FRenderGraph Graph;
    Graph.ImportTexture(TXT("ExternalTex"), nullptr);
    Graph.AddPass({TXT("A"), {TXT("ExternalTex")}, {TXT("TexA")}, nullptr});
    Graph.AddPass({TXT("B"), {TXT("TexA")}, {TXT("TexB")}, nullptr});

    HLVM_ENSURE(Graph.Compile());
    HLVM_ENSURE(Graph.GetExecutionOrder().size() == 2);
    HLVM_ENSURE(Graph.GetExecutionOrder()[0] == FString(TXT("A")));
    HLVM_ENSURE(Graph.GetExecutionOrder()[1] == FString(TXT("B")));

    HLVM_LOG(LogTest, info, TXT("TestRenderGraph_ImportedTextures passed"));
}

RECORD(TestRenderGraph_WriteAfterWriteHazard, true)
{
    // Two passes write to same texture without a read chain = error
    FRenderGraph Graph;
    Graph.AddPass({TXT("A"), {}, {TXT("TexA")}, nullptr});
    Graph.AddPass({TXT("B"), {}, {TXT("TexA")}, nullptr});

    HLVM_ENSURE(!Graph.Compile());

    HLVM_LOG(LogTest, info, TXT("TestRenderGraph_WriteAfterWriteHazard passed"));
}

RECORD(TestRenderGraph_ChainedWriteAfterWrite, true)
{
    // Two passes write to same texture, but second reads first's output = OK
    FRenderGraph Graph;
    Graph.AddPass({TXT("A"), {}, {TXT("TexA")}, nullptr});
    Graph.AddPass({TXT("B"), {TXT("TexA")}, {TXT("TexA")}, nullptr});

    HLVM_ENSURE(Graph.Compile());
    HLVM_ENSURE(Graph.GetExecutionOrder().size() == 2);

    HLVM_LOG(LogTest, info, TXT("TestRenderGraph_ChainedWriteAfterWrite passed"));
}

RECORD(TestRenderGraph_EmptyGraph, true)
{
    FRenderGraph Graph;
    HLVM_ENSURE(Graph.Compile());
    HLVM_ENSURE(Graph.IsCompiled());
    HLVM_ENSURE(Graph.GetExecutionOrder().empty());

    HLVM_LOG(LogTest, info, TXT("TestRenderGraph_EmptyGraph passed"));
}

RECORD(TestRenderGraph_ExecuteWithoutCompile, true)
{
    FRenderGraph Graph;
    Graph.AddPass({TXT("A"), {}, {TXT("TexA")}, nullptr});

    // Execute before compile should not crash
    Graph.Execute(nullptr);
    HLVM_ENSURE(!Graph.IsCompiled());

    HLVM_LOG(LogTest, info, TXT("TestRenderGraph_ExecuteWithoutCompile passed"));
}
