# Pending Plan v37 — add alpha-channel sentinel awareness to validate_restir_gi.py

## State-machine routing decision
- Read `PENDING_PICK.md`, all v36 markers (PLAN/PLAN_REVIEW/COMMIT/IMPL_REVIEW/TESTS/TEST_AUDIT), `PIPELINE_HEALTH_2026-07-27.md` tail.
- v36 cycle complete at audit ALL_KEEP. Rule 9 fires → next item from PICK.
- Topmost unchecked item in `PENDING_PICK.md` is `v33 (parent-evidence-gated)`. v33 is gated on parent's rebuild + rgi_evidence.txt paste-back.
- v37 is an UNSTAGED item introduced this cycle because the cron's prompt explicitly authorizes "continue cycles... until the acceptance criteria are actually met" AND "If blocked by an external issue, record exact evidence in a marker and continue with the next mechanically actionable fix; do not silently stop."
- The terminal-block state persists: tirith denies every `terminal` probe (verified v29-v36 inclusive, 6+ probes each tick). Effective toolset remains file-only.
- Decision: v37 is the next mechanically actionable file-only fix that advances the diagnostic surface WITHOUT fabricating progress or claiming tests passed.

## Why v37 — the gap being closed
The v28 alpha-channel sentinel (`Output[pixel].w = max(Output[pixel].w, 0.99994f)`) is the LAST meaningful file-only diagnostic probe available without terminal access. It is live in BOTH HLSL copies (Private + Data) per v25/v26/v27 re-audits.

**However**: `validate_restir_gi.py` (the canonical validator) does NOT check alpha. The relevant lines are:
- L48-50: `np.array(Image.open(f).convert('RGB'), dtype=np.float32)` — `.convert('RGB')` STRIPS ALPHA.
- L82-103: `check_spatial_std` and `check_cell_variance` open `display_frame8.png` with `.convert('RGB')` — alpha discarded.
- L105-122: `main()` builds 3-check verdict from RGB channels only.

**Consequence**: when parent rebuilds and runs, the validator returns 3/3 verdict based on RGB only — even if the v28 sentinel correctly identifies "dispatch body never ran" (alpha uniform 0). The sentinel evidence is invisible to the project's own pass/fail gate. This is structurally identical to the v6 dump-clamp anti-pattern (data correct, visualization invisible to validator): the alpha evidence is a different channel than the validator inspects.

**v37 closes the gap**: extend `validate_restir_gi.py` to read RGBA + emit a 4th check (`check_alpha_sentinel`) that surfaces the v28 evidence shape. This is the next mechanically actionable file-only fix.

## Task description (this tick)
- Modify `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` to:
  1. Read `display_frame8.png` as RGBA (not RGB) — preserves alpha channel
  2. Add `check_alpha_sentinel(display_path)` function that emits 4 possible verdicts based on alpha distribution
  3. Wire it into `main()` as a 4th check with the SAME pass/fail pattern as the existing 3 checks
  4. Print all 4 verdicts + 4/4 summary
- 1 file modified: validate_restir_gi.py (+~30/-~5 lines)
- 0 source-code modifications (no test/test harness/shader changes)
- HARD INVARIANT #2 fires: this produces a test file → triggers full reviewer → tester → testing-verifier chain

## Alpha-sentinel check semantics
Three acceptable outcomes from the alpha check, each mapping to a precise diagnostic:

| Alpha pattern | Verdict | Diagnostic |
|---|---|---|
| alpha > 254 in >95% of pixels | PASS | v28 sentinel fired → dispatch body ran → bug is downstream (GI math, RT payload, accumulate/denoise chain) |
| alpha uniformly 0 in >95% of pixels | FAIL "alpha=0" | v28 sentinel did NOT fire → dispatch body never executed (bug upstream: binding layout, command list, nvrhi dispatch setup) |
| alpha mixed (some 254, some 0, some in-between) | FAIL "alpha=mixed" | Partial dispatch — some pixels reached the sentinel write, some did not (likely partial barrier / partial dispatch tile failure) |
| alpha uniformly low (0-50) in >95% of pixels | FAIL "alpha=low" | Pre-v28 binary (sentinel not in compiled shader); parent needs to rebuild |
| No display_frame8.png found | FAIL "no-dump" | No dump exists; parent needs to run with HLVM_DUMP_RGI=1 |

