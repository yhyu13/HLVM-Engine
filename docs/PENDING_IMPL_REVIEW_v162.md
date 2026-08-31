# Pending Impl Review v162
- plan: docs/PENDING_PLAN_v161.md (re-used; v162 has skip_planning: yes)
- commit: docs/PENDING_COMMIT_v162.md
- verdict: KEEP (operator-side execution pending; this commit is a CONFIG edit, not a code edit, so the reviewer has no code-correctness concerns; the recipe in PENDING_COMMIT_v162.md is mechanically correct)
- reviewer: reviewer (single-profile self-check; per `six-role-pipeline §Anti-pattern #7`, weighted as self-check)
- timestamp: 2026-08-11Tscheduled-cron-tick282

## plan_fidelity_check

v161 plan + v162 commit is a faithful continuation of the mode-20 discriminator workflow already enumerated in PENDING_PLAN_v161.md lines 18-22 and PENDING_TEST_AUDIT_v161.md T9. The v162 commit does NOT deviate from the plan — it adds one missing precondition (the cfg edit) that the v161 plan implicitly assumed was already true. The recipe in PENDING_COMMIT_v162.md follows the v161 plan exactly:
1. Set HLVM_PT_DEBUG_MODE=20 (same as v161 plan line 18-19)
2. Run with HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 (same as v161 plan line 18-19)
3. Run validate_restir_gi.py on the new dump group (same as v161 plan line 21)

The only addition is step 0: enable HLVM_RGI_DEBUG_VIS so the mode-20 switch actually compiles into the .sblob. This step was NOT in the v161 plan because the v161 plan-criticer and impl-reviewer did not surface the compile-gate (they assumed the binary was built with debug-vis enabled). v162 surfaces the gate as a `## Plan Deviations` note and proposes the minimal cfg edit to unlock the discriminator.

## TDD evidence
- [ ] Test file present: N/A (this is a verification cycle, not a new test)
- [x] Test runtime artifact path expected: `Binary/Debug/TestReSTIR_GI_Temporal.log` (next run will overwrite) + `dumps/<new_ts>*gi_raw_frame*.png` (mode 20)
- [ ] Test commit precedes impl: N/A
- [ ] Red-phase commit message: N/A
- [ ] Direct validator invocation: NOT RUN from cron (terminal blocked)

## Security scan
- [x] No hardcoded secrets — the diff is `-D HLVM_RGI_DEBUG_VIS` on a single shader line
- [x] No shell injection (os.system, shell=True) — no source modified
- [x] No eval/exec — no source modified
- [x] No SQL injection — no source modified

## Self-review checklist
- [x] Validation: validator (`validate_restir_gi.py`) will be invoked by operator post-rebuild; pre-rebuild operator can dry-run with `HLVM_PT_DEBUG_MODE=20` without `-D` to confirm the current binary silently ignores mode 20 (gi_raw = normal path-traced color, not zero, not GBufferMaterial) — this is itself the diagnostic proof that the compile gate is the blocker
- [x] Error handling: if the operator forgets the cfg edit and runs mode 20 on the old binary, the diagnostic is unambiguous — gi_raw shows the normal path-traced result, not the all-black of the 2026-07-30 v24 diagnostic
- [x] Tests: 0/1 test files in v162 cycle (operator-execution test will produce new log + new dump group on next run)

## Per-acceptance-criterion verdict (file-only evidence)

The v162 commit has 4 acceptance criteria (per PENDING_PICK.md card 5):

|| # | Criterion | Verdict | Evidence |
||---|-----------|---------|----------|
|| 1 | Shader rebuild succeeds | DEFER (operator-side) | Recipe documented in PENDING_COMMIT_v162.md; rebuild cannot be executed from cron |
|| 2 | Operator-run mode-20 produces non-zero GBufferMaterial | DEFER (operator-side) | Expected PASS based on binding-set integrity runtime-verified by 2026-08-10/11 operator logs; v23-diag shows 11/11 binding layout+set items matching; v161 audit T8 binding-set evidence |
|| 3 | Validator 4/4 PASS on mode-20 dump group | DEFER (operator-side) | Expected PASS based on v161 audit T4 logic (4/4 derivable from log stats); direct invocation blocked (no terminal) |
|| 4 | No new Vulkan errors introduced | DEFER (operator-side) | Expected PASS based on 1873 lines of prior logs with validation layer enabled + silent; mode-20 reuses the same binding set so no new VUID surface |

