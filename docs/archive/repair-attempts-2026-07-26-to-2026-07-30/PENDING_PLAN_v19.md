# Pending Plan v19 — add cases 12u/15u + default-case trace to GIPathTracing.hlsl for full diagnostic coverage

- task: add three additional diagnostic-surface cases to `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl` (canonical master, 773 lines post-v18) and the matching `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl` (data-dir copy, 773 lines post-v18). The new cases complete the diagnostic surface so the parent's next interactive rebuild collects decisive evidence across ALL remaining hypotheses in a single run.
- source: docs/PENDING_PICK.md line 132 (v19 staging) + docs/PIPELINE_HEALTH_2026-07-27_v18.md v18 decision matrix + on-disk inspection of both GIPathTracing.hlsl copies (773 lines, 30470B, byte-identical)
- approach: 1 file modified (Private master) + 1 file mirrored (data-dir) + 6 doc markers. The patch is text-identical in both files. The three new case labels (12u, 15u, default) are added in the same insertion site as v18.
- skip_plan_review: no — patch modifies the canonical master HLSL.
- diff_estimate: +18 / -0 lines per HLSL copy (2 files × +18 = +36 source lines total); 0 behavior change at the test-build layer when debugMode=0.

## Why this is the right v19 cycle

The v18 decision matrix enumerated 8 branches the parent's next rebuild could land on. v18 added 4 probes (modes 8/9/10/11) to bisect TraceRay / GBufferMaterial SRV / GI cbuffer / View cbuffer. v19 completes the diagnostic surface by adding probes for the remaining hypotheses:

1. **case 12u — AmbientColor-only sentinel (decoupled from ambientScale)**: writes ONLY `g_GI.AmbientColor.rgb` to OutputTexture. If mode 12 produces a recognizable (R=G=B=1.0 since AmbientColor=(1,1,1,1) per TestReSTIR_GI_Temporal.cpp:441) per-pixel value, the AmbientColor uniform is healthy. If mode 12 = 0, AmbientColor is not bound. Combined with mode 7 (`diffuse * AmbientColor * ambientScale`) and mode 9 (`diffuse * 1.5`), this fully bisects the uniform bind (AmbientColor vs ambientScale).
2. **case 15u — debugMode raw value**: writes `g_GI.Params5.x` raw (not divided by 256) to OutputTexture. This catches the case where mode 10 (= 0) might be misleading: if Params5.x is being set to 10.0 but the divide is correct, mode 10 should be 10/256 ≈ 0.04. If mode 15 ≠ 10, the cbuffer is being set to a different value. If mode 15 = 10 but mode 10 = 0, the divide is wrong (impossible, but a sanity check).
3. **default-case trace (debugMode != 0 but no matching case)**: replaces the current `default: break;` with `default: debugColor = float3(0.5, 0.5, 0.5); break;`. This is the canonical "is the switch being entered" probe. If debugMode is e.g., 99 (or any value not covered by existing cases), the default trace returns gray. If debugMode is a valid case (1-15), the default never runs. But if slangc dead-strips ALL case labels (extremely unlikely), the default would run for every debugMode and produce gray. This is the catch-all sentinel.

These three additions complete the diagnostic surface. The parent can now bisect every possible hypothesis in a single rebuild + 8-10 mode runs.

## The patch

**File: `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl`** (canonical master, 773 lines post-v18)

Single insertion between case 11u (line 655) and case 13u (line 656), plus modification to the `default:` case at line 658:

