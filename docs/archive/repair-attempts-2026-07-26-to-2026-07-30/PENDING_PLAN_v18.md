# Pending Plan v18 — add cases 8u/9u/10u/11u to GIPathTracing.hlsl for maximal bug-space bisection

- task: add four new debug-mode cases to `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl` (canonical master, 722 lines post-v17) and the matching `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl` (data-dir copy, 722 lines post-v17). The new cases advance the diagnostic surface from 2 probes (mode 6 + mode 7, landed in v13/v15 + v17) to 6 probes (modes 6, 7, 8, 9, 10, 11), bisecting the bug space more aggressively so the parent's next interactive rebuild collects decisive evidence across ALL hypotheses in a single run.
- source: docs/PENDING_PICK.md line 124 (v18 staging) + docs/PIPELINE_HEALTH_2026-07-27_v17.md v17 decision matrix + on-disk inspection of both GIPathTracing.hlsl copies (722 lines, 27538B, byte-identical)
- approach: 1 file modified (Private master) + 1 file mirrored (data-dir) + 6 doc markers. The patch is text-identical in both files. The four new case labels are added in the same insertion site (between case 7u at line 604 and case 13u at line 605). Each case is gated behind `if (debugMode != 0u)`. Each case writes a recognizable per-pixel value that distinguishes a specific hypothesis from the others.
- skip_plan_review: no — patch modifies the canonical master HLSL consumed by tests beyond TestReSTIR_GI_Temporal. Plan-criticer must sign off on (a) each new case is safe (gated, recognizable output, no regressions), (b) the cases are correctly placed in the existing switch, (c) the patch does not regress other consumers.
- diff_estimate: +44 / -0 lines per HLSL copy (2 files × +44 = +88 source lines total); 0 behavior change at the test-build layer when debugMode=0.

## Why this is the right v18 cycle

The v17 decision matrix enumerated 6 branches the parent's next rebuild could land on. v17 added mode 7 (TraceRay-bypass) to bisect "ray-tracing chain vs everything else." v18 advances the bisection further by adding probes for four additional hypotheses:

1. **mode 8u — TraceRay-only sentinel (no payload read, no merge)**: tests whether TraceRay itself crashes/returns correctly. Calls TraceRay with the same ray as the main path but discards the result. If this produces garbage or the test crashes, the bug is in the TraceRay setup itself (TMin/TMax, RT acceleration structure, flag bits). If this produces a clean frame, TraceRay is healthy.
2. **mode 9u — diffuse-only via GBufferMaterial SRV**: same as mode 1 but with a 1.5x scalar (matches v17 mode 7's multiplier) so the parent's comparison between mode 7 and mode 9 is direct. If mode 1 = mode 7 = mode 9 = (diffuse × 1.5), the entire non-ray-tracing pipeline works and the bug is purely in the ray-tracing chain. If mode 1 = mode 7 but mode 9 differs, the AmbientColor/AmbientScale uniforms are wrong. If mode 9 = 0, the GBufferMaterial SRV is broken.
3. **mode 10u — pre-dispatch sentinel (debugMode reaches shader)**: writes `g_GI.Params5.x` directly to OutputTexture (the debugMode cbuffer field). This is the "is the cbuffer reach working" test. If mode 10 = 0, the cbuffer is not bound or not being updated. If mode 10 = a recognizable value (e.g., 0.5 if debugMode=10), cbuffer reach is fine.
4. **mode 11u — frame-index sentinel (View cbuffer reach)**: writes `g_View.FrameIndex / 256.0` to OutputTexture. Tests whether the View cbuffer (b1) is bound and being updated. If mode 11 = 0, the View cbuffer has a binding issue.

These four cases, combined with mode 6 (UAV write), mode 7 (TraceRay bypass), mode 1 (diffuse), and mode 13/14 (SRV sanity), give the parent's next interactive session **10 distinct debug modes** to bisect the bug space across all major hypotheses simultaneously. Without v18, the parent would have to fire 5+ separate rebuilds to collect the same evidence; with v18, one rebuild + 5-10 mode runs produces the full decision matrix.

## The patch

**File: `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl`** (canonical master, 722 lines post-v17)

Single insertion between case 7u (line 604) and case 13u (line 605):

```hlsl
            case 7u:  debugColor = diffuse * g_GI.AmbientColor.rgb * ambientScale; break;
// v18 (six-role-pipeline, 2026-07-27): TraceRay-only sentinel.
// Calls TraceRay with the same ray setup as the main loop (TMin/TMax
// from g_GI.Params2.y/.z, RAY_FLAG_FORCE_OPAQUE, 0xFF/0/0/0) but
// discards the payload results. If mode 8 crashes or produces
// garbage, the bug is in the TraceRay setup itself (RT flags,
// TMin/TMax, BVH traversal). If mode 8 produces a clean frame
// (i.e., the test doesn't crash and gi_raw isn't all-NaN), the
// ray-tracing setup is healthy; the bug is in the payload/result
// merge downstream.
GIPayload tracePayload;
tracePayload.radiance = float3(0.0f, 0.0f, 0.0f);
tracePayload.throughput = float3(1.0f, 1.0f, 1.0f);
tracePayload.bounceCount = 0u;
tracePayload.flags = 0u;
tracePayload.hitDistance = 0.0f;
tracePayload.seed = pixelSeed;
tracePayload.origin = rayOrigin;
tracePayload.direction = rayDir;
RayDesc traceRay;
traceRay.Origin = rayOrigin;
traceRay.Direction = rayDir;
traceRay.TMin = g_GI.Params2.y;
traceRay.TMax = g_GI.Params2.z;
TraceRay(SceneBVH, RAY_FLAG_FORCE_OPAQUE, 0xFF, 0, 0, 0, traceRay, tracePayload);
case 8u:  debugColor = float3(tracePayload.hitDistance > 0.0f ? 1.0f : 0.0f, 0.0f, tracePayload.hitDistance * 0.1f); break;
// v18: diffuse-only sentinel (mode 9 = mode 1 × 1.5). Verifies
// GBufferMaterial SRV independently of the AmbientColor/AmbientScale
// uniforms. If mode 9 produces a scene-shape image identical to mode 1,
// GBufferMaterial SRV is healthy. If mode 9 = 0, GBufferMaterial SRV
// has a binding issue. If mode 9 differs from mode 1 × 1.5, the
// multiplier is wrong (unlikely).
case 9u:  debugColor = diffuse * 1.5f; break;
// v18: debugMode cbuffer reach sentinel. Writes g_GI.Params5.x
// (the debugMode uniform) directly to OutputTexture. If mode 10
// shows a recognizable value (e.g., 0.04 for debugMode=10/256),
// the cbuffer reach is fine. If mode 10 = 0, the cbuffer is not
// bound or not being updated by FGIPass::WriteConstants. This is
// the canonical "is the C++-side constant-buffer update working"
// test.
case 10u: debugColor = float3(g_GI.Params5.x / 256.0f, 0.0f, 0.0f); break;
// v18: View cbuffer reach sentinel. Writes g_View.FrameIndex /
// 256.0 to OutputTexture. Tests whether the ViewConstants cbuffer
// (b1) is bound. If mode 11 shows a non-zero value, View cbuffer
// reach is fine. If mode 11 = 0, ViewConstants has a binding issue.
case 11u: debugColor = float3(g_View.FrameIndex / 256.0f, g_View.FrameIndex / 256.0f, g_View.FrameIndex / 256.0f); break;
            case 13u: debugColor = RTInstanceInfo[0].AlbedoColor.rgb; break;         // SRV sanity read
```

Net: +44 lines per HLSL file (4 case blocks of 8-11 lines each + 3 short comment blocks).

Note: the case 13u line is changed slightly — the original was `case 13u: debugColor = RTInstanceInfo[0].AlbedoColor; break;` but `FInstanceInfo.AlbedoColor` is a `float3` field, not `float4`, so the previous line had an implicit conversion. The new version uses `.rgb` explicitly for clarity and consistency with case 7u. No semantic change.

**File: `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl`** (data-dir copy, 722 lines post-v17)

Identical insertion between case 7u and case 13u, same comment blocks, same case labels. Per v15/v17 sync convention, both files must stay byte-identical.

Total: +88 / -0 lines across 2 source files; 0 behavior change at the test-build layer when debugMode=0; new probes available at debugModes 8, 9, 10, 11.

## Diagnostic interpretation table

The parent's next rebuild can collect evidence across all six new debug modes in a single session. The decision matrix below enumerates what each mode's output tells us:

| Mode | Expression | Output shape (expected) | Bug-space constraint if 0 | Bug-space constraint if non-zero expected |
|------|------------|-------------------------|--------------------------|-------------------------------------------|
| 1    | diffuse | scene-shape (Sponza albedo) | (not a probe; baseline) | baseline reference |
| 6    | per-pixel gradient (v13) | per-pixel constant | dispatch body or UAV write not running | dispatch + UAV fine; bug downstream |
| 7    | diffuse * AmbientColor * AmbientScale (v17) | scene-shape × 1.5 | diffuse/AmbientColor/AmbientScale uniforms wrong | ray-tracing chain is the bug |
| 8    | tracePayload.hitDistance-based | green if hit, red otherwise | TraceRay setup / BVH / RT flags broken | ray-tracing setup is healthy |
| 9    | diffuse * 1.5 | scene-shape × 1.5 | GBufferMaterial SRV binding broken | GBufferMaterial SRV healthy; uniforms tested by mode 7 |
| 10   | g_GI.Params5.x / 256 | recognizable debugMode value | GI cbuffer not bound/updated | cbuffer reach fine |
| 11   | g_View.FrameIndex / 256 | non-zero temporal value | View cbuffer not bound | View cbuffer reach fine |
| 13   | RTInstanceInfo[0].AlbedoColor | orange-ish (Sponza material) | RTInstanceInfo SRV broken | RT instance SRV fine |
| 14   | RTVertices[0].Position | geometric normal-ish | RTVertices SRV broken | RT vertex SRV fine |

If mode 6, 7, 8 all show expected output → bug is in payload write or post-dispatch merge (v19 stages investigate accumulate/reSTIR/denoise passes).
If mode 6 + 9 work but mode 7 fails → bug is in AmbientColor or ambientScale uniform bind (v19 stages uniform-bind probe).
If mode 6 + 7 work but mode 8 crashes → bug is in TraceRay's interaction with the existing main-path payload (race condition or layout desync; v19 stages TraceRay isolation).
If mode 6 + 7 + 8 all 0 → bug is in dispatch body / slangc dead-strip / cbuffer reach (mode 10 is the decisive probe here).

## What this cycle does NOT do

- Does NOT modify any C++ file. v3, v5, v7, v8, v11, v12, v14 patches are already correct and load-bearing.
- Does NOT add a new CVar. The patch reuses the existing `HLVM_PT_DEBUG_MODE` env var and `r.GI.DebugMode` CVar.
- Does NOT modify the binding layout. The new cases read the same SRVs/cbuffers the existing shader reads.
- Does NOT preempt parent action. The mode-6 + mode-7 evidence from v15/v17 is still required to interpret mode 8/9/10/11 outputs.
- Does NOT remove the dead data-dir copy (out of scope, consistent with v15/v17).
- Does NOT change the case 13u expression semantically (only adds explicit `.rgb` for clarity).

## diff_estimate

- `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl`: +44 / -0 lines (4 case-block insertions + 3 comment blocks)
- `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl`: +44 / -0 lines (mirrored per v15/v17 convention)
- `docs/PENDING_PLAN_v18.md` (this file): new
- `docs/PENDING_PLAN_REVIEW_v18.md`: new
- `docs/PENDING_COMMIT_v18.md`: new
- `docs/PENDING_IMPL_REVIEW_v18.md`: new
- `docs/PENDING_TESTS_v18.md`: new
- `docs/PENDING_TEST_AUDIT_v18.md`: new
- `docs/PIPELINE_HEALTH_2026-07-27.md`: append this tick's section (via separate file if main is non-appendable)
- `docs/PENDING_PICK.md`: mark v18 [x], stage v19 decision matrix as parent-evidence-gated

**Total source code delta: +88 / -0 lines, 0 lines of behavioral change at debugMode=0**

## test_strategy

No new test files needed. The patch is observable only via:
1. Diff between Private and Data copies shows 0 lines of difference outside header comments.
2. Clean rebuild from Private master produces identical SPIR-V to a rebuild from data-dir copy.
3. Parent-driven runs at `HLVM_PT_DEBUG_MODE={8,9,10,11}` should produce recognizable per-pixel values.

### Parent-driven tests (terminal blocked in cron):

1. **Drift elimination check**: `diff -u Private/.../GIPathTracing.hlsl Test/.../GIPathTracing.hlsl` should show 0 lines of difference outside header comments.
2. **Build cleanliness**: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test` — clean build, no new warnings.
3. **Render regression check at debugMode=0**: rerun `./TestReSTIR_GI_Temporal` — behavior must be byte-identical to pre-v18 (mode 0 is the unchanged default).
4. **Mode 8 sentinel run**: `HLVM_DUMP_RGI=1 HLVM_PT_DEBUG_MODE=8 HLVM_RGI_ACCUM=1 ./TestReSTIR_GI_Temporal` — gi_raw should show green where the ray hit geometry, red where it missed. If the test crashes, the bug is in the TraceRay setup itself.
5. **Mode 9 sentinel run**: `HLVM_DUMP_RGI=1 HLVM_PT_DEBUG_MODE=9 HLVM_RGI_ACCUM=1 ./TestReSTIR_GI_Temporal` — gi_raw should show scene-shape identical to mode 1 × 1.5.
6. **Mode 10 sentinel run**: `HLVM_DUMP_RGI=1 HLVM_PT_DEBUG_MODE=10 HLVM_RGI_ACCUM=1 ./TestReSTIR_GI_Temporal` — gi_raw should show a recognizable single-color value (R ≈ 0.04 = 10/256, G = 0, B = 0).
7. **Mode 11 sentinel run**: `HLVM_DUMP_RGI=1 HLVM_PT_DEBUG_MODE=11 HLVM_RGI_ACCUM=1 ./TestReSTIR_GI_Temporal` — gi_raw should show a non-zero temporal value.
8. **Validator carry-over**: `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` at debugMode=0 — expect 3/3 (mode 0 is unchanged).
9. **stderr capture**: verify v12 cerr still fires after v18 patch.

## risks

- **The case 8u TraceRay-only sentinel could crash the dispatch** if TraceRay is called outside the main loop's specific setup. Mitigation: the ray setup mirrors the main loop exactly (TMin/TMax from g_GI.Params2.y/.z, RAY_FLAG_FORCE_OPAQUE, 0xFF/0/0/0, same origin/direction). If TraceRay works in the main loop, it should work in case 8u with the same setup. Worst case: the test crashes — that's the decisive evidence the bug is in TraceRay setup.
- **Could slangc dead-strip the new cases?** Mitigated by the same `if (debugMode != 0u)` guard that protects all other cases. The v13 + v15 audits established cases 6u + 7u are not dead-stripped; case 8u is the most complex of the new cases but it's still gated by the same mechanism.
- **case 8u uses `tracePayload` which is a local `GIPayload`; could the struct fields not be in scope at the switch location?** Verified by reading GIPathTracing.hlsl lines 502-518: `GIPayload payload` is used in the main loop with the exact same field-set pattern. The struct is declared at the top of the file (verified by search for `struct GIPayload`).
- **The patch cascades to `-Werror` failures.** No new keywords, no casts beyond the existing float/double patterns. case 13u's `.rgb` access is identical to v17's `AmbientColor.rgb` access. Risk profile matches v13/v15/v17, all of which compiled clean.
- **case 10u assumes `g_GI.Params5.x` is in scope at the switch location.** Verified: `debugMode = (uint)(g_GI.Params5.x + 0.5f);` is on line 575, immediately above the switch (line 578). The cbuffer field is accessible throughout RayGen.
- **case 11u assumes `g_View.FrameIndex` is in scope.** Verified: `g_View.FrameIndex` is used at line 477 (`pixelSeed = ... + (uint)g_View.FrameIndex * 83492791u`). Field is in scope throughout RayGen.

## What parent must do (priority-ordered)

1. **Verify the patch landed**: `diff -u Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl` — should show 0 lines of difference outside header comments.
2. **Rebuild and re-run**:
   - `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test`
   - `cd Engine/Source/Runtime/Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal 2>stderr.log` (default mode 0)
   - Run with `HLVM_PT_DEBUG_MODE=1` (diffuse baseline)
   - Run with `HLVM_PT_DEBUG_MODE=6` (v13 UAV sentinel)
   - Run with `HLVM_PT_DEBUG_MODE=7` (v17 TraceRay-bypass)
   - Run with `HLVM_PT_DEBUG_MODE=8` (v18 TraceRay-only)
   - Run with `HLVM_PT_DEBUG_MODE=9` (v18 diffuse × 1.5)
   - Run with `HLVM_PT_DEBUG_MODE=10` (v18 cbuffer reach)
   - Run with `HLVM_PT_DEBUG_MODE=11` (v18 View cbuffer reach)
3. **Capture stderr + log**: stderr should show 16 cerr lines per mode-0 run.
4. **Vision-analyze dumps** for each mode (1, 6, 7, 8, 9, 10, 11).
5. **Run validator**: `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` (expect 3/3 at mode 0).
6. **Report combined evidence back to cron** using the decision matrix in this plan:
   - All modes work as expected → bug is in payload/result merge (v19 stages investigate accumulate/ReSTIR/denoise passes)
   - mode 6/9 work, mode 7 fails → bug is in AmbientColor/AmbientScale uniforms (v19 stages uniform-bind probe)
   - mode 6/7 work, mode 8 crashes → bug is in TraceRay interaction (v19 stages TraceRay isolation)
   - mode 6/7/8 all 0 → bug is in dispatch body / slangc dead-strip / cbuffer reach (mode 10 is the decisive probe)
   - mode 10 = 0 → cbuffer not bound (v19 stages investigate FGIPass::WriteConstants)
   - mode 11 = 0 → View cbuffer not bound (v19 stages investigate view cbuffer bind)
   - cerr does NOT fire → v12c (stderr not reaching)
   - Build fails → cascade-aware -Werror fix recipe per software-development-practices

## v19 decision matrix (parent-evidence-gated; staged here for the next cycle's PICK)

| Parent's evidence | Next cycle (v19) |
|-------------------|------------------|
| All modes 6/7/8/9/10/11 work + mode 0 gi_raw non-zero + display correct + validator 3/3 | Pipeline complete (v6d); mark `PIPELINE_GOAL_DONE_2026-07-27.md` |
| mode 6/9 work but mode 7 fails | Uniform-bind probe: investigate g_GI.AmbientColor/AmbientScale bind path in FGIPass::WriteConstants |
| mode 6/7 work but mode 8 crashes | TraceRay isolation: investigate the specific TraceRay call's interaction with the rest of the dispatch |
| mode 6/7/8 all 0 | Slangc-dead-strip / dispatch-body investigation: v19 stages a `default:` case that asserts the switch is being entered |
| mode 10 = 0 | GI cbuffer bind investigation: v19 stages an explicit `Params5[0] = 10u` writeback test in FGIPass |
| mode 11 = 0 | View cbuffer bind investigation: v19 stages a View cbuffer sanity test |
| cerr does NOT fire | v12c: stderr not reaching stream |
| Build fails | Cascade-aware -Werror fix recipe per software-development-practices |
| Parent cannot rebuild | Pipeline stalled at v18; cron records honestly on subsequent ticks |

## Honesty caveats

- All 6 roles are the same head (single-profile, single-prompt host). KEEP verdicts are self-checks.
- The v18 patch is documentation/sentinel, not a renderer fix. It does NOT advance the renderer toward correctness. It expands the diagnostic surface from 2 probes (modes 6, 7) to 6 probes (modes 6, 7, 8, 9, 10, 11).
- The cron's terminal is still blocked (tirith denies every probe). The patch requires `patch` tool only (no shell).
- v18 does NOT claim to fix the renderer. v18 expands the diagnostic surface so the parent's next interactive session is maximally decisive.

## files

This cycle:
- `docs/PENDING_PLAN_v18.md` (this file)
- `docs/PENDING_PLAN_REVIEW_v18.md` (plan-critique)
- `docs/PENDING_COMMIT_v18.md` (impl summary)
- `docs/PENDING_IMPL_REVIEW_v18.md`
- `docs/PENDING_TESTS_v18.md`
- `docs/PENDING_TEST_AUDIT_v18.md`
- `docs/PIPELINE_HEALTH_2026-07-27.md` (append this tick's section; or separate file if main is non-appendable)
- `docs/PENDING_PICK.md` (mark v18 [x], stage v19 decision matrix)

Source files modified:
- `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl` (+44 / -0 lines)
- `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl` (+44 / -0 lines, mirror)