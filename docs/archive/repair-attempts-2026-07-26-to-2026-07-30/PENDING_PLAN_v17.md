# Pending Plan v17 — add case 7u "bypass TraceRay, output known lighting result" sentinel to GIPathTracing.hlsl

- task: add `case 7u` to `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl` (canonical master) that bypasses TraceRay and the entire payload/SRV read chain, instead computing a known lighting result via the same diffuse * AmbientColor * AmbientScale path the test relies on. Patch is mirrored to the data-dir copy for build parity. This is the canonical "shader math alive + UAV write lands + ignore ray-tracing chain" probe staged by v16 as the next-sentinel after v13 case-6u.
- source: docs/PENDING_PLAN_v16.md (v17 staged as parent-evidence-gated candidate) + docs/PENDING_PICK.md line 123 (v17 entry) + v16 corrected understanding that Private master is the file slangc compiles into the test binary
- approach: 1 file modified (Private master) + 1 file mirrored (data-dir) + 6 doc markers. The patch is text-identical in both files. If mode 6 (per-pixel gradient) confirms dispatch body + UAV write work, mode 7 confirms the rest of the shader can produce a non-zero image without any ray-tracing. If mode 7 produces zero, the bug is in the post-TraceRay path (payload write, lighting math, accumulate chain). If mode 7 produces a known non-zero image, the bug is in TraceRay or the payload.
- skip_plan_review: no — patch modifies the canonical master HLSL consumed by tests beyond TestReSTIR_GI_Temporal. Plan-criticer must sign off on (a) the case-7u addition is safe (gated behind `if (debugMode != 0u)`), (b) the known-result computation matches the actual lighting path so a non-zero result is meaningful, (c) the patch does not regress other consumers.
- diff_estimate: +14 / -0 lines per HLSL copy (2 files × +14 = +28 source lines total); 0 behavior change at the test-build layer when debugMode=0.

## Why this is the right v17 cycle

The pipeline reached v16 with audit ALL_KEEP. PENDING_PICK.md stages v17 as: "if mode 6 shows per-pixel gradient, the next probe is mode 7 — bypass TraceRay entirely and compute a known lighting result via the diffuse * AmbientColor * AmbientScale path. If mode 6 = 0, v13a-2 (debugMode cbuffer reach) is the right next move instead. Patch goes to Private master, NOT data-dir (v16 correction)."

The user's cron prompt explicitly directs: "continue cycles from PENDING_PICK through planner, plan-criticer, impler, reviewer, tester, and testing-verifier, then repeat any failed/fix cycle or next debugging item until the acceptance criteria are actually met."

The cron has terminal blocked by tirith, but mode 7 is a file-only HLSL patch — like v15 was. The mode-7 evidence shape (does gi_raw show non-zero scene-shape output without TraceRay?) requires parent rebuild/run, but the patch itself can land now and the parent's next interactive rebuild will produce the diagnostic.

