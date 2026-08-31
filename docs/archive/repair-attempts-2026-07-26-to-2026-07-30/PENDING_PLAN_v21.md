# Pending Plan v21 — Fix the FGIPass nvrhi-deferred-barrier-ordering bug by splitting the SRV + UAV binding set into two binding sets dispatched in sequence

- task: implement the canonical nvrhi-deferred-barrier-ordering fix per `gpu-rendering-bisect-debug` `references/nvrhi-deferred-barrier-ordering.md` for the FGIPass shader. The current FGIPass::DispatchRays has a single binding set with both SRV (t1/t2/t3 GBuffer textures → SHADER_READ_ONLY_OPTIMAL) AND UAV (u0 OutputTexture → GENERAL) bindings. nvrhi's `setComputeState` (and `setRayTracingState` for RT) records `vk::bindDescriptorSets` BEFORE `commitBarriers`, so the descriptors go live with stale image layouts, which matches the `DeviceManager.cpp:52` "A command list should be executed before it is reopened" warning pattern observed 7x per stale run.
- source: file-only patch (no source-code modification of TestReSTIR_GI_Temporal.cpp, GIPathTracing.hlsl, or any other renderer file outside the GI binding layout). The fix touches only `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp` and the corresponding header at `Engine/Source/Runtime/Public/Renderer/GI/FGIPass.h` (which is currently empty/missing — needs to be created or the binding layout split lives entirely in FGIPass.cpp).
- approach:
  1. **Phase 1 (parent-evidence gate, NOT executed in this cycle):** wait for parent to run `run_rgi_diagnostic.sh` and paste back `rgi_evidence.txt`. The 9-branch decision matrix in PENDING_PICK.md lines 141-150 will identify which v21a..v21i sub-plan to execute.
  2. **Phase 2 (this plan covers v21a, the highest-priority branch):** if the v20 evidence shows `DeviceManager.cpp:52` warning still fires AND the GI dispatch returns but `gi_raw` is 0,0,0, then v21a executes: split the FGIPass binding layout into SRV-only + UAV-only binding sets, dispatch in two phases.
  3. **Concrete v21a code change** (staged but NOT applied until parent evidence arrives):
     - In `FGIPass::CreateBindingLayout()` (FGIPass.cpp:260-296), create TWO binding layouts via the existing `FBindingLayoutBuilder`:
       - `SRVBindingLayout` (AddConstantBuffer(0) + AddConstantBuffer(1) + AddRayTracingAccelStruct(0) + AddTextureSRV(1) + AddTextureSRV(2) + AddTextureSRV(3) + AddStructuredBufferSRV(5/6/7/8) + AddSampler(2)) — all read-only.
       - `UAVBindingLayout` (AddTextureUAV(0) + AddTextureUAV(1)) — write-only.
     - In `FGIPass::DispatchRays()` (FGIPass.cpp:455-581), build TWO binding sets:
       - `SRVBindingSet = Device->createBindingSet(SRVBuilder.Build(), SRVBindingLayout)`
       - `UAVBindingSet = Device->createBindingSet(UAVBuilder.Build(), UAVBindingLayout)`
     - Add a third overload to `FRayTracingPipeline::DispatchRays(CmdList, Width, Height, Depth, SRVBindingSet, UAVBindingSet)` that calls `State.addBindingSet(SRVBindingSet.Get())` then `State.addBindingSet(UAVBindingSet.Get())` in that order. nvrhi's `rt::State` supports multiple binding sets (verified at `Engine/Source/Runtime/Private/Renderer/RayTracing/FRayTracingPipeline.cpp:304-332` which already has the 7-arg overload with a descriptor table).
  4. **Phase 3 (post-impl):** rebuild and re-run the diagnostic; verify the `DeviceManager.cpp:52` warning count drops to 0; verify `gi_raw` shows non-zero scene-shape; verify validator 3/3.