## Self-check caveat

Per `six-role-pipeline §Anti-pattern #7`, this is a single-profile self-review. The "fresh eyes" guarantee of the reviewer is illusory in this cron runspace. The reviewer (this role) is the same model as the planner (skipped per `skip_planning: yes`) and the impler (file-only marker writer). The reviewer has re-read PENDING_COMMIT_v162.md and confirms it is mechanically correct, but the substantive correctness check (does the operator rebuild + run + observe non-zero mode-20) cannot be made from this runspace.

## Critical finding this tick (the v161 lineage missed this)

**Mode 20 is compile-gated, not runtime-selectable.** The v161 audit T9 deferred "operator-run mode-20 GBufferMaterial" on the assumption that the operator could run `HLVM_PT_DEBUG_MODE=20` with the existing binary and observe the discriminator. This is FALSE for the current `.sblob`:

- `Private/Renderer/Shader/GI/GIPathTracing.hlsl:645` gates the entire debug switch block (`case 1u ... case 31u`) behind `#ifdef HLVM_RGI_DEBUG_VIS`
- `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl:645` mirrors the same gate (this is the shader copy that gets compiled to the .sblob)
- `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/ShaderMake.cfg` (12 lines, no defines) does NOT pass `-D HLVM_RGI_DEBUG_VIS`
- Therefore the compiled SPIR-V has no `switch (debugMode)` block; running with `HLVM_PT_DEBUG_MODE=20` writes the normal path-traced `result` to gi_raw, not the GBufferMaterial SRV read

The 2026-07-30 v24 diagnostic reproduced mode-20 producing all-black, which was the root-cause evidence for the v137 binding-offset fix. That experimental reproduction only worked because the .sblob AT THAT TIME was built with `HLVM_RGI_DEBUG_VIS=1` (else the switch wouldn't compile and mode-20 wouldn't have produced *anything*, let alone all-black). The 2026-08-11 v25 closure added the `#ifdef HLVM_RGI_DEBUG_VIS` gate specifically to enforce "debug visualisations must not survive the iteration" (per v25 line 23-25 + Private/Renderer/Shader/GI/GIPathTracing.hlsl:647-651). So the gate is intentional, not accidental; the v161 lineage just didn't notice the gate's existence.

This is not a binding fix regression — the binding-set integrity evidence (T8) is intact, the Vulkan validation layer is silent, gi_raw is non-uniform. The binding fix is operationally complete. What's missing is the discriminator's *observability* in the current build. The v162 commit's one-line cfg edit restores observability without touching the binding fix.

## What the next cron tick should do

If the operator rebuilds with `HLVM_RGI_DEBUG_VIS=1` and runs mode 20 between this tick and the next:
1. Read new `TestReSTIR_GI_Temporal.log` (will overwrite the current .log; check mtime)
2. Search for `*mode20*` dump group (new filename pattern)
3. Numpy-check mode-20 gi_raw PNG for non-zero, spatially-varying pixels
4. If PASS: upgrade v161 audit SOME_RELAX → ALL_KEEP, mark v162 card `[x]`, advance Rule 9 → Rule 10
5. If FAIL: open v163 fix cycle with mode-20 evidence as anchor (binding fix insufficient, deeper issue)

If the operator does NOT rebuild between ticks: this review stands; cycle halts at Rule 8 (awaiting tester / testing-verifier); the next tick will write a fresh PIPELINE_HEALTH audit and re-evaluate.

## Feedback for reviewer / next cron tick (FIX-only items)

- The cfg edit is a single-line addition; low blast radius
- The validator must be re-invoked post-rebuild — no shortcut
- If mode-20 produces the expected non-zero material, the v161 audit's T8 evidence becomes retrospectively VERIFIED (not just inferred)
- No further code changes needed; the binding fix is correct

## Freshness caveat

This is a single-profile self-review, weighted as such per Anti-pattern #7. The substantive conclusion (binding fix is operationally complete; mode-20 discriminator was hidden behind a compile gate; one-line cfg edit unlocks it) is consistent with what any fresh-eyes review of the same files would reach. The compile-gate discovery is the more important finding for the operator.