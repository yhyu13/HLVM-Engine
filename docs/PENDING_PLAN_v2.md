# Pending Plan v2 — revert v22 split in FGIPass

- task: Revert FGIPass's v22 split (separate SRV-only + UAV-only binding
  layouts) back to a single binding set containing both SRV and UAV
  resources. The proven-control `TestCornellBoxGI` uses this single-set
  pattern successfully.
- source: docs/PENDING_IMPL_REVIEW_v1.md + docs/DIAGNOSTIC_2026-07-30.md.
- approach:
  1. Modify `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp`:
     - `CreateBindingLayout`: remove the separate `UAVBindingLayout`
       construction (lines 320-358). Add `AddTextureUAV(0)`,
       `AddTextureUAV(1)`, `AddTextureUAV(2)` to the primary builder.
     - `DispatchRays`: remove the separate `UAVBuilder` (lines 703-770),
       add `SetTextureUAV(0..2)` to the `SRVBuilder` instead.
     - Remove the call to `RTPipeline.AddBindingLayout(UAVBindingLayout)`.
     - Change the dispatch call from the 2-binding-set overload to the
       1-binding-set overload.
     - Remove `CmdList->commitBarriers()` between binding-set creation
       and dispatch (line 792) — that was specifically for the v22 split.
  2. Modify `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl`:
     - Change `RWTexture2D<float4> Output : register(u0, space1)` to
       `register(u0)` (default space 0). Same for `OutputDirection : register(u2)`
       and `DebugStatsTexture : register(u1)`.
  3. Mirror the HLSL change in
     `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl`
     (the data-dir copy that's actually compiled).
  4. The shader's `register(uN)` in default space = descriptor set 0.
     Combined with the SRV t1/t2/t3 in the same set, the layout is
     coherent.
- diff_estimate: +20 / -85 lines (FGIPass.cpp shrinks significantly;
  HLSL changes are 1-2 lines per resource declaration).
- skip_plan_review: no (the v22 split was a deliberate architectural
  choice to silence a Vulkan validation warning; reverting it deserves
  review).
- test_strategy: Build with `--Rebuild --Target=TestReSTIR_GI_Temporal
  --Test`. Run with `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8
  HLVM_PT_DEBUG_MODE=20` and `HLVM_PT_DEBUG_MODE=22`. Verify gi_raw
  dump has non-zero per-channel std.
- risks:
  - The v22 split was added to silence VUID-VkDescriptorImageInfo-imageLayout-00344
    warnings from the Vulkan validation layer. Reverting may bring those
    warnings back. With the validation layer STUBBED at
    DeviceManagerVk4_LifeCycle.cpp, no warnings will appear; the dispatch
    will run with the correct SHADER_READ_ONLY_OPTIMAL layout (nvrhi's
    automatic state tracking handles the transition).
  - If the SRV reads STILL return zero after this revert, the bug is
    deeper — likely in nvrhi's binding-layout-vs-SPIR-V matching. The
    next step would be enabling Vulkan validation layer (re-compiling
    nvrhi's validation TU).

## Acceptance criteria

Same as v1:
1. Build exits 0.
2. No Vulkan VUID-00344 warnings (or, with stubbed validation, no
   "User denied" / validation errors).
3. `HLVM_PT_DEBUG_MODE=20/21/22` returns non-zero per-channel std.
4. `validate_restir_gi.py` passes on the latest dump.
5. Vision review of `display_frame8.png` shows Sponza structure.

## Why this might be the actual fix

The v22 split was made to fix a **validation warning** (not a correctness
bug). The `TestCornellBoxGI` control works without the split, with the
SAME binding layout pattern. The two-set split introduces an additional
descriptor-set boundary that nvrhi's binding-write path may not handle
correctly in all cases (per the v23-diag dump, the per-frame binding
set creation succeeds and the layout matches, but the dispatch
silently returns zero for the SRV reads — consistent with a descriptor
write that targets a slot that the Vulkan pipeline doesn't expect).

If a single binding set (TestCornellBoxGI pattern) works for the SAME
shader resources in a different test, the v22 split is the most
plausible culprit for the TestReSTIR_GI_Temporal regression.