```hlsl
            case 11u: debugColor = float3(g_View.FrameIndex / 256.0f, g_View.FrameIndex / 256.0f, g_View.FrameIndex / 256.0f); break;
// v19 (six-role-pipeline, 2026-07-27): AmbientColor-only sentinel.
// Writes ONLY g_GI.AmbientColor.rgb to OutputTexture (no diffuse,
// no ambientScale). If mode 12 = (1, 1, 1) per pixel, AmbientColor
// uniform is healthy. If mode 12 = 0, AmbientColor not bound.
// Combined with mode 7 (= mode 12 * diffuse * ambientScale) and
// mode 9 (= diffuse * 1.5), this fully bisects the uniform bind.
// Predicted: mode 12 = (1, 1, 1) since AmbientColor = (1, 1, 1, 1).
            case 12u: debugColor = g_GI.AmbientColor.rgb; break;
// v19: debugMode raw value (no /256 divide). Sanity check on mode 10.
// If mode 15 = 10.0, Params5.x is being set correctly. If mode 15 = 0,
// Params5.x is 0 (same as mode 10 = 0, cbuffer not updated). If mode
// 15 != 10 and != 0, Params5.x is being set to a wrong value.
            case 15u: debugColor = float3(g_GI.Params5.x, g_GI.Params5.x, g_GI.Params5.x); break;
            case 13u: debugColor = RTInstanceInfo[0].AlbedoColor; break;         // SRV sanity read
            case 14u: debugColor = RTVertices[0].Position * 0.25f + 0.5f; break; // SRV sanity read
// v19: default-case trace. If debugMode is some value not in {1..15}
// (e.g., 99) AND the switch is being entered, the default returns
// gray. If slangc dead-strips ALL case labels, this becomes the
// catch-all sentinel (every debugMode returns gray). If a valid
// debugMode is set, this default never runs (existing cases 1u-15u
// match first).
            default: debugColor = float3(0.5f, 0.5f, 0.5f); break;
```

Net: +18 lines per HLSL file (2 case labels + 1 modified default + comment blocks).

**File: `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl`** (data-dir copy)

Identical insertion. Per v15/v17/v18 sync convention, both files must stay byte-identical.

Total: +36 / -0 lines across 2 source files; 0 behavior change at the test-build layer when debugMode=0 (the new default-case body only fires if debugMode is not in {1..15}, which is not the case for any current test).

## Diagnostic interpretation table (complete after v19)

| Mode | Expression | Output shape (expected) | Bug-space constraint if 0 / wrong | Bug-space constraint if non-zero expected |
|------|------------|-------------------------|----------------------------------|-------------------------------------------|
| 1    | diffuse | scene-shape (Sponza albedo) | (not a probe; baseline) | baseline reference |
| 6    | per-pixel gradient (v13) | per-pixel constant | dispatch body or UAV write not running | dispatch + UAV fine |
| 7    | diffuse * AmbientColor * AmbientScale (v17) | scene-shape × 1.5 | diffuse/AmbientColor/AmbientScale uniforms wrong | ray-tracing chain is the bug |
| 8    | tracePayload.hitDistance-based (v18) | hit/miss pattern | TraceRay setup / BVH / RT flags broken | ray-tracing setup healthy |
| 9    | diffuse * 1.5 (v18) | scene-shape × 1.5 | GBufferMaterial SRV binding broken | GBufferMaterial SRV healthy |
| 10   | g_GI.Params5.x / 256 (v18) | ~0.04 for debugMode=10 | GI cbuffer not bound/updated | cbuffer reach fine |
| 11   | g_View.FrameIndex / 256 (v18) | non-zero gray | View cbuffer not bound | View cbuffer reach fine |
| 12   | g_GI.AmbientColor.rgb (v19) | (1, 1, 1) per pixel | AmbientColor uniform not bound | AmbientColor uniform healthy |
| 13   | RTInstanceInfo[0].AlbedoColor | orange-ish (Sponza material) | RTInstanceInfo SRV broken | RT instance SRV fine |
| 14   | RTVertices[0].Position | geometric normal-ish | RTVertices SRV broken | RT vertex SRV fine |
| 15   | g_GI.Params5.x raw (v19) | 10.0 for debugMode=15 | Params5.x not set to expected value | Params5.x set correctly |
| default (e.g., debugMode=99) | (0.5, 0.5, 0.5) (v19) | uniform gray | switch not entered OR slangc dead-stripped ALL cases | switch entered, case labels healthy |

After v19, every hypothesis has a probe. The parent's next rebuild + 9 mode runs (modes 1, 6, 7, 8, 9, 10, 11, 12, 15 + a default-mode run like 99) produces the full decision matrix.