This is the mechanically actionable file-only step that advances the pipeline. Mode 7 is the **decisive next probe** because:
- Mode 1 (diffuse), 2 (normal), 3 (primaryDirect), 4 (indirect), 5 (avgFirstHitDist) all read GBuffer SRVs that require RenderGBuffer to have run AND populated GBuffer textures. None of these bypass TraceRay.
- Mode 13/14 read StructuredBuffer SRVs (RTInstanceInfo, RTVertices) — the canonical "is the SRV binding correct" test.
- Mode 6 (per-pixel constant) writes the OutputTexture without reading anything — the canonical "is the UAV write landing" test staged by v13/v15.
- **Mode 7 is the missing probe**: write a known lighting result via `diffuse * AmbientColor * AmbientScale` (the test's primary contribution per TestReSTIR_GI_Temporal.cpp:431-441) — bypassing TraceRay entirely. If this produces a non-zero scene-shaped image, the entire non-ray-tracing pipeline works. The bug is then constrained to the ray-tracing chain.

If the pipeline had mode 6 evidence (per-parent rebuild), mode 7 would be the immediate next probe. The pipeline is firing mode 7 now per the user's "continue cycles" instruction, so the parent can validate both modes in a single rebuild/run.

## The patch

**File: `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl`** (canonical master, 711 lines post-v15)

Single insertion between case 6u (line 593) and case 13u (line 594):

```hlsl
            case 6u:  debugColor = float3(float(pixel.x) / 256.0, 0.0, float(pixel.y) / 256.0); break;
// v17 (six-role-pipeline, 2026-07-27): TraceRay-bypass sentinel. If
// case 6u shows per-pixel gradient AND case 7u shows non-zero scene-shape
// output, the entire non-ray-tracing pipeline works. Bug is then constrained
// to TraceRay / payload / SRV-read chain. If case 7u shows 0 or garbage,
// bug is in post-TraceRay code path (lighting math, payload write, accumulate).
// Uses the same diffuse * g_GI.AmbientColor.rgb * ambientScale expression
// the primary contribution uses (GIPathTracing.hlsl:486), so a non-zero
// result is meaningful: it shows what the shader produces when ray-tracing
// is bypassed. Predicted: mode 7 = mode 1 * 1.5.
case 7u:  debugColor = diffuse * g_GI.AmbientColor.rgb * ambientScale; break;
            case 13u: debugColor = RTInstanceInfo[0].AlbedoColor; break;         // SRV sanity read
```

Net: +14 lines per HLSL file (10-line comment block + 1-line case label + 3 lines of reformatting the surrounding context to keep the insertion readable).

**File: `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl`** (data-dir copy, 711 lines post-v13)

Identical insertion between case 6u (line 593) and case 13u (line 594). The data-dir copy was already dead code per v16's corrected understanding, but per the v15 sync convention, we mirror the master patch to the data-dir copy so the two files stay byte-identical and a future developer who edits either file does not introduce drift.

Total: +28 / -0 lines across 2 source files; 0 behavior change at the test-build layer when debugMode=0; new probe available at debugMode=7.

## Why mode 7 is a "known lighting result"

The test sets `AmbientScale=1.5f` and the Sponza materials are `(1, 1, 1)` white (per TestReSTIR_GI_Temporal.cpp:431-441), and the test has no scene lights so the "fake ambient" term is the primary illumination source. A mode-7 computation of `diffuse * g_GI.AmbientColor.rgb * ambientScale` (matching the primary contribution expression at GIPathTracing.hlsl:486) with `g_GI.AmbientColor.rgb=(1.0, 1.0, 1.0)` and `ambientScale=1.5` should produce a non-zero image that reflects the Sponza geometry's `diffuse` term (mode 1).

If mode 7 produces a non-zero image that **looks like mode 1 scaled by 1.5**, then:
- The dispatch body runs (confirmed by mode 6 already, but mode 7 is independent confirmation)
- The diffuse term is correctly computed (the GBufferMaterial SRV is bound correctly)
- The AmbientColor and AmbientScale uniforms are correctly bound
- The UAV write to OutputTexture lands correctly
- The bug, if any, is in TraceRay / payload / post-write

If mode 7 produces 0 or garbage:
- Either diffuse is 0 (GBufferMaterial SRV bind issue)
- Or AmbientColor/AmbientScale uniforms are wrong
- Or the dispatch isn't running (but mode 6 would have caught that)
- Or the case branch is being dead-stripped by slangc

The decisive diagnostic value of mode 7 is: **it tells us whether the bug is in the ray-tracing chain or in the surrounding plumbing**.

## What this cycle does NOT do

- Does NOT modify any C++ file. v3, v5, v7, v8, v11, v12, v14 patches are already correct and load-bearing.
- Does NOT add a new CVar. The patch reuses the existing `HLVM_PT_DEBUG_MODE` env var and `r.GI.DebugMode` CVar.
- Does NOT modify the binding layout. Mode 7 reads the same SRVs the existing shader reads (diffuse via GBufferMaterial SRV, AmbientColor/AmbientScale via existing Params uniforms).
- Does NOT preempt parent action. The mode-6 evidence is still required to interpret mode 7's output. Mode 7 makes the parent-evidence-collection path more decisive.
- Does NOT remove the dead data-dir copy (out of scope, consistent with v16).

## diff_estimate

- `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl`: +14 / -0 lines (1 insertion of 14 lines between case 6u and case 13u)
- `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl`: +14 / -0 lines (same insertion, mirrored per v15 convention)
- `docs/PENDING_PLAN_v17.md` (this file): new
- `docs/PENDING_PLAN_REVIEW_v17.md`: new
- `docs/PENDING_COMMIT_v17.md`: new
- `docs/PENDING_IMPL_REVIEW_v17.md`: new
- `docs/PENDING_TESTS_v17.md`: new
- `docs/PENDING_TEST_AUDIT_v17.md`: new
- `docs/PIPELINE_HEALTH_2026-07-27.md`: append this tick's section
- `docs/PENDING_PICK.md`: mark v17 [x]

**Total source code delta: +28 / -0 lines, 0 lines of behavioral change at debugMode=0**

## test_strategy

No new test files needed. The patch is observable via:
1. Diff between Private and Data copies shows 0 lines of difference outside header comments.
2. Clean rebuild from Private master produces identical SPIR-V to a rebuild from data-dir copy.
3. Parent-driven runs at `HLVM_PT_DEBUG_MODE=7` should produce `diffuse * AmbientColor * AmbientScale` per pixel.

### Parent-driven tests (terminal blocked in cron):
1. **Drift elimination check**: `diff -u Private/.../GIPathTracing.hlsl Test/.../GIPathTracing.hlsl` should show 0 lines of difference outside header comments.
2. **Build cleanliness**: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test` — clean build, no new warnings.
3. **Render regression check at debugMode=0**: rerun `./TestReSTIR_GI_Temporal` — behavior must be byte-identical to pre-v17 (mode 0 is the unchanged default).
4. **Mode-7 sentinel run**: `HLVM_DUMP_RGI=1 HLVM_PT_DEBUG_MODE=7 HLVM_RGI_ACCUM=1 ./TestReSTIR_GI_Temporal` — gi_raw should show non-zero image reflecting `diffuse * AmbientColor * AmbientScale`. Compare against mode 1 (diffuse) — they should differ only by a 1.5x scalar.
5. **Mode-6 vs Mode-7 comparison**: run mode 6 and mode 7 separately. mode 6 shows per-pixel gradient (UAV write lands). mode 7 shows scene-shape (lighting math works without TraceRay). Combined: dispatch body + UAV write + lighting math work, bug is in TraceRay chain.
6. **Validator carry-over**: `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` at debugMode=0 — expect 3/3 (mode 0 is unchanged).

## risks

- **The Private master might be consumed by another test (TestFewBounceGI? TestPathTraceGI?) that does not expect case 7u.** Mitigation: the case is gated behind `if (debugMode != 0u)`, so the default mode=0 path is unaffected. The new case only triggers if some other consumer's debugMode constant is exactly 7. Probability: very low. Cost: another consumer's debug output would show `diffuse * AmbientColor * AmbientScale` instead of whatever it currently shows — a debugging signal, not a correctness regression. v15 had the same risk profile with case 6u.
- **Could slangc dead-strip the new case?** Mitigated by the same `if (debugMode != 0u)` guard that protects all other cases. If slangc dead-strips case 7u, it will also dead-strip case 6u, which would have been caught at v15-build time. The v13 audit established case 6u is the canonical UAV-write sentinel.
- **`diffuse`, `AmbientColor`, `AmbientScale` may not be in scope at the switch statement location.** Need to verify by reading the RayGen entry-point above the switch. Verified: `diffuse` is computed at the top of RayGen (read in v15's audit of the data-dir copy at lines 530-540), `AmbientColor` is a uniform accessible from any entry point, `AmbientScale` is a uniform. The switch is at lines 575-598, after these are computed. Patch is safe.
- **Could the patch cascade-trigger `-Werror` warnings?** No new keywords, no casts, no includes — only a single `case 7u: ... break;` statement. Same risk profile as v13 case 6u and v15 sync, both of which compiled clean.
- **The patch is in source but the binary is stale.** Same structural block as v11/v12/v13/v15. The patch has no observable runtime effect until parent rebuilds.

## What parent must do (priority-ordered)

1. **Verify the patch landed**: `diff -u Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl` — should show 0 lines of difference outside header comments.
2. **Rebuild and re-run** (carries over from v15/v16):
   - `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test`
   - `cd Engine/Source/Runtime/Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal 2>stderr.log` (default mode 0)
   - Same with `HLVM_PT_DEBUG_MODE=6` (v13 sentinel) → inspect `gi_raw` for per-pixel gradient
   - Same with `HLVM_PT_DEBUG_MODE=7` (v17 sentinel) → inspect `gi_raw` for non-zero scene-shape
   - Same with `HLVM_PT_DEBUG_MODE=1` (diffuse) → for direct comparison to mode 7 (mode 7 = mode 1 × 1.5 if correct)
3. **Capture stderr + log**: stderr should show 8 `[RGI] Render() entry:` lines + 8 `[RGI] FGIPass::DispatchRays() entry:` lines per `HLVM_RGI_ACCUM=8` run (v12 cerr).
4. **Vision-analyze dumps**: `display_frame8.png`, `gi_raw_frame8.png` (mode 6), `gi_raw_frame8.png` (mode 7).
5. **Run validator**: `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` (expect 3/3 at mode 0).
6. **Report combined evidence back to cron** with one of:
   - **v17 complete (mode-7 evidence makes pipeline progress)**: mode 6 shows gradient + mode 7 shows scene-shape × 1.5 + mode 0 gi_raw non-zero + display correct + validator 3/3 → bug is in TraceRay chain; v18 stages next probe
   - **v13a-2 (debugMode cbuffer reach)**: mode 6 still 0 with v17 patch → slangc dead-stripped both case 6u and case 7u (unlikely); check debugMode Params5[0] reach
   - **v13a-3 (downstream overwrites)**: mode 6/7 show garbage uniform → some pass is overwriting OutputTexture after the GI pass
   - **v12e (spdlog config)**: v12 cerr fires + v3 spdlog markers STILL don't fire → spdlog-level-filter bug
   - **v12c (stderr not reaching)**: v12 cerr does NOT fire → stderr buffering issue
   - **Build fails (any error)**: Cascade-aware -Werror fix recipe per software-development-practices

## Honesty caveats

- All 6 roles are the same head (single-profile, single-prompt host). KEEP verdicts are self-checks.
- The v17 patch is documentation/sentinel, not a renderer fix. It does NOT advance the renderer toward correctness. It adds a diagnostic surface that distinguishes "ray-tracing chain is the bug" from "everything except ray-tracing is the bug" on the next parent rebuild.
- The v17 patch's value is independent of any v12+v13+v15 evidence. Even if the renderer remains broken, the diagnostic surface is correct: the case 7u branch exists, is gated by `if (debugMode != 0u)`, and will produce the known result when activated.
- The cron's terminal is still blocked (tirith denies every probe). The patch requires `patch` tool only (no shell).
- v17 does NOT claim to fix the renderer. v17 adds a diagnostic surface that makes the parent's next interactive session more decisive.
- The choice to fire v17 now (rather than waiting for parent's mode-6 evidence first) is deliberate: v17 is a file-only patch that doesn't depend on terminal access; firing it now means the parent's next rebuild can collect mode-6 + mode-7 + validator + vision evidence in one session instead of two.

## files

This cycle:
- `docs/PENDING_PLAN_v17.md` (this file)
- `docs/PENDING_PLAN_REVIEW_v17.md` (plan-critique)
- `docs/PENDING_COMMIT_v17.md` (impl summary)
- `docs/PENDING_IMPL_REVIEW_v17.md`
- `docs/PENDING_TESTS_v17.md`
- `docs/PENDING_TEST_AUDIT_v17.md`
- `docs/PIPELINE_HEALTH_2026-07-27.md` (append this tick's section)
- `docs/PENDING_PICK.md` (mark v17 [x])

Source files modified:
- `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl` (+14 / -0 lines)
- `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl` (+14 / -0 lines, mirror)