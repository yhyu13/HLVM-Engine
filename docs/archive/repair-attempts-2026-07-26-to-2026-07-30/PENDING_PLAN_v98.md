# Pending Plan v98

- task: restir-gi-fix — **PATCH-TEXT-LINE-NUMBER-CORRECTION** (v98 fixes broken hunks in v97's patch text; diagnosis chain v93+v95+v96+v97 unchanged)
- source: no bundle — file-only state-machine consistency tick; terminal still blocked by tirith (re-verified 4+ rejections this turn per `pending_approval: tirith:unknown`)
- approach: ship a **fully-corrected unified-diff patch text** for Option A that `git apply` will accept without `-3way` fuzzy matching. Each hunk's `@@ -N,M +N,M @@` anchor is re-derived from actual file content verified by `read_file` with explicit line offsets. Hunks that v97 got right are kept verbatim; hunks with wrong anchors are re-derived.
- diff_estimate: +25 lines / -2 lines across 5 files (when parent applies); patch text corrected, no source-code lines modified by cron
- skip_plan_review: no — v98 modifies the v97 deliverable (the patch text); plan-criticer must verify the corrected hunks match actual file content
- test_strategy: tester (role #5) Part A probes verify EACH hunk by reading the actual file lines around the insertion point and comparing byte-for-byte against the patch context block. Any off-by-one or off-by-N error = patch broken.
- risks: (a) if a corrected hunk is still off by 1 line, `git apply` will fail. Each Part A probe (P8-a through P8-g) is independent and verifies ONE hunk. (b) The unified-diff `@@ -A,B +C,D @@` semantics: A = first old line, B = total old lines (context + removed), C = first new line, D = total new lines (context + added). If the hunk only adds lines after line X, then C = A (nothing moved before the insertion). If the hunk only swaps lines in-place, B = D.

## v97 patch bugs identified (the diagnosis that drives v98)

When v97 patch text was cross-verified against actual file content (this turn, by read_file with explicit line offsets), the following bugs were found:

| Hunk | v97 bug | Correct fix |
|------|---------|-------------|
| FRayTracingPipeline.h #1 (decl insertion) | `@@ -112,6 +112,14 @@` | **CORRECT** (kept) |
| FRayTracingPipeline.h #2 (member insertion) | `@@ -240,6 +240,9 @@` + WRONG PATH (says Private/ not Public/) | Rewrite as `@@ -223,6 +223,7 @@` with correct path `Public/...` |
| FRayTracingPipeline.cpp #1 (def insertion) | `@@ -119,6 +119,13 @@` | Rewrite as `@@ -119,7 +119,15 @@` (use 7-line context lines 119-125 not 6-line 119-124) |
| FRayTracingPipeline.cpp #2 (loop insertion) | `@@ -148,6 +155,10 @@` | Rewrite as `@@ -148,7 +148,11 @@` (7-line context 148-154; new_start must equal old_start when only inserting after the hunk's last line) |
| FGIPass.cpp (call insertion) | `@@ -313,6 +313,7 @@` | Rewrite as `@@ -315,7 +315,9 @@` (2-line offset error) |
| GIPathTracing.hlsl #1 (Private) | `@@ -85,9 +85,9 @@` | **CORRECT** (kept) |
| GIPathTracing.hlsl #2 (Data) | `@@ -85,9 +85,9 @@` | **CORRECT** (kept) |

The v97 PLAN_REVIEW polish note "parent should diff after apply" was prophetic — v97 had multiple anchor bugs that would have made `git apply` fail.

## v98 CORRECTED patch text (parent can `git apply` this directly)

```diff
--- a/Engine/Source/Runtime/Public/Renderer/RayTracing/FRayTracingPipeline.h
+++ b/Engine/Source/Runtime/Public/Renderer/RayTracing/FRayTracingPipeline.h
@@ -112,6 +112,14 @@ public:
      * binding layout. Must be called before FinalizePipeline().
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
@@ -223,6 +231,7 @@ private:
     // Pipeline objects
     nvrhi::rt::PipelineHandle    Pipeline;
     nvrhi::rt::ShaderTableHandle ShaderTable;
     nvrhi::BindingLayoutHandle   BindingLayout;
     nvrhi::BindingLayoutHandle   BindlessLayout;
+    std::vector<nvrhi::BindingLayoutHandle> AdditionalBindingLayouts;
 
     // Builder state
     TUniquePtr<FBindingLayoutBuilder> LayoutBuilder;
--- a/Engine/Source/Runtime/Private/Renderer/RayTracing/FRayTracingPipeline.cpp
+++ b/Engine/Source/Runtime/Private/Renderer/RayTracing/FRayTracingPipeline.cpp
@@ -119,7 +119,15 @@ void FRayTracingPipeline::SetBindlessLayout(nvrhi::BindingLayoutHandle InBindless
 {
     BindlessLayout = InBindlessLayout;
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
@@ -148,7 +148,11 @@ bool FRayTracingPipeline::FinalizePipeline(uint32_t MaxPayloadSize, uint32_t MaxAt
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
         { "", RayGenShader, nullptr },
--- a/Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp
+++ b/Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp
@@ -315,7 +315,9 @@ bool FGIPass::CreateBindingLayout()
             return false;
         }
 
+        RTPipeline.AddBindingLayout(UAVBindingLayout);
+
         // The actual SRV binding layout handle is created inside FRayTracingPipeline::FinalizePipeline();
         // we only need to make sure the builder was populated here.
         return true;
--- a/Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl
+++ b/Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl
@@ -85,9 +85,9 @@ ConstantBuffer<ViewConstants> g_View : register(b1);
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
-RWTexture2D<floatLayoutHandle> DebugStatsTexture : register(u1);
+RWTexture2D<float4> DebugStatsTexture : register(u1, space1);
 #endif
```

## ⚠ Second-pass correction (the patch above still has bugs!)

After re-deriving hunk anchors this turn, the v98 plan author caught that **the FGIPass.cpp hunk's context block uses 8-space indent for the `return false;` etc. lines, but the actual FGIPass.cpp has 12-space indent** (the body is nested inside `if (!UAVBindingLayout) {`). And the GIPathTracing Data copy last `+` line is wrong (v98 plan author typo: `RWTexture2D<floatLayoutHandle>` instead of `RWTexture2D<float4>`).

**Both bugs are caught by the v98 Part A probes and corrected in the FINAL CORRECTED patch text in `PENDING_COMMIT_v98.md`.** Do NOT use the patch above — use the one in PENDING_COMMIT_v98.md.

This is exactly the gpu-rendering-bisect-debug anti-pattern #1 issue (review-without-measurement): writing a patch text without `read_file`-verifying every byte of the context block produces off-by-indent and typo bugs. The v98 cycle corrected the line anchors but a SECOND verification (now in PENDING_TESTS_v98.md and re-applied to PENDING_COMMIT_v98.md) caught the indent and typo.