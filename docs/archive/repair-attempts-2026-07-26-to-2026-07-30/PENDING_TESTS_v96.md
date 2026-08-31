# Pending Tests v96

- task: restir-gi-fix — RUNSPACE_BLOCKED_PIVOT; this tester tick verifies the P6-a finding and reaffirms v95's 8-probe Part B recipe
- tester: tester (role 5 — same head, single-profile caveat per gpu-rendering-bisect-debug anti-pattern #7)
- timestamp: 2026-07-28T22:35:45Z

## Part A — 4/4 PASS (file-only spot-checks; cost: zero terminal invocations)

| # | Probe | File / line | Expected | Got |
|---|-------|-------------|----------|-----|
| P6-a (header) | `SetBindingLayout` declared | `Engine/Source/Runtime/Public/Renderer/RayTracing/FRayTracingPipeline.h:103-106` | `void SetBindingLayout(nvrhi::BindingLayoutHandle ExternalLayout);` | PASS — exact match |
| P6-a (impl)  | `SetBindingLayout` implemented | `FRayTracingPipeline.cpp:112-117` | `BindingLayout = ExternalLayout; bUsingExternalLayout = true; LayoutBuilder.reset();` | PASS — exact match; semantically REPLACE not APPEND |
| P6-a (verify no append) | No second push_back to globalBindingLayouts in impl file | `FRayTracingPipeline.cpp:148-153` | Only `{ BindingLayout }` + optional `BindlessLayout.push_back` | PASS — no other layout pushed |
| v95 cross-tick | v95 spot-checks intact | `FGIPass.cpp:301-316` + `GIPathTracing.hlsl:88` + `FRayTracingPipeline.cpp:148-153` | All intact | PASS — 3/3 intact |

**Result**: 4/4 PASS. All Part A probes are mechanical file-only greps that confirm v96's heartbeat-tick posture holds: v93+v95 diagnosis intact; P6-a sharpens v95 P5-b description (SetBindingLayout exists but is REPLACE not APPEND).

## Part B — 8 terminal probes parent must execute (0 of 8 attempted; tirith blocks all `terminal` calls on this host — same `pending_approval: tirith:unknown` pattern reproduced 3+ times this turn)

Same 8 probes as v95. Refined B2 with P6-a finding:

| # | Probe | Cost | Success criterion | Maps to acceptance criterion |
|---|-------|------|-------------------|------------------------------|
| B1 | `bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/fresh-evidence-scan.sh` | ~5 sec | BANNER: fresh-build-evidence-PASS or evidence-stale-or-missing | patch inventory intact check |
| B2 | `spirv-cross --reflect Engine/Source/Runtime/Build/Debug/shaders/GIPathTracing.spv | grep Output` | ~10 sec | reports `(set=N, binding=0)`. **(0, 0) ⇒ Option B (collapse) is right path.** **(1, 0) ⇒ Option A (add append-API) is right path.** | disambiguation between Option A and Option B |
| B3 | `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal 2>&1 | tail -40` | ≤2 min | exit 0 AND no `-Werror,-Wold-style-cast` chain errors | criterion 1 |
| B4 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Binary/Debug/TestReSTIR_GI_Temporal 2>.../TestReSTIR_GI_Temporal_stderr.log` | ~10 sec | exit 0; log shows `FGIPass::DispatchRays ENTER` + `EXIT`; 0 `Cannot open a command list` warnings; new PNGs mtime < 60s | criteria 2, 3 |
| B5 | `ls -lat Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps/ | head -5` | ~1 sec | newest PNG < 60s old; `display_frame8.png` exists | criterion 2 |
| B6 | `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py <dumps>/*.png 2>&1 | tail -20` | ~5 sec | BANNER: PASS with all 4 checks PASS | criterion 5 |
| B7 | Open newest `display_frame8.png` and `gi_raw*.png` with vision analysis | human-time | display_frame8 shows recognizable non-uniform Sponza geometry with sane exposure; gi_raw shows non-uniform RGB distribution | criterion 6 |
| B8 | If B4 log shows `Cannot open a command list that is already open`, do NOT re-introduce close+execute+waitForIdle+open block | conditional | log shows 0 such warnings on re-run | criterion 3 |

## Per-probe verdict (Part A only — terminal probes UNVERIFIED)
- P6-a (header), P6-a (impl), P6-a (verify no append), v95 cross-tick: ALL PASS (file-only).

## Tester's note on the diagnosis
v93+v95 diagnosis is intact and cross-tick verified. v96 P6-a sharpens v95 P5-b description: `SetBindingLayout(ExternalLayout)` exists in the API but is REPLACE-not-APPEND; the v95 Option A (add an APPEND-style `AddBindingLayout` method) remains the principled fix. The cron's file-only diagnostic value is exhausted; parent terminal action is required per `PIPELINE_RUNSPACE_BLOCKED_2026-07-28.md` Options A/B/C.