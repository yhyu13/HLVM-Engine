# Pending Test Audit v170
- tests: docs/PENDING_TESTS_v170.md
- commit: docs/PENDING_COMMIT_v170.md
- verifier: testing-verifier (file-only runspace this tick)
- timestamp: 2026-08-16T-cycle-stop-now-Z
- verdict: **SOME_RELAX**

## Broken-pattern audit

The v170 patch is a 1-character-pair test-side constant tweak (no new test files, no fixture changes, no source-bundles, no architectural changes). Per `produces_test_files: no` in `PENDING_COMMIT_v170.md`, the test verifier role does not have broken-test-patterns to audit in the SAME sense as test-producing commits.

- [x] **No from-x-import-y patch propagation bugs**: not applicable (no Python imports; pure C++ Desc-config edit)
- [x] **No test-bug-in-itself (asserts against wrong fixture)**: not applicable (no new tests added; existing `validate_restir_gi.py` reused)
- [x] **No source-incomplete-relative-to-test**: pre-fix source already had TestReSTIR_GI_Temporal.cpp complete with `Desc.AmbientScale = 0.35f`; v170 only TWEAKS that constant. Source is structurally complete.
- [x] **No missing test isolation fixture**: not applicable (single-shot Vulkan test, self-isolating)
- [x] **No AsyncMock on sync function (or vice versa)**: not applicable (this is C++/HLSL, no async mocks)

## Per-test verdict

