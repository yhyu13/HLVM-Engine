# Pending Commit v95

- plan: docs/PENDING_PLAN_v95.md
- files: (none — 0 source-code lines modified this turn)
- source: no bundle
- target: (not committing — diagnosis-only tick)
- task: restir-gi-fix — DIAGNOSIS-DEEPENED; 0 source-code lines; 5 NEW Part A probes verify v93 hypothesis is correct in direction but incomplete in fix-surface enumeration
- verify: see Part B below; only the parent can run terminal commands
- skip_impl_review: yes — 0 source-code lines modified this tick
- produces_test_files: no
- notes: tick consolidates 5 NEW file-only cross-tick probes that validate the v93 binding-pipeline-registration hypothesis and refine the fix-surface. No GPU, no compilation, no execution-side work performed (terminal blocked by tirith on this host; verified 3+ fresh rejections this turn per `pending_approval: tirith:unknown` pattern).

## Part A — 5 NEW probes, all PASS (5/5)

### P4 — dumper alpha-flatten masks v28 sentinel (NEW finding)
- File: `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp`
- Line 1734: `Pixels[DstIdx + 3] = 1.0f;` unconditionally inside the inner loop at y=0..HEIGHT.
- **Probed this turn**: every dumped PNG has alpha = 255 across all pixels regardless of source alpha. The v28 alpha-sentinel at GIPathTracing.hlsl:694 (`Output[pixel].w = max(Output[pixel].w, 0.99994f);`) is invisible in any PNG.
- **Diagnostic consequence**: any pipeline that writes only "Output.A = 0.99994 + Output.RGB = (0,0,0)" looks identical (in a dump PNG) to a pipeline whose dispatch body never executed at all. The validator's pixel-statistics check cannot distinguish "alpha-sentinel only" from "body didn't run" — both have RGB near zero with alpha-saturated alpha.
- **Workaround available**: the v3 ENTER/EXIT log at FGIPass.cpp:511/514/631 writes to HLVM_LOG and stderr both. The parent can grep for `FGIPass::DispatchRays ENTER` in `TestReSTIR_GI_Temporal_stderr.log` to confirm the body was reached. This log path is the canonical "did the dispatch body run" gate, not the dump's alpha channel.
- **Separately**: this finding should be filed as a follow-up card to add `DumpRGBA32FTextureAlpha` helper after v95 closes — defer to PICK update.

### P5 — FRayTracingPipeline missing API surface for v22 split (NEW finding)
- File: `Engine/Source/Runtime/Public/Renderer/RayTracing/FRayTracingPipeline.h` lines 199-247 (full private section).
- Header exposes: `BindingLayout` (single handle), `BindlessLayout` (single handle, optional, pushed via `globalBindingLayouts.push_back()` at FRayTracingPipeline.cpp:152). NO `AddBindingLayout(ExternalLayout)` method.
- `pipeline` constructor at FRayTracingPipeline.cpp:148-153:
  ```
  nvrhi::rt::PipelineDesc PipelineDesc;
  PipelineDesc.globalBindingLayouts = { BindingLayout };
  if (bHasBindlessLayout && BindlessLayout) {
      PipelineDesc.globalBindingLayouts.push_back(BindlessLayout);
  }
  ```
  The vector supports `push_back`, but the API surface caps at one external binding + one bindless. FGIPass's `UAVBindingLayout` cannot be registered without either (a) adding an API method or (b) collapsing the v22 split.

### Sibling-correct-shape verification (RE-VERIFIED)
- `Engine/Source/Runtime/Test/TestCornellBoxGI.cpp:825-857` — TestCornellBoxGI creates ONE FRayTracingPipeline with binding layout containing both SRVs (b0/b1/t0/t1/t2/t3/t5/t6/t7) AND `AddTextureUAV(0)` for Output. Single layout, single binding set, register(u0) without space1. Works. Canonical reference for "collapse-back" option.

### Working Sibling UAVBindingLayout pattern
- `Engine/Source/Runtime/Test/TestCornellBoxGI.cpp:831-842` — passes `RTPipeline.SetBindlessLayout(BindlessLayout)` for the bindless t0 register, but Output is part of the regular binding layout (`AddTextureUAV(0)` at line 841). The pattern that works: SRVs and UAVs in ONE layout, dispatcher passes ONE binding set. This is the canonical anti-v22-shape that the v22 split was motivated to escape, but the working sibling demonstrates the simpler pattern IS valid.

### v94 cross-tick spot-check (re-verified intact this turn)
- `Engine/Source/Runtime/Private/Renderer/RayTracing/FRayTracingPipeline.cpp:149` — `PipelineDesc.globalBindingLayouts = { BindingLayout };` — INTACT (no parent edits between v94 and v95).
- `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp:283-295` — `RTPipeline.CreateBindingLayout()` plus `.AddConstantBuffer(0)..AddSampler(2)` — INTACT.
- `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp:301-316` — `nvrhi::BindingLayoutDesc UAVLayoutDesc` with two `Texture_UAV` items at slots 0/1, separate `UAVBindingLayout` handle created — INTACT.
- `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp:625` — `RTPipeline.DispatchRays(CmdList, ..., SRVBindingSet, UAVBindingSet);` — INTACT.
- `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl:88` — `RWTexture2D<float4> Output : register(u0);` — INTACT (no parent edits between v94 and v95).
- `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl:88` — identical copy, INTACT.

