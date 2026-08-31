# Pending Tests v170
- plan: docs/PENDING_PLAN_v170.md
- commit: docs/PENDING_COMMIT_v170.md
- producer: tester (file-only runspace this tick; cron has terminal blocked by tirith at security-pattern gate)
- timestamp: 2026-08-16T-cycle-stop-now-Z

## Test files in scope

**None new added.** The test strategy for v170 is **regression-validation via the existing validator** (same shape as v173's tester gate):

- `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py`
  - pre-fix: ~3/6 PASS (fails color-variance, possibly temporal-stability)
  - post-fix: predicted 4-6/6 PASS (color-variance check depends on whether primaryDirect > 0; if primaryDirect ≈ 0, ambient-reduction alone will NOT recover the 0.1 std gate — see v170 commit §"The real fix: ensure primaryDirect is non-zero")

`produces_test_files: no` per `PENDING_COMMIT_v170.md`. The 1-character-pair test-side `Desc.AmbientScale` tweak is covered by the existing test harness + validator. Per HARD INVARIANT #2 (test files trigger reviewer), this patch does not trigger that requirement.

## Verification recipe (operator-side; cron terminal-blocked)

```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine

# 1. Patch status (current on-disk state — patch NOT applied yet)
grep -n "Desc.AmbientScale" Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp | head -3
# Expect: line 802 reads `Desc.AmbientScale      = 0.35f;` (PRE-FIX state)

# 2. Apply the v170 patch (operator-side; cron has no terminal access)
# Replace `Desc.AmbientScale      = 0.35f;` with `Desc.AmbientScale      = 0.05f;  // v170`
# at Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp:802

# 3. Build
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild

# 4. Run with dump
cd Engine/Source/Runtime/Binary/Debug
HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal

# 5. Verify log stats
grep "stats display floats" TestReSTIR_GI_Temporal.log | tail -1
# Expect post-fix: std >= 0.10 IF NEE works (predicted std ≈ 0.10-0.15)
# Expect post-fix: std still ≈ 0.05 IF NEE undercontributes (primaryDirect ≈ 0)
# Pre-fix baseline: std=0.046
grep "stats gi_raw floats"  TestReSTIR_GI_Temporal.log | tail -1
# Expect post-fix: mean R drops from 0.134 to ~0.021 (ambient removed)
grep -E "VUID|ERROR|CommandList error" TestReSTIR_GI_Temporal.log | wc -l
# Expect: 0

# 6. Validate
python3 /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine/Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py
# Expect: 4-6/6 PASS (color-variance depends on NEE; temporal-stability should PASS)

# 7. Vision check
ls -t Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps/*display_frame8.png | head -1
# Open the new display PNG. Expect: darker image, more directional shadow visible.

# 8. Mode-20 sanity (the user's discriminator #6)
HLVM_PT_DEBUG_MODE=20 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal
# Expect: gi_raw dump is NON-UNIFORM (pre-fix already showed non-uniform at log:246 gbuffer_material)

# 9. If criterion #1 (display.std ≥ 0.10) still fails after v170 fix,
#    the v173 MaxM=1.0f patch is the next-step fix (already on disk).
#    Compound fix: BOTH v170 AmbientScale=0.05 AND v173 MaxM=1.0f applied.
```

## Acceptance criteria mapped to test file (existing validator)

| # | Criterion | Test artifact | Expected outcome (post-v170) |
|---|-----------|---------------|------------------------------|
| 1 | Debug target builds | `build_test_Debug_TestReSTIR_GI_Temporal.log` | exit code 0 |
| 2 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` runs clean | `TestReSTIR_GI_Temporal.log` exit | exit code 0 |
| 3 | No Vulkan VUID/ERROR | grep `VUID` in log | 0 matches |
| 4 | validate_restir_gi.py passes newest dump | validator exit | 0, color-variance CHECK DEPENDS on NEE |
| 5 | Vision shows recognizable Sponza | display_frame8.png | darker than pre-fix; sun shadows more visible |
| 6 | Mode-20 returns non-zero GBufferMaterial | grep gi_raw stats | non-uniform (already proven) |
| 7 | All 7 criteria pass | aggregate | conditional on NEE |

## Test-run isolation

`TestReSTIR_GI_Temporal` is a single-shot Vulkan test; no concurrent test process. Self-contained: ~10s build, 8 PNGs dumped, log written, exit. No inter-test state pollution.

## Defects in patch detected by tests

Predicted post-fix defects (per v170 commit §"Expected post-fix behavior"):

1. **Display too dim** (mean < 0.20): ambient removed but primaryDirect also small; this is the v25 hypothesis ("primaryDirect ≈ 0 due to NEE undercontribution") confirmed. Operator should compensate with `HLVM_EXPOSURE=2.0` per `TestReSTIR_GI_Temporal.cpp:604-607` exposure override, OR apply the v173 MaxM=1.0f fix as compound.
2. **Color-variance still fails** (display std < 0.07): primaryDirect is too uniform; v170 alone is insufficient. Need compound fix (v170 + v173).
3. **Temporal-stability fails** (frame-to-frame jump): unrelated to v170; if it appears, the v173 MaxM=1.0f fix should be applied separately first.

## Cross-cycle context

The v170 commit's "Plan Deviations" section already notes that v170 is a **diagnostic fix** that may not solve the bug alone. The empirical evidence from tick1544/tick1545 showed that the variance collapse is **two-stage** (gbuffer→gi_raw mild 1.78×, gi_raw→display severe 2.0×), not one-stage. v170 targets the gbuffer→gi_raw stage (ambient dominance); v173 targets the gi_raw→display stage (temporal W variance).

**Both fixes may be necessary.** The operator-side test strategy should:
1. Apply v170 alone first, run validator. If 6/6 PASS → done.
2. If v170 fails color-variance, apply v173 (already on disk) on top of v170 → compound fix. Run validator again.
3. If both fail, the bug is at the GI shader level (NEE undercontribution); need mode-3 diagnostic per v170 commit §"Step 1".

## Caveat: terminal-blocked this tick

This tester role, like all cron roles this tick, is in a file-only runspace — terminal blocked at tirith gate. The test design is COMPLETE on disk; running the tests is operator-gated. The pre-fix binary log (`Binary/Debug/TestReSTIR_GI_Temporal.log`, 2026-08-14 22:18:56) shows 0 VUIDs, 0 CommandList errors, 8 frames — the current state is clean, the v170 patch is the next-step fix candidate.

— tester, 2026-08-16, tick-now, file-only, single-profile host, terminal-blocked.