- diff_estimate: +60 / -30 lines (FGIPass.cpp only; FRayTracingPipeline.h gets a new DispatchRays overload ~10 lines, FRayTracingPipeline.cpp gets its implementation ~12 lines; FGIPass.h gets a new SRVBindingLayout/UAVBindingLayout member pair ~6 lines; FGIPass::CreateBindingLayout splits into two builders ~+20/-20 lines; FGIPass::DispatchRays splits the SetBuilder into two builders ~+15/-15 lines).
- skip_plan_review: no — the binding-layout split is high-risk (changes the dispatch contract, requires verifying nvrhi's multi-binding-set RT support, requires re-running the diagnostic).
- test_strategy: parent-driven build + run + log + validator + vision analysis after each phase.
- risks:
  - **Risk A: nvrhi's RT state may not support multiple binding sets with conflicting layouts.** Need to verify by reading nvrhi source (`Engine/Source/Runtime/ThirdParty/NVRHI/...`) for the `rt::State` implementation. If unsupported, the fix is to dispatch the SRV-bind in one phase and the UAV-bind in a separate dispatch, with a `commitBarriers` between them.
  - **Risk B: pipeline recompile.** The pipeline's `globalBindingLayouts` references `BindingLayout` (now SRVBindingLayout); the UAV binding set is a per-frame addition. The HLSL register-to-binding-set mapping may need to be explicit in the binding set ordering. Mitigation: keep both binding sets referencing the same HLSL register indices (b0/b1/t0/t1/t2/t3/t5/t6/t7/t8/s2/u0/u1); only the binding-set grouping changes.
  - **Risk C: slangc RT shader-table binding.** RT shader tables bind shader-record descriptors, not bind-set entries. The per-frame binding sets are separate from the shader table. Should be unaffected.
  - **Risk D: max binding set count per pipeline.** nvrhi limits vary by backend; splitting 1 → 2 should be safe (Vulkan allows 4 sets per pipeline by default; 2 is well within bounds).
  - **Risk E: cascading -Werror build break.** Per the gpu-rendering-bisect-debug `references/werror-cascade-fix-recipe.md`, the new `State.addBindingSet` calls may trigger `-Wold-style-cast` warnings if any cast sneaks in. Mitigation: grep the patch for `(uintptr_t)` and `reinterpret_cast` patterns before committing.
  - **Risk F: tangent — single-head freshness caveat.** The plan-criticer, impler, reviewer, tester, and testing-verifier are all the same head as the planner (per the six-role-pipeline anti-pattern #7). The binding-layout split is a substantial refactor; the freshness caveat applies. Mitigation: explicit acknowledgment in the plan + impl + review markers.

## Why this cycle is correct

The v23 inner-pipeline heartbeat (`docs/PIPELINE_HEALTH_2026-07-27_v23.md`) and v22 heartbeat (`docs/PIPELINE_HEALTH_2026-07-27_v22.md`) both identified the nvrhi-deferred-barrier-ordering pattern as the highest-confidence root cause of the GI dispatch not landing data on `OutputTexture`. The evidence chain is:

1. Stale `TestReSTIR_GI_Temporal.log` shows `DeviceManager.cpp:52` warning fires 7x per run.
2. FGIPass binding layout (FGIPass.cpp:277-291) has SRV (t1/t2/t3) + UAV (u0) in the same binding set.
3. This is the canonical nvrhi-deferred-barrier pattern from `gpu-rendering-bisect-debug/nvrhi-deferred-barrier-ordering.md`.
4. The v20 evidence (when parent runs the diagnostic) will confirm by showing the warning count + gi_raw values.
5. Even with full diagnostic surface (modes 1-15 + default-case), the underlying barrier ordering prevents the UAV write from landing in the correct layout at submit time.

The fix is mechanical and well-documented in the skill reference. The cost is moderate (60 lines refactor); the risk is bounded by the explicit HLSL register preservation; the verification is parent-driven (build + run + log + validator + vision).

## Decision tree

v21a is the highest-priority branch from PICK line 141-150 (hypothesis #1: all probes match but mode-0 still broken → nvrhi-deferred-barrier-ordering is the likely cause, fix the binding layout split).

The other 8 branches from PICK lines 141-150 will be staged as v21b..v21i once the v20 evidence arrives:

- v21b (mode 6 works but mode 12 fails → AmbientColor uniform bind fix)
- v21c (mode 6/7 work but mode 8 crashes → TraceRay isolation)
- v21d (modes 6/7/8/9 all 0 + default works → slangc dead-strip investigation)
- v21e (modes 6/7/8/9 all 0 + default also 0 → debugMode reach investigation)
- v21f (mode 10 = 0 but mode 15 = 15.0 → divide-by-256 issue)
- v21g (mode 11 = 0 → View cbuffer not bound)
- v21h (cerr does NOT fire → stderr buffering investigation)
- v21i (build fails → -Werror cascade-aware fix recipe)

## Implementation outline (v21a only — staged not applied)

```cpp
// FGIPass.h additions
nvrhi::BindingLayoutHandle SRVBindingLayout;
nvrhi::BindingLayoutHandle UAVBindingLayout;

// FGIPass::CreateBindingLayout() — split into two
bool FGIPass::CreateBindingLayout()
{
    auto& SRVBuilder = RTPipeline.CreateBindingLayout();
    SRVBuilder.SetVisibility(nvrhi::ShaderType::All)
              .AddConstantBuffer(0)             // b0 - GIConstants
              .AddConstantBuffer(1)             // b1 - ViewConstants
              .AddRayTracingAccelStruct(0)      // t0 - SceneBVH
              .AddTextureSRV(1)                 // t1 - GBufferWorldPos
              .AddTextureSRV(2)                 // t2 - GBufferNormal
              .AddTextureSRV(3)                 // t3 - GBufferMaterial
              .AddStructuredBufferSRV(5)        // t5 - RTVertices
              .AddStructuredBufferSRV(6)        // t6 - RTIndices
              .AddStructuredBufferSRV(7)        // t7 - Lights
              .AddStructuredBufferSRV(8)        // t8 - RTInstanceInfo
              .AddSampler(2);                   // s2 - LinearSampler

    // UAV layout built separately via the device
    nvrhi::BindingLayoutDesc UAVDesc;
    UAVDesc.visibility = nvrhi::ShaderType::All;
    UAVDesc.bindings = {
        { 0, nvrhi::BindingType::Texture_UAV, 1, nvrhi::ShaderStage::All },     // u0 - OutputTexture
        { 1, nvrhi::BindingType::Texture_UAV, 1, nvrhi::ShaderStage::All },     // u1 - DebugStatsTexture
    };
    UAVBindingLayout = Device->createBindingLayout(UAVDesc);
    return true;
}

// FGIPass::DispatchRays() — split binding set into SRV + UAV
FBindingSetBuilder SRVBuilder;
SRVBuilder.SetConstantBuffer(0, ConstantBuffer)
          .SetConstantBuffer(1, Desc.ViewConstants)
          .SetRayTracingAccelStruct(0, Desc.SceneTLAS)
          .SetTextureSRV(1, Desc.GBufferWorldPos)
          .SetTextureSRV(2, Desc.GBufferNormal)
          .SetTextureSRV(3, Desc.GBufferMaterial);
// ... (same as before for StructuredBufferSRVs + Sampler)

nvrhi::BindingSetHandle SRVBindingSet = Device->createBindingSet(SRVBuilder.Build(), BindingLayout);

FBindingSetBuilder UAVBuilder;
UAVBuilder.SetTextureUAV(0, Desc.OutputTexture)
          .SetTextureUAV(1, DebugStatsUAV);

nvrhi::BindingSetHandle UAVBindingSet = Device->createBindingSet(UAVBuilder.Build(), UAVBindingLayout);

RTPipeline.DispatchRays(CmdList, Desc.OutputWidth, Desc.OutputHeight, 1, SRVBindingSet, UAVBindingSet);
```

## Files
- Modify: `Engine/Source/Runtime/Public/Renderer/GI/FGIPass.h` (create if missing, add 2 binding layout members ~6 lines)
- Modify: `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp` (split CreateBindingLayout + DispatchRays ~+50/-30 lines)
- Modify: `Engine/Source/Runtime/Public/Renderer/RayTracing/FRayTracingPipeline.h` (add 2-binding-set DispatchRays overload ~10 lines)
- Modify: `Engine/Source/Runtime/Private/Renderer/RayTracing/FRayTracingPipeline.cpp` (implement new overload ~12 lines)

## Verification (parent-driven, not by cron)

After v21a is applied and rebuilt:

1. Run `bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/run_rgi_diagnostic.sh`
2. Verify `rgi_evidence.txt` shows:
   - Build: 0 errors
   - Cerr fire check (default): >=8 Render + >=8 FGIPass lines
   - Default mode dump: >=1 PNG with non-zero gi_raw
   - Validator: 3/3 PASS
3. Vision-analyze `display_frame8.png` for recognizable non-uniform Sponza geometry
4. Verify `DeviceManager.cpp:52` warning count in `rgi_default.stderr` is 0 (vs 7 in stale log)
5. If all 4 pass: write `docs/PIPELINE_GOAL_DONE_2026-07-27.md` and mark v0 task `[x]` in PENDING_PICK.md

## Notes for impl-reviewer

- v21a is a structural refactor of the FGIPass binding layout. The HLSL register-to-binding mapping is preserved exactly; only the binding-set grouping changes.
- The fix is conditional on v20 evidence: if v20 evidence does NOT show the nvrhi-deferred-barrier pattern (e.g., no `DeviceManager.cpp:52` warning, gi_raw non-zero, validator 3/3), v21a is the wrong fix and a different v21b..v21i sub-plan should be staged instead.
- The fix is staged in this plan but NOT applied in this cycle. Application requires parent evidence to confirm the v20 evidence shape, which gates the v21a decision.
