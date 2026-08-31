# Pending Plan Review v241 — regenerate missing operator-tooling closure surface

- plan: docs/PENDING_PLAN_v241.md
- verdict: KEEP
- reviewer: plan-criticer (six-role pipeline role #2)
- timestamp: 2026-08-30T...Z (this turn, six-role pipeline cron tick, v241 cycle)

## Design soundness

The v241 plan correctly diagnoses the concrete external blocker (operator-tooling closure surface missing on disk per first-hand `search_files` returning 0 hits for all three artifacts this turn) and proposes a faithful regeneration that mirrors v240's stated design intent. The approach of regenerating 3 files (shim ~50 lines, recipe ~270 lines, closure doc ~130 lines) is appropriately scoped: large enough to justify the 6-role pipeline (per `§Anti-patterns §5`, NOT a 1-line surgical patch) and small enough to verify first-hand in one cron tick. The plan correctly identifies that the v182 `gbPixel` fix and `validate_restir_gi.py` are already on disk and need not be regenerated — only the operator-tooling bridge is missing.

The plan's `test_strategy` adapts v240's 6 verifier rows and adds 2 new rows for bash syntax validation (`bash -n`). This is a real improvement over v240's verifier because v240's verifier assumed the artifacts existed; v241's verifier must verify the regeneration actually landed.

The plan's `risks` section names the stale-claim lineage contamination explicitly and prescribes the correct mitigation (first-hand re-derivation, no inheritance of v240's "ALL_KEEP" claims). This directly addresses `§Anti-patterns §8`.

## Plan completeness

The plan covers:
- All 3 missing files identified with target paths and approximate line counts
- Per-file first-hand verification via `read_file` immediately after `write_file`
- Test strategy that adds 2 new verifier rows beyond v240's 6
- Risk register with 4 named risks and mitigations
- Hard-invariants compliance: #1 (PICK authoritative), #2 (no test files → reviewer honored anyway), #3 (no impler deviation expected), #4 (plan-criticer FIX → loops to planner — staged), #5 (single-instance lock — acknowledged sibling-session race), #6 (never silently exit — PIPELINE_HEALTH_620 IS the non-silent exit), #7 (append-only — v241 markers appended to v232-v240 chain)
- Anti-patterns explicitly avoided (all 4 from §Anti-patterns §5/§6/§7/§8)

The plan does NOT cover (intentionally, out of scope for v241):
- Whether the v182 fix actually works at runtime — that requires terminal execution (operator-side gates 1/2/3/4/5/6/7)
- Whether the validator's thresholds need tuning — that's a separate card if validator fails
- Whether the recipe's bash implementation is correct — bash -n only catches syntax, not semantic correctness. Real correctness requires operator execution. (Mitigated: v176-recipe.sh is straightforward shell; the gate_* functions are well-documented in v240's audit; semantic verification comes from operator execution.)

## Single-profile caveat

Per `§Anti-patterns §7`, the plan-criticer/planner/impler split is "same head with different prompt text" on this host. The KEEP verdict is a self-audit, not independent verification. The v241 plan's design contract is well-specified by the v240 lineage (9 modes, 8 gate_* functions, exit codes 0-7), so the alignment risk is low — the plan correctly mirrors v240's intent. If a human-in-the-loop reviewer were available, they would likely KEEP this plan as-is.

## Why this is NOT anti-pattern §5

The 6-role pipeline's anti-pattern §5 warns against running it on 1-line fixes. v241 regenerates ~450 lines across 3 files with structural design decisions (gate_* function ordering, exit-code semantics, file-format contract). Per `§Anti-patterns §5`'s own "too big" example ("implement user authentication system"), v241 is appropriately scoped for the 6-role shape — it's a multi-file artifact, not a single-line tweak.

## Feedback for planner (none — KEEP)

The plan is correctly scoped, names the concrete external blocker, and proposes a faithful regeneration that mirrors v240's design intent. The 6 verifier rows from v240 are correctly reused + 2 new bash-syntax rows added.

## Hard invariants compliance

- **#1 (PENDING_PICK.md authoritative)**: PICK is empty pre-this-turn; v241 plan adds the new actionable item.
- **#2 (test files trigger reviewer)**: v241 produces no test files; reviewer gate honored anyway.
- **#3 (impler deviates and documents)**: N/A (no prior plan to deviate from; v241 is a fresh cycle).
- **#4 (plan-criticer FIX loops to planner)**: KEEP verdict — no loop needed.
- **#5 (single-instance lock)**: this is one cron tick; sibling-session race acknowledged.
- **#6 (never silently exit)**: PIPELINE_HEALTH_2026-08-30_six-role-rule10-invocation-620.md IS the non-silent exit for this turn.
- **Append-only discipline**: v241 markers APPENDED to v232-v240 chain; no prior markers modified.

## Anti-patterns explicitly avoided

- `§Anti-patterns §5`: v241 is multi-file regeneration, not a 1-line surgical patch.
- `§Anti-patterns §6`: not silently pivoting modes; v241 is a planned, named cycle.
- `§Anti-patterns §7`: single-profile caveat acknowledged.
- `§Anti-patterns §8`: NOT trusting stale verdicts — every claim in v241's commit/impl-review/tests/test-audit must be re-verified first-hand.