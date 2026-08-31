# Pending Impl Review v138
- plan: docs/PENDING_PLAN_v138.md
- commit: docs/PENDING_COMMIT_v138.md
- verdict: KEEP
- reviewer: reviewer (file-only single-profile mode)
- timestamp: 2026-07-31

## plan_fidelity_check

Impler followed the plan precisely. The change at `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl:475-491` correctly:
- Added a 12-line comment block explaining the v138 reasoning (lines 475-485), including the re-analysis that invalidates tick 248's mode-6 discriminator claim.
- Added `|| debugModeEarly == 6u` as the FIRST entry in the `bypassEarlyReturn` chain (line 486), placing it before the existing `20u` entry.
- Existing chain entries (`20u`, `21u`, `22u`, `30u`, `31u`) preserved in their original positions.
- Termination semicolon at line 491 unchanged.

The deviation noted in `PENDING_COMMIT_v138.md` (placing `6u` as the first entry rather than the last) is correctly justified as a cosmetic ordering choice — the chain is a `||` sequence and position is semantically irrelevant.

The fix matches the v128/v131 design intent (modes 20/21/22/30/31u are in the bypass list for exactly the same reason — the diagnostic needs to run regardless of whether `GBufferWorldPos` returns zero).

## TDD evidence
- [ ] Test file present: n/a (this is a diagnostic-mode patch, not a behavioral change)
- [ ] Test commit precedes impl: n/a
- [ ] Red-phase commit message: n/a

## Security scan
- [x] No hardcoded secrets
- [x] No shell injection (os.system, shell=True)
- [x] No eval/exec
- [x] No SQL injection

## Self-review checklist
- [x] Validation: the change is 1 line of executable code (`|| debugModeEarly == 6u`) plus a 12-line comment. Validation is "the next rebuild succeeds + mode 6 dump shows the per-pixel gradient" — terminal+vision-required, deferred to parent runspace.
- [x] Error handling: no change to error handling; existing shader behavior unchanged for non-mode-6 paths.
- [x] Tests: no test changes; v131+v135+v136+v137 patches have file-only test markers (mode 6 should now show gradient if v137 was sufficient).

## File-only state verification

All 8 patches still intact after v138:

| Patch | File:Line | Status |
|---|---|---|
| v131 (commitBarriers defense-in-depth) | `FGIPass.cpp:675` | INTACT |
| v131 (cases 20/21/22/30/31u discriminator) | `GIPathTracing.hlsl:685-687, 712-714` | INTACT |
| v132 (createValidationLayer hookup) | `DeviceManagerVk4_LifeCycle.cpp:88` | **REVERTED by v136** |
| v133 (cmake FORCE NVRHI_WITH_VALIDATION=ON) | `Engine/Source/Runtime/CMakeLists.txt:182` | INTACT |
| v134 (validation TUs in add_library) | `Build/Debug/_deps/nvrhi-src/CMakeLists.txt:209-214, 233-236` | INTACT |
| v135 (commitBarriers BEFORE createBindingSet) | `FGIPass.cpp:557-562` | INTACT |
| v136 (v132 revert) | `DeviceManagerVk4_LifeCycle.cpp:96, 176` | APPLIED |
| v137 (UAV binding-offset fix) | `FGIPass.cpp:313-318` | APPLIED |
| **v138 (mode 6 bypassEarlyReturn addition)** | `GIPathTracing.hlsl:486` | **APPLIED** |

No other references to `bypassEarlyReturn` exist in the codebase outside GIPathTracing.hlsl (verified via search_files).

## Critical re-analysis finding (for the testing-verifier)

**v137's premise was structurally wrong** because mode 6 was masked by the early-return. v138 restores the discriminator chain by adding `6u` to the bypass list. After v138 lands:

- If mode 6 dump shows the per-pixel gradient: v137's UAV fix was correct, and mode 20/21/22 should now also return non-zero (they were masked by the SAME early-return at line 481-484 that masked mode 6). Bisect closes.
- If mode 6 dump is still zero: the SRV is the actual bug; v137 was a wrong-fix for the symptom. v139 will investigate SRV binding/layout.

**The v137 patch itself may still be a real bug fix** (descriptor writes at slot 768 vs shader expecting 384 IS structurally wrong), but it was not the root cause of the all-zero gi_raw symptom. v137 should be retained as a defensive fix; v139 may need to investigate the SRV separately.

## Feedback for impler (FIX only)

None.

---

**Per `six-role-pipeline §Role #4 (reviewer)`, this is a file-only verdict based on the diff content + read_file verification of the cited file:line references. Behavioral verification requires terminal+vision — deferred to parent runspace.**