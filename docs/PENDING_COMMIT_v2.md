# Pending Commit v2 — revert v22 split in FGIPass

- plan: docs/PENDING_PLAN_v2.md
- files:
  - Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp
  - Engine/Source/Runtime/Private/Renderer/GI/FGIPass.h
  - Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl
  - Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl
- source: docs/DIAGNOSTIC_2026-07-30.md (no source bundle)
- target: working tree (cron tick v2; not committed)
- task: Revert FGIPass's v22 split. Single binding set now contains SRV + UAV. HLSL UAV declarations no longer carry `space1` qualifier. Use single binding set in DispatchRays.
- verify:
  - `./Build.sh --Rebuild --Config=Debug --Target=TestReSTIR_GI_Temporal --Test` exits 0
  - `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 HLVM_PT_DEBUG_MODE=20` produces non-black gi_raw dump
  - `HLVM_PT_DEBUG_MODE=22` (GBufferWorldPos SRV read) returns non-zero std
  - `validate_restir_gi.py` passes structural checks on latest dump
- skip_impl_review: no (substantial refactor + HLSL/C++ coordination)
- produces_test_files: no
- notes:
  - The HLSL files (private canonical + test data-dir copy) MUST stay in sync.
    Both have `register(u0, space1)` → `register(u0)` etc. The test-data copy
    is what `create_restir_gi_temporal_shadermake` actually compiles.
  - The `ReSTIR_Temporal_cs.hlsl` and `ReSTIR_Generate_cs.hlsl` files in
    `TestReSTIR_GI_Temporal_Data/` also use `register(u0, space1)` etc. for
    their compute-pipeline UAVs — DO NOT change those. Their pipeline is
    separate and uses the v22 split style intentionally.
  - The `commitBarriers()` call between binding set creation and dispatch
    (previously at line 760, removed in this commit) was specifically for
    the two-set split. With a single set, nvrhi's automatic barrier tracking
    handles the layout transition; the explicit commit was redundant.
    The early `commitBarriers()` at line 572 (BEFORE binding set creation)
    is KEPT — that one was for the SRV-only barrier race documented in
    v135, not the v22 split.

## Diff summary

### FGIPass.cpp
- `CreateBindingLayout`: removed separate `UAVBindingLayout` construction
  (50+ lines). Merged `AddTextureUAV(0..2)` into the primary builder.
- `DispatchRays`: removed separate `UAVBuilder` and `UAVBindingSet`
  construction (~110 lines). Added `SetTextureUAV(0..2)` to the
  existing `SRVBuilder`. Dispatch now uses the single-set overload.
- `Shutdown`: removed `UAVBindingLayout = nullptr` line.

### FGIPass.h
- Removed `UAVBindingLayout` member.

### GIPathTracing.hlsl (both copies)
- `register(u0, space1)` → `register(u0)` for Output
- `register(u1, space1)` → `register(u1)` for DebugStatsTexture
- `register(u2, space1)` → `register(u2)` for OutputDirection

## Plan Deviations

**None.** The impl follows the plan exactly. All four files edited.
No new test files. No new code paths. The revert is mechanical.

## Pre-impl hypothesis (preserved for tick audit)

The v22 split was added to silence a Vulkan validation warning
(VUID-VkDescriptorImageInfo-imageLayout-00344) about SRV/UAV image
layout transitions within a single binding set. The proven-control
TestCornellBoxGI uses a single binding set without that warning
causing data corruption. The hypothesis is that the warning was
non-fatal but the split introduced an actual binding descriptor
mismatch that caused GBuffer SRV reads to return zero. Reverting the
split should restore correctness at the cost of bringing back the
non-fatal validation warning (which is invisible because validation
is stubbed).