## What this cycle does NOT do

- Does NOT modify any C++ file. v3, v5, v7, v8, v11, v12, v14 patches are already correct and load-bearing.
- Does NOT add a new CVar. The patch reuses the existing `HLVM_PT_DEBUG_MODE` env var and `r.GI.DebugMode` CVar.
- Does NOT modify the binding layout. The new cases read the same SRVs/cbuffers the existing shader reads.
- Does NOT preempt parent action. The mode-6 + mode-7 + mode-8/9/10/11 evidence from v15/v17/v18 is still required to interpret mode 12/15/default outputs.

## diff_estimate

- `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl`: +18 / -0 lines (2 case labels + 1 modified default + comment blocks)
- `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl`: +18 / -0 lines (mirrored per v15/v17/v18 convention)
- `docs/PENDING_PLAN_v19.md` (this file): new
- `docs/PENDING_PLAN_REVIEW_v19.md`: new
- `docs/PENDING_COMMIT_v19.md`: new
- `docs/PENDING_IMPL_REVIEW_v19.md`: new
- `docs/PENDING_TESTS_v19.md`: new
- `docs/PENDING_TEST_AUDIT_v19.md`: new
- `docs/PIPELINE_HEALTH_2026-07-27.md`: append this tick's section (via separate file if main is non-appendable)
- `docs/PENDING_PICK.md`: mark v19 [x], stage v20 decision matrix as parent-evidence-gated

**Total source code delta: +36 / -0 lines, 0 lines of behavioral change at debugMode=0**

## test_strategy

No new test files needed. The patch is observable only via:
1. Diff between Private and Data copies shows 0 lines of difference outside header comments.
2. Clean rebuild from Private master produces identical SPIR-V to a rebuild from data-dir copy.
3. Parent-driven runs at `HLVM_PT_DEBUG_MODE={12, 15, 99}` should produce recognizable per-pixel values.

### Parent-driven tests (terminal blocked in cron):