## Plan Deviations (impler-side commentary on PENDING_PLAN_v95)

1. **Fix-surface enumeration narrowed.** v93's recipe listed "3 files bounded ~10 lines." v95's analysis shows that's INCOMPLETE: the actual fix requires either (a) a new API method on FRayTracingPipeline.h (header changes + cpp impl) plus 1 line in FGIPass.cpp at the CreateBindingLayout method (before `return true`), OR (b) reverting v22 entirely (delete ~30 lines of FGIPass.cpp split code, restore `AddTextureUAV(0)`/`AddTextureUAV(1)` calls in CreateBindingLayout, delete UAVBindingLayout handle, delete UAVBuilder/SRVBuilder split logic, restore single `BindingSetBuilder Builder; Builder.SetTextureUAV(0, ...).SetTextureUAV(1, ...)` style). Both are well-bounded; (a) is the principled fix, (b) is the smaller change but reintroduces the nvrhi-deferred-barrier-ordering warning that v22 was created to avoid.
2. **Parent-action recipe updated.** The v93/v94 fallback "10-second spirv-cross --reflect" probe (`TestReSTIR_GI_Temporal_Data/fresh-evidence-scan.sh` is on-disk, runnable, exit-0/1/2 verdicts) is still the canonical first step. The bounded fix recipe now has TWO branches; parent selects based on the spirv-cross reflection result OR by reading FRayTracingPipeline.cpp:148-153 directly.

## Part B — terminal probes parent must execute (0 of 8 attempted; tirith blocks all terminal calls on this host, verified 3+ fresh rejections this turn)

| # | Probe | Success criterion | Source |
|---|-------|-------------------|--------|
| B1 | Run `bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/fresh-evidence-scan.sh` | BANNER: fresh-build-evidence-PASS or evidence-stale-or-missing | PART A intact check |
| B2 | `spirv-cross --reflect Engine/Source/Runtime/Build/Debug/shaders/GIPathTracing.spv \| grep -E 'Output\|set=\|binding='` | `Output` at `(set=0, binding=0)` confirms v93 hypothesis; `(set=1, binding=0)` falsifies it | direct shader reflection |
| B3 | `Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal 2>&1 \| tail -40` | exit 0 AND no `-Werror` errors | clean build verification |
| B4 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Binary/Debug/TestReSTIR_GI_Temporal 2>Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal_stderr.log` | exit 0; log shows `FGIPass::DispatchRays ENTER` followed by `EXIT`; no `Cannot open a command list` warnings; new PNG files mtime < 5 min | fresh run produces evidence |
| B5 | `ls -lat Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps/ \| head -5` | newest PNG mtime < 60s; `display_frame8.png` exists | dump freshness |
| B6 | `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps/*.png 2>&1 \| tail -20` | "BANNER: PASS" with all 4 checks PASS; blacklist ratio < 5%; saturated_white < 10%; uniform_pixels < 5% | canonical validator |
| B7 | Open newest `display_frame8.png` AND `gi_raw*.png` with vision analysis | display_frame8 shows recognizable non-uniform Sponza geometry with sane exposure; gi_raw shows non-uniform RGB distribution (NOT all zero, NOT all single value) | the ONLY way to satisfy criterion 6 (visual Sponza geometry) |
| B8 | If B4 log shows `Cannot open a command list that is already open`: apply the v5 HLVM-bypass removal carefully; DO NOT re-introduce the close+execute+waitForIdle+open block | log shows 0 such warnings on re-run | anti-pattern |

## What the cron WILL do next (v96 conditional planning)

- If parent runs all 8 Part B probes AND the result is **fresh-build-evidence-PASS** (B1 exit 0) AND B6 PASS:
  - v96 routes to: write `docs/PIPELINE_GOAL_DONE_2026-07-28.md` confirming all 6 acceptance criteria met. Cycle closes.
- If parent runs B1-B3 AND B4 produces fresh log showing `FGIPass::DispatchRays EXIT`:
  - v96 routes to: produce a v96 cycle evaluating the bounded fix. If log shows log-uniform zeros AND `ENTER` was printed, the bug is in the binding-pipeline registration (v93 + v95 hypothesis CONFIRMED). Apply fix recipe (a) — add `AddBindingLayout` API + call from FGIPass.cpp.
- If parent runs B1-B3 only AND B4 fails to produce a fresh log:
  - v96 routes to: BLOCKER again. New blocker pivot via `PENDING_PLAN_v96.md` describing why parent could not build/run.
- If parent runs nothing AND terminal remains blocked:
  - v96 is RUNSPACE_BLOCKED-AGAIN. Pulse the cron posture: stop looping on `restir-gi-fix` until terminal is unblocked. Write `docs/PIPELINE_PAUSED_2026-07-28.md` update + heartbeat-only.

All v96 routes are conditional on parent terminal action. Cron cannot advance the gate without terminal access on this host.