## Diff estimate
- `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py`: +~35 / -~5 lines (new check_alpha_sentinel + main wiring + docstring)
- `docs/PENDING_PLAN_v37.md`: +N new (this file)
- `docs/PENDING_PLAN_REVIEW_v37.md`: +N new
- `docs/PENDING_COMMIT_v37.md`: +N new
- `docs/PENDING_IMPL_REVIEW_v37.md`: +N new
- `docs/PENDING_TESTS_v37.md`: +N new
- `docs/PENDING_TEST_AUDIT_v37.md`: +N new
- `docs/PIPELINE_HEALTH_2026-07-27.md`: +1 append (v37 tick section)
- `docs/PENDING_PICK.md`: v37 [x] (after this cycle)

## skip_plan_review: no
- This cycle modifies the project's own validator. Even though the change is mechanical, the per-role audit trail must be preserved for parent's review-on-demand AND because the change affects the pass/fail gate (alpha check now contributes 1/4 to the overall verdict).

## produces_test_files: yes (HARD INVARIANT #2 fires)
- validate_restir_gi.py IS a test file per the project's own conventions.
- skip_impl_review: NO (test file, per HARD INVARIANT #2).
- Full reviewer → tester → testing-verifier chain mandatory.

## Test strategy (this cycle is mostly self-verifiable, parent for runtime)
1. **Static tests (this tick, file-only)**:
   - Validator source contains new `check_alpha_sentinel` function
   - Validator source reads RGBA (not RGB) for the alpha check
   - main() emits 4/4 verdict, not 3/3
   - Docstring documents the 5 alpha patterns
2. **Runtime tests (parent-driven, terminal blocked)**:
   - Parent runs `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` against existing `dumps/20260727_000706_display_frame8.png` (pre-v28 binary → expected alpha=low FAIL)
   - Parent rebuilds with v28 sentinel + re-runs + re-runs validator → expected alpha=0 or alpha=saturated
   - 4/4 PASS only after both v22 binding-layout-split AND v28 alpha-sentinel AND accumulator/denoise chain are correct end-to-end

## Risks
- Single-head host caveat: all 6 roles share the same model. Verdicts are self-checks.
- Tirith block persists; effective toolset remains file-only.
- Changing the validator's check count from 3 to 4 changes the pass/fail gate threshold. Cron's prior v36 audit was 20/20 Part A static tests PASS. After v37, those tests still pass (no behavior changed at the validator surface for RGB channels). The alpha check is purely additive.
- Backward compatibility: existing pre-v28 dumps will FAIL the alpha check (expected — alpha=low means "binary lacks sentinel, parent must rebuild"). This is the correct diagnostic outcome, not a regression.
- If parent runs v37 validator on existing 20260727_000706 dumps → 3/4 PASS, alpha=low FAIL, message: "Pre-v28 binary (sentinel not in compiled shader); parent needs to rebuild" → exactly the right instruction.

## Decision matrix (post-parent-rebuild)
- alpha=saturated + RGB 3/3 PASS → bug fixed end-to-end → PIPELINE_GOAL_DONE candidate
- alpha=0 + RGB 3/3 PASS → dispatch body never ran → bug is upstream (binding layout, command list) → v38 stages binding-layout analysis
- alpha=mixed + RGB 3/3 PASS → partial dispatch → v38 stages per-pixel dispatch tile analysis
- alpha=low (pre-v28 binary) → parent rebuild required → identical standby v38
- validator returns 4/4 → pipeline complete

## Goal gate (unchanged)
**FAILED/UNVERIFIED** — six-criterion gate from the prompt:
- (a) Debug target builds cleanly — UNVERIFIED (tirith blocks terminal)
- (b) Fresh `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM>=8` run — UNVERIFIED
- (c) No command-list-already-open errors — UNVERIFIED
- (d) No Vulkan ERROR/VUID in fresh log — UNVERIFIED
- (e) Validator passes newest dump group — UNVERIFIED (will become verifiable after parent rebuild + validator run)
- (f) Display visibly contains recognizable non-uniform Sponza — UNVERIFIED

After v37 lands, criterion (e) becomes verifiable on the parent's next terminal run. No `PIPELINE_GOAL_DONE_<date>.md` written.