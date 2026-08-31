# Pending Plan v97

- task: restir-gi-fix — **RUNSPACE_BLOCKED_PIVOT + NEW PRECISE-PATCH-TEXT-READY** (semantic continuing v94 RUNSPACE_BLOCKED → v95 DIAGNOSIS_DEEPENED → v96 RUNSPACE_BLOCKED_PIVOT; v97 ships a verbatim apply-command patch text for parent)
- source: no bundle — file-only state-machine consistency tick; terminal still blocked by tirith (re-verified 4+ rejections this turn per `pending_approval: tirith:unknown`)
- approach: produce ONE new file-only artifact — a copy-paste-ready `git apply` patch text for **Option A** (the principled fix) that the parent can apply verbatim, then rebuild and validate. The patch is structurally complete (5 files / +25 lines, mirrors existing `SetBindlessLayout` API) and matches the proven-correct shape used by FReSTIRPass.cpp:246-247 (the working sibling that registers BOTH SRV + UAV layouts via two `addBindingLayout()` calls). No source-code lines are written this tick; the patch text is delivered as a marker for parent application. This is the next mechanically actionable step in file-only runspace per the user's instruction "If blocked by an external issue, record exact evidence in a marker and continue with the next mechanically actionable fix".
- diff_estimate: +25 lines / -2 lines across 5 files (when parent applies)
- skip_plan_review: yes — v96 KEEP; v97 is patch-text-delivery only, no diagnostic re-framing
- test_strategy: tester (role #5) produces a 1-step parent-side verification recipe (4 commands) that should produce fresh dumps + validator PASS if the v97 patch is correct. Part B verification (10s `spirv-cross --reflect GIPathTracing.spv` to CONFIRM/FALSIFY v93) remains the cheapest disambiguation before applying the patch.
- risks: (a) The patch text may have a typo invisible to file-only review; parent should diff after apply. (b) The Option A surface change (new `AddBindingLayout` API) is symmetric with existing `SetBindlessLayout` and inherits its pattern, so the regression surface is bounded — the only consumer is `FGIPass::CreateBindingLayout()` calling `RTPipeline.AddBindingLayout(UAVBindingLayout)`. (c) Without terminal, the patch cannot be verified by the cron; parent must do the rebuild + validate step. This is honest and matches the gpu-rendering-bisect-debug skill's anti-fabrication rule.

## NEW: verbatim patch text for Option A (parent can `git apply` this)

```diff
--- a/Engine/Source/Runtime/Public/Renderer/RayTracing/FRayTracingPipeline.h
+++ b/Engine/Source/Runtime/Public/Renderer/RayTracing/FRayTracingPipeline.h
@@ -112,6 +112,14 @@ public:
      */
     void SetBindlessLayout(nvrhi::BindingLayoutHandle InBindlessLayout);

+    /**
+     * @brief Append an additional binding layout to globalBindingLayouts
+     *
+     * APPEND-style API for pipelines that need a second binding layout
+     * alongside the main one (e.g. v22 split: SRV-only + UAV-only).
+     * Must be called before FinalizePipeline().
+     */
+    void AddBindingLayout(nvrhi::BindingLayoutHandle InLayout);
+
     /**
      * @brief Create the ray tracing pipeline from loaded shaders + binding layout
      * @param MaxPayloadSize Maximum ray payload size in bytes
--- a/Engine/Source/Runtime/Private/Renderer/RayTracing/FRayTracingPipeline.cpp
+++ b/Engine/Source/Runtime/Private/Renderer/RayTracing/FRayTracingPipeline.cpp
@@ -119,6 +119,13 @@ void FRayTracingPipeline::SetBindlessLayout(nvrhi::BindingLayoutHandle InBindless
     bHasBindlessLayout = true;
 }

+void FRayTracingPipeline::AddBindingLayout(nvrhi::BindingLayoutHandle InLayout)
+{
+    if (InLayout)
+    {
+        AdditionalBindingLayouts.push_back(InLayout);
+    }
+}
+
 bool FRayTracingPipeline::FinalizePipeline(uint32_t MaxPayloadSize, uint32_t MaxAttributeSize)
 {
@@ -148,6 +155,10 @@ bool FRayTracingPipeline::FinalizePipeline(uint32_t MaxPayloadSize, uint32_t MaxAt
     nvrhi::rt::PipelineDesc PipelineDesc;
     PipelineDesc.globalBindingLayouts = { BindingLayout };
     if (bHasBindlessLayout && BindlessLayout)
     {
         PipelineDesc.globalBindingLayouts.push_back(BindlessLayout);
     }
+    for (const auto& Layout : AdditionalBindingLayouts)
+    {
+        PipelineDesc.globalBindingLayouts.push_back(Layout);
+    }
     PipelineDesc.shaders = {
--- a/Engine/Source/Runtime/Private/Renderer/RayTracing/FRayTracingPipeline.h
+++ b/Engine/Source/Runtime/Private/Renderer/RayTracing/FRayTracingPipeline.h
@@ -240,6 +240,9 @@ private:
     nvrhi::BindingLayoutHandle BindingLayout;
     nvrhi::BindingLayoutHandle BindlessLayout;
+    std::vector<nvrhi::BindingLayoutHandle> AdditionalBindingLayouts;
     bool bUsingExternalLayout = false;
     bool bHasBindlessLayout = false;
--- a/Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp
+++ b/Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp
@@ -313,6 +313,7 @@ bool FGIPass::CreateBindingLayout()
         return false;
     }

+    RTPipeline.AddBindingLayout(UAVBindingLayout);
     // The actual SRV binding layout handle is created inside FRayTracingPipeline::FinalizePipeline();
     // we only need to make sure the builder was populated here.
     return true;
--- a/Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl
+++ b/Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl
@@ -85,9 +85,9 @@ ConstantBuffer<ViewConstants> g_View : register(b1);
 // =============================================================================
 // Resources
 // =============================================================================

-RWTexture2D<float4> Output : register(u0);
+RWTexture2D<float4> Output : register(u0, space1);

 #if GI_DEBUG_STATS
-RWTexture2D<float4> DebugStatsTexture : register(u1);
+RWTexture2D<float4> DebugStatsTexture : register(u1, space1);
 #endif
--- a/Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl
+++ b/Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl
@@ -85,9 +85,9 @@ ConstantBuffer<ViewConstants> g_View : register(b1);
 // Resources
 // =============================================================================

-RWTexture2D<float4> Output : register(u0);
+RWTexture2D<float4> Output : register(u0, space1);

 #if GI_DEBUG_STATS
-RWTexture2D<float4> DebugStatsTexture : register(u1);
+RWTexture2D<float4> DebugStatsTexture : register(u1, space1);
 #endif
```

## Parent-side apply + verify recipe (after `git apply restir-gi-fix-v97.patch`)

```bash
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild && \
  HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal && \
  python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py
```

Acceptance: 6/6 criteria per `PENDING_TESTS_v97.md` Part B. UNVERIFIED in this runspace (terminal blocked).