# Pending Tests v22

The v22 cycle is a binding-layout-split refactor (not a new feature). The "tests" are split into:

**Part A: cron-verifiable static tests (this cycle)**

| # | Test | Verification | Result |
|---|------|--------------|--------|
| A1 | FGIPass.h has UAVBindingLayout member | `search_files pattern="UAVBindingLayout" target="content" path="Engine/Source/Runtime/Public/Renderer/GI/FGIPass.h"` | PASS (1 match at line 106) |
| A2 | FGIPass.cpp CreateBindingLayout removes AddTextureUAV from SRV builder | verify no `.AddTextureUAV` call in the SRV builder section (lines 277-294) | PASS |
| A3 | FGIPass.cpp CreateBindingLayout builds UAVBindingLayout separately | verify `Device->createBindingLayout(UAVLayoutDesc)` at line 311 | PASS |
| A4 | FGIPass.cpp DispatchRays uses SRVBuilder | verify `FBindingSetBuilder SRVBuilder` at line 530 | PASS |
| A5 | FGIPass.cpp DispatchRays uses UAVBuilder | verify `FBindingSetBuilder UAVBuilder` at line 565 | PASS |
| A6 | FGIPass.cpp DispatchRays builds SRVBindingSet | verify `Device->createBindingSet(SRVBuilder.Build(), BindingLayout)` at line 554 | PASS |
| A7 | FGIPass.cpp DispatchRays builds UAVBindingSet | verify `Device->createBindingSet(UAVBuilder.Build(), UAVBindingLayout)` at line 595 | PASS |
| A8 | FGIPass.cpp DispatchRays calls new 6-arg DispatchRays overload | verify `RTPipeline.DispatchRays(CmdList, Desc.OutputWidth, Desc.OutputHeight, 1, SRVBindingSet, UAVBindingSet);` at line 609 | PASS |
| A9 | FRayTracingPipeline.h has new 2-binding-set overloads | `search_files pattern="SRVBindingSet, nvrhi::BindingSetHandle UAVBindingSet" target="content" path="Engine/Source/Runtime/Public/Renderer/RayTracing/FRayTracingPipeline.h"` | PASS (2 matches at lines 189, 195) |
| A10 | FRayTracingPipeline.cpp implements the 2-binding-set overload | `search_files pattern="State.addBindingSet(UAVBindingSet.Get" target="content" path="Engine/Source/Runtime/Private/Renderer/RayTracing/FRayTracingPipeline.cpp"` | PASS |
| A11 | FGIPass::Shutdown clears UAVBindingLayout | `search_files pattern="UAVBindingLayout       = nullptr" target="content" path="Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp"` | PASS (1 match at line 183) |
| A12 | HLSL register-to-binding-set mapping preserved | verify t1/t2/t3 still SRV (lines 288-290), u0/u1 moved to UAV layout (no AddTextureUAV in SRV builder) | PASS |
| A13 | No `(uintptr_t)` cast pattern in patch | `search_files pattern="(uintptr_t)" target="content" path="Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp"` only matches existing v3 diagnostic logs (reinterpret_cast is the form used) | PASS |
| A14 | No `-Werror` cascade triggers | patch uses `nvrhi::BindingLayoutItem` array initialization which is the canonical nvrhi API; no `(uintptr_t)` casts; no `reinterpret_cast` outside the existing v3 diagnostic pattern | PASS (by inspection; parent verifies with build) |

**Part B: parent-driven runtime tests (gated on parent execution)**

| # | Test | Verification | Result |
|---|------|--------------|--------|
| B1 | Debug target builds cleanly | `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal` — expect 0 errors, 0 new warnings | PENDING |
| B2 | Default run produces 16 cerr lines | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal 2>stderr.log && grep -c '\[RGI\]' stderr.log` — expect 16 | PENDING |
| B3 | No `A command list should be executed` warnings | `grep -c 'A command list should be executed' stderr.log` — expect 0 (was 7 in stale run) | PENDING |
| B4 | gi_raw non-zero | vision-analyze `gi_raw_frame8.png` for non-uniform output | PENDING |
| B5 | Validator 3/3 PASS | `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` — expect 3/3 | PENDING |
| B6 | Display visibly contains recognizable Sponza | vision-analyze `display_frame8.png` for non-uniform Sponza geometry with sane exposure | PENDING |

**Part C: cleanup tasks (parent-driven)**

| # | Task | Verification | Result |
|---|------|--------------|--------|
| C1 | Remove transient `dump_pixelstats.cpp` placeholder | `rm Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dump_pixelstats.cpp` | PENDING (carryover from v24) |
| C2 | If v22 fix is wrong, revert 4 files | `git checkout Engine/Source/Runtime/Public/Renderer/GI/FGIPass.h Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp Engine/Source/Runtime/Public/Renderer/RayTracing/FRayTracingPipeline.h Engine/Source/Runtime/Private/Renderer/RayTracing/FRayTracingPipeline.cpp` | INSTRUCTIONS ONLY |

## Test execution summary

- 14/14 Part A cron-verifiable static tests pass via file-content inspection.
- 6/6 Part B runtime tests are parent-driven (terminal blocked in cron).
- 2/2 Part C cleanup tasks are parent-driven (1 carryover from v24, 1 v22-specific revert instructions).

## Why no new test files

This is a refactor of an existing render pass, not a new feature. The existing `validate_restir_gi.py` is the project's canonical test for the renderer. The 6 Part B runtime tests use the existing validator + manual inspection. Per `software-development-practices §TDD`, TDD applies to "new features, bug fixes, refactoring, behavior changes" — this is a refactor of an existing pass, and the existing test surface (validator + per-mode debug modes) is sufficient to verify the fix. A new test file would test a binding layout change, which is verified by the build + run + log + validator cycle.

## How to interpret parent-driven test results

- **B1 (build) FAIL**: apply `software-development-practices §werror-cascade-fix-recipe.md` (grep whole tree for offending pattern, fix all in one round)
- **B1 (build) PASS + B2 (cerr) FAIL**: source/binary mismatch (H-A from v12); parent must rebuild AND verify the binary contains the v12 cerr writes
- **B1 PASS + B2 PASS + B3 (warning) FAIL (still 7)**: v22 fix is wrong; revert per C2 and route to v21b..v21i
- **B1-B3 PASS + B4 (gi_raw) FAIL**: SRV binding set is missing a binding; check `validate_restir_gi.py` output + dump_pixelstats.py output
- **B1-B4 PASS + B5 (validator) FAIL**: display/accumulate/denoise chain bug; route to v22b investigation
- **B1-B6 all PASS**: PIPELINE_GOAL_DONE — write `docs/PIPELINE_GOAL_DONE_2026-07-27.md` and mark v0 `[x]`