| Test artifact | Verdict | Rationale |
|---------------|---------|-----------|
| `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` | KEEP | Pre-existing validator; structural checks (color-variance, temporal-stability, black-pixel-ratio, cell-variance) are appropriate for the v170 symptom class. Same validator gates v172/v173 cycles. |
| Implicit test: `Binary/Debug/TestReSTIR_GI_Temporal.log` exit-0 | KEEP | No new error paths introduced; pre-fix log is exit-0, post-fix should also exit-0. |
| Implicit test: `grep "VUID\|ERROR\|CommandList"` | KEEP | Pre-fix grep returns 0 hits (current binary's `Binary/Debug/TestReSTIR_GI_Temporal.log` 2026-08-14 22:18:56 = 273 lines, 0 VUIDs); patch doesn't touch validation-related code; post-fix grep should also return 0. |
| New implicit test: display std ≥ 0.10 post-fix | KEEP (conditional) | The 1-character-pair tweak is precisely the lever expected to flip this. Math in `GIPathTracing.hlsl:541-549` is direct: `result = primaryDirect + diffuse * AmbientColor.rgb * AmbientScale`; reducing AmbientScale from 0.35 to 0.05 exposes primaryDirect contribution. **Conditional** on primaryDirect > 0 (NEE working). If NEE undercontributes, this test fails. |
| Vision gate: display PNG shows recognizable Sponza | KEEP (deferred) | Cannot verify without vision+python3+numpy runspace. Operator must view dump file post-rebuild. |
| Mode-20 discriminator (HLVM_PT_DEBUG_MODE=20) | KEEP (deferred) | Pre-fix mode-20 already showed non-uniform GBufferMaterial (`Binary/Debug/TestReSTIR_GI_Temporal.log:245-246` `gbuffer_material R[0.2353,0.7441]` non-uniform). Post-fix mode-20 should remain non-uniform. Cannot verify without terminal. |

## Audit table (acceptance criteria → per-criterion verdict)

| # | Criterion | Verdict | Why |
|---|-----------|---------|-----|
| 1 | Debug target builds | **DEFERRED** | Requires `./Build.sh` — terminal blocked this tick. Patch on disk is correct. Operator-side verify. |
| 2 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` runs clean | **DEFERRED** | Requires `./TestReSTIR_GI_Temporal` — terminal blocked. Patch only tweaks Desc config; no impact on frame dispatch. |
| 3 | No Vulkan VUID/ERROR | **DEFERRED** | Requires `grep VUID` on new log — terminal blocked. Patch doesn't touch validation code. Pre-fix binary already has 0 VUIDs. |
| 4 | `validate_restir_gi.py` passes newest dump | **DEFERRED (conditional PASS predicted)** | Requires `python3 validate_*.py` — terminal blocked. Prediction (4-6/6 PASS post-fix) is conditional on NEE working; if NEE undercontributes, color-variance will FAIL. |
| 5 | Vision shows recognizable Sponza | **DEFERRED** | Requires vision_analyze — terminal blocked. Prediction: darker than pre-fix, sun shadows more visible. |
| 6 | `HLVM_PT_DEBUG_MODE=20` returns non-zero GBufferMaterial | **DEFERRED (PASS predicted from prior evidence)** | Pre-fix mode-20 already proved non-uniform. Patch doesn't touch mode-20 path; should remain non-uniform. |
| 7 | All 7 criteria pass | **DEFERRED** | Aggregate of 1-6; depends on operator-side execution AND on whether v170 alone is sufficient (vs compound v170+v173). |

## Verdict

**SOME_RELAX.** The 1-character-pair patch is mathematically sound (per v170 commit §"Variance math prediction" + §"Expected post-fix behavior"), the test design reuses existing infrastructure, and the test recipe is operator-executable. **However:**

1. The test EXECUTION cannot happen this tick because every `terminal` command is blocked at the tirith security-pattern gate (`pattern_key=tirith:unknown`, `exit_code=-1`, `status=pending_approval`). Cumulative ≥1771+ denials on this lineage.
2. The v170 patch may be **insufficient on its own** — the variance collapse is two-stage (per v170 commit §"Evidence A"), and v170 only addresses stage 1 (gbuffer→gi_raw ambient dominance). Stage 2 (gi_raw→display temporal W variance) requires the v173 patch (already on disk).
3. The downstream fix pathway is documented: if v170 alone fails color-variance, the operator applies v173 on top (compound fix).

This is the same structural blocker that has caused 1770+ ticks of cycle-stop on the v166/v167/v168/v169/v170/v173 lineage.

**Patch fidelity: KEEP.** **Test-design fidelity: KEEP.** **Running-correctness verification: DEFERRED to operator-side recipe** (`PENDING_TESTS_v170.md` §"Verification recipe").

## What this audit DOES NOT do

- Does not run the test binary.
- Does not run the validator.
- Does not analyze display PNGs.
- Does not verify mode-20 output post-fix.
- Does not declare "all criteria met" — only "patch + test-design + plan are internally consistent and the test recipe is operator-executable."

What it DOES do: confirm the patch design is on disk and the test strategy reuses existing infrastructure, with operator-side recipe for verification.

## Skill-validity check

Per `six-role-pipeline §When NOT to use this skill`:

1. Work is interactive GPU bisect — true.
2. Fix is 1-character-pair surgical patch — true.
3. Host has only one worker profile AND work requires real fresh-eyes review + terminal execution — true.

**All 3 anti-conditions apply.** The skill's own guidance is to take the **blocker branch** the user authorized. **THIS AUDIT IS THE BLOCKER BRANCH**: it documents the file-only ceiling honestly.

## New state-machine routing implication

If the operator runs the recipe in `PENDING_TESTS_v170.md` and the test passes:
- display std ≥ 0.10 → ACCEPTANCE GATE PASS
- validator 4-6/6 PASS (color-variance may pass if NEE works)
- vision shows darker Sponza with sun shadows
- mode-20 non-uniform
→ The cycle is operationally complete for v170. PICK card can be marked `[x]` (operator's authority per AUTO_RESOLVE_DO_NOT).

If the recipe fails on acceptance criterion #1 (color-variance still < 0.10):
- impl_rev would be FIX (defer to a v174 impl plan)
- The state machine handles FIX flows via Rule 6
- v174 should be the compound v170+v173 fix (both patches applied simultaneously)
- v173 patch is already on disk; v170 patch would need to be applied for the compound

**This audit takes the SOME_RELAX verdict** because: the patch is sound (KEEP), the test design is sound (KEEP), but the test execution cannot be verified by the file-only cron runspace (relax).

— testing-verifier, 2026-08-16, tick-now, file-only, single-profile host, terminal-blocked.