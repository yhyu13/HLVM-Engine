# Pending Tests v95

- task: restir-gi-fix — DIAGNOSIS-DEEPENED; this tester tick re-defines the verification surface for v95
- tester: tester (role 5 — same head, single-profile caveat per gpu-rendering-bisect-debug anti-pattern #7)
- timestamp: 2026-07-28T22:15:45Z

## Part A — 5/5 PASS (file-only spot-checks; cost: zero terminal invocations)

| # | Probe | File / line | Expected | Got |
|---|-------|-------------|----------|-----|
| P4-a | dumper alpha-flatten | `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp:1734` | `Pixels[DstIdx + 3] = 1.0f;` (unconditional) | PASS — exact match |
| P4-b | per-channel norm preserves alpha at 1.0 | `TestReSTIR_GI_Temporal.cpp:1764-1766` | Only R/G/B rescaled, A untouched (still 1.0) | PASS — exact match |
| P5-a | FRayTracingPipeline.h has `BindingLayout` member | `Engine/Source/Runtime/Public/Renderer/RayTracing/FRayTracingPipeline.h:225` | `nvrhi::BindingLayoutHandle   BindingLayout;` | PASS |
| P5-b | NO `AddBindingLayout` API exists in header | header full content lines 99-114 | No matching method | PASS — confirmed missing |
| P5-c | FRayTracingPipeline.cpp only registers 2 layouts in globalBindingLayouts | `FRayTracingPipeline.cpp:148-153` | `{ BindingLayout }` plus optional `BindlessLayout` push | PASS — exact match |

**Result**: 5/5 PASS. All Part A probes are mechanical file-only greps that confirm the v95 DIAGNOSIS-DEEPENED hypothesis holds in the on-disk source.

## Part B — 8 terminal probes parent must execute (0 of 8 attempted; tirith blocks all `terminal` calls on this host — same `pending_approval: tirith:unknown` pattern reproduced 3+ times this turn)

| # | Probe | Cost | Success criterion | Maps to acceptance criterion |
|---|-------|------|-------------------|------------------------------|
| B1 | `bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/fresh-evidence-scan.sh` | ~5 sec | BANNER: fresh-build-evidence-PASS or evidence-stale-or-missing | patch inventory intact check |
| B2 | `spirv-cross --reflect Engine/Source/Runtime/Build/Debug/shaders/GIPathTracing.spv \| grep Output` | ~10 sec | reports `(set=N, binding=0)`. `(0, 0)` ⇒ v93/v95 hypothesis confirmed. `(1, 0)` ⇒ hypothesis falsified | disambiguation between hypothesis (a) "missing push_back" and hypothesis (b) "collapse needed" |
| B3 | `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal 2>&1 \| tail -40` | ≤2 min | exit 0 AND no `-Werror,-Wold-style-cast` chain errors | criterion 1 "Debug target builds cleanly" |
| B4 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Binary/Debug/TestReSTIR_GI_Temporal 2>.../TestReSTIR_GI_Temporal_stderr.log` | ~10 sec | exit 0; log shows `FGIPass::DispatchRays ENTER` + `EXIT`; 0 `Cannot open a command list` warnings; new PNGs mtime < 60s | criteria 2, 3 |
| B5 | `ls -lat Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps/ \| head -5` | ~1 sec | newest PNG < 60s old; `display_frame8.png` exists | criterion 2 (fresh dumps) |
| B6 | `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py <dumps>/*.png 2>&1 \| tail -20` | ~5 sec | BANNER: PASS with all 4 checks PASS | criterion 5 |
| B7 | Open newest `display_frame8.png` and `gi_raw*.png` with vision analysis | human-time | display_frame8 shows recognizable non-uniform Sponza geometry with sane exposure; gi_raw shows non-uniform RGB distribution (not all zero, not single value) | criterion 6 (visual Sponza recognition) |
| B8 | If B4 log shows `Cannot open a command list that is already open`, do NOT re-introduce the close+execute+waitForIdle+open block (v5 removal rationale) | conditional | log shows 0 such warnings on re-run | criterion 3 |

## Per-probe verdict (Part A only — terminal probes UNVERIFIED)
- P4-a, P4-b, P5-a, P5-b, P5-c: ALL PASS (file-only).

## Tester's note on the diagnosis
The v93 diagnosis is correct in direction: v22 split is half-applied to FGIPass, Output register(u0) doesn't bind to UAVBindingLayout because UAVBindingLayout is never registered on the pipeline. v95 sharpens this to: the API surface for registering a second layout is missing entirely (P5-b). Parent's bounded fix recipe must either (a) add an `AddBindingLayout` API to FRayTracingPipeline.h (1 method declaration + 1 method impl + 1 call site in FGIPass.cpp) OR (b) collapse the v22 split entirely (revert ~30 lines in FGIPass.cpp, restore `AddTextureUAV(0)` + `AddTextureUAV(1)` to the CreateBindingLayout chain — matches TestCornellBoxGI sibling at lines 831-842 which works without split). Option (a) is the principled fix; option (b) is smaller but reintroduces the nvrhi-deferred-barrier-ordering pattern that v22 was created to fix. Tester notes option (a) is recommended.