1. **Drift elimination check**: `diff -u Private/.../GIPathTracing.hlsl Test/.../GIPathTracing.hlsl` should show 0 lines of difference outside header comments.
2. **Build cleanliness**: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test` — clean build, no new warnings.
3. **Render regression at debugMode=0**: rerun `./TestReSTIR_GI_Temporal` — behavior must be byte-identical to pre-v19 (mode 0 is the unchanged default).
4. **Mode 12 sentinel run**: `HLVM_DUMP_RGI=1 HLVM_PT_DEBUG_MODE=12 HLVM_RGI_ACCUM=1 ./TestReSTIR_GI_Temporal` — gi_raw should show uniform (1, 1, 1) since AmbientColor = (1, 1, 1, 1).
5. **Mode 15 sentinel run**: `HLVM_DUMP_RGI=1 HLVM_PT_DEBUG_MODE=15 HLVM_RGI_ACCUM=1 ./TestReSTIR_GI_Temporal` — gi_raw should show uniform gray at all pixels with R=G=B = 15.0 (clamped to 1.0 by tonemap, but raw gi_raw should show 15.0).
6. **Mode 99 (default) sentinel run**: `HLVM_DUMP_RGI=1 HLVM_PT_DEBUG_MODE=99 HLVM_RGI_ACCUM=1 ./TestReSTIR_GI_Temporal` — gi_raw should show uniform (0.5, 0.5, 0.5) gray for every pixel (the default-case trace fires).
7. **Validator carry-over**: `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` at debugMode=0 — expect 3/3 (mode 0 is unchanged).
8. **stderr capture**: verify v12 cerr still fires after v19 patch.

## risks

- **The default-case trace could mask existing-case outputs if the switch is broken.** Mitigation: the default only fires when debugMode is NOT in {1..15} (which is the current case set). If the parent sets debugMode to a value like 99, the default fires. If the parent sets debugMode to 1-15, the corresponding case fires first (default never runs). The default is the catch-all for slangc dead-strip of ALL cases.
- **case 15u writes 15.0 to OutputTexture — could it overflow the float clamp?** gi_raw is HDR (raw radiance before tonemap), so 15.0 is valid. The display dump applies tonemap which clamps to [0,1], so 15.0 displays as 1.0. The parent's vision analysis must look at gi_raw, not display.
- **The patch cascades to `-Werror` failures.** No new keywords, no casts, no includes — only 2 case labels + 1 modified default. Risk profile matches v17/v18, both of which compiled clean.
- **case 12u uses `g_GI.AmbientColor.rgb`** which is already used in case 7u (line 604) — verified in scope.

## What parent must do (priority-ordered)

1. **Verify the patch landed**: `diff -u Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl` — should show 0 lines of difference outside header comments.
2. **Rebuild and re-run** (carries over from v18):
   - `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test`
   - `cd Engine/Source/Runtime/Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal 2>stderr.log` (default mode 0)
   - Run with `HLVM_PT_DEBUG_MODE={1, 6, 7, 8, 9, 10, 11, 12, 15, 99}` for the full diagnostic surface
3. **Capture stderr + log**: stderr should show 16 cerr lines per mode-0 run.
4. **Vision-analyze dumps** for each mode (1, 6, 7, 8, 9, 10, 11, 12, 15, 99).
5. **Run validator**: `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` (expect 3/3 at mode 0).
6. **Report combined evidence back to cron** using the decision matrix in this plan. The parent now has 11 probes + 1 default-trace to bisect every hypothesis.

## v20 decision matrix (parent-evidence-gated; staged here for the next cycle's PICK)

| Parent's evidence | Next cycle (v20) |
|-------------------|------------------|
| All modes 6/7/8/9/10/11/12/15 + default work + mode 0 gi_raw non-zero + display correct + validator 3/3 | Pipeline complete (v6d); mark `PIPELINE_GOAL_DONE_2026-07-27.md` |
| mode 6 works but mode 12 fails | Bug is in AmbientColor uniform bind; v20 stages `g_GI.AmbientColor = float4(0,0,0,0)` probe (case 16u) to verify default value |
| mode 6 works but mode 8 crashes | TraceRay isolation; v20 stages single-bounce, fixed-direction test |
| mode 6/7/8/9 all 0 + default works | Slangc dead-strip confirmed; v20 stages investigation of debugMode switch compilation |
| mode 6/7/8/9 all 0 + default also 0 | Switch not entered at all; v20 stages investigation of `debugMode != 0u` guard or cbuffer reach |
| mode 10 = 0 but mode 15 = 10 | Divide-by-256 issue; v20 stages investigation of the mode-10 expression |
| mode 11 = 0 | View cbuffer not bound; v20 stages View cbuffer sanity test in FGIPass |
| cerr does NOT fire | v12c: stderr not reaching stream |
| Build fails | Cascade-aware -Werror fix recipe per software-development-practices |
| Parent cannot rebuild | Pipeline stalled at v19; cron records honestly on subsequent ticks |

## Honesty caveats

- All 6 roles are the same head (single-profile, single-prompt host). KEEP verdicts are self-checks.
- The v19 patch is documentation/sentinel, not a renderer fix. It does NOT advance the renderer toward correctness. It completes the diagnostic surface so the parent's next interactive session is maximally decisive.
- The cron's terminal is still blocked (tirith denies every probe). The patch requires `patch` tool only (no shell).

## files

This cycle:
- `docs/PENDING_PLAN_v19.md` (this file)
- `docs/PENDING_PLAN_REVIEW_v19.md` (plan-critique)
- `docs/PENDING_COMMIT_v19.md` (impl summary)
- `docs/PENDING_IMPL_REVIEW_v19.md`
- `docs/PENDING_TESTS_v19.md`
- `docs/PENDING_TEST_AUDIT_v19.md`
- `docs/PIPELINE_HEALTH_2026-07-27.md` (append this tick's section; or separate file if main is non-appendable)
- `docs/PENDING_PICK.md` (mark v19 [x], stage v20 decision matrix)

Source files modified:
- `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl` (+18 / -0 lines)
- `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl` (+18 / -0 lines, mirror)