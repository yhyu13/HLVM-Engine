# Pending Plan Review v242 — fix operator-tooling recipe bugs

- plan: docs/PENDING_PLAN_v242.md
- verdict: KEEP
- reviewer: plan-criticer (six-role pipeline role #2)
- timestamp: 2026-08-30T...Z (this turn, six-role pipeline cron tick, v242 cycle)

## Design soundness

The v242 plan correctly identifies 3 concrete, file-only-verifiable bugs in `v176-recipe.sh` that the v241 cycle's 8/8 verifier rows did not catch because v241 verified file EXISTENCE and STRUCTURAL SHAPE, not semantic correctness. Each bug is mapped to a specific C++ source line that defines the correct behavior:

| Bug | Symptom | C++ contract reference | Fix |
|-----|---------|------------------------|-----|
| 1. `DUMPS_DIR` path | dumps never found; `find ... -newer "${LOG_FILE}"` returns 0 hits | `TestReSTIR_GI_Temporal.cpp:2951-2953` dumps to `${GProjectRoot}/.../TestReSTIR_GI_Temporal_Data/dumps` | `DUMPS_DIR="${TEST_DATA_DIR}/dumps"` |
| 2. `gate_val` missing required arg | validator exits 2 (USAGE) on every call | `validate_restir_gi.py:510` declares `dump_dir` as REQUIRED positional | `python3 "${VALIDATOR}" "${DUMPS_DIR}" --log "${LOG_FILE}"` |
| 3. `gate_m20` wrong filename glob | `*mode20*.png` returns 0 hits; gate exits 2 | `TestReSTIR_GI_Temporal.cpp:3022,3055` dumps as `<timestamp>_<channel>_frame<N>.png` where `channel` is the dump's nickname (mode 20 → `display` because OutputTexture → DisplayTexture) | `ls -t "${DUMPS_DIR}"/*_display_frame*.png` |

This is exactly the kind of bug the v242 plan-critique is meant to catch: a verifier that says "all structural elements present" but misses "elements present but incorrectly wired." The plan's `test_strategy` is a 6-row semantic-correctness verifier, not a 6-row existence verifier — the right shape for catching this class of bug.

The plan's `risks` section is honest about the sibling-session race (per `six-role-pipeline §When NOT to use this skill §4` on repeated investigations), the dump-pattern dependency on the mode-20 invocation, and the most important risk: **without v242, the operator would see exit codes 5 and 6 from the recipe and incorrectly conclude the v182 GPU fix is broken**. v242 is the corrective cycle that prevents the operator from wasting hours chasing phantom GPU bugs that are bash bugs.

## Plan completeness

The plan covers:
- All 3 bugs identified with target file + line numbers + C++ contract reference
- 3 patch operations (`+5 / -4` lines total) — small enough for 1 cron tick, large enough to need the 6-role shape (3 distinct root causes, not a 1-line fix)
- 6-row semantic-correctness verifier (replaces v241's 8-row existence verifier)
- Risk register with 4 named risks and mitigations
- Hard-invariants compliance acknowledged
- Anti-patterns explicitly avoided (all 4 from §Anti-patterns §5/§6/§7/§8)

The plan does NOT cover (intentionally, out of scope for v242):
- Whether the operator-side runtime actually closes gates 1-7 (requires terminal, blocked by tirith)
- Whether v182's fix really fixes mode-20 at runtime (requires operator to run the binary; v242 only ensures the recipe correctly drives that run)
- Whether the validator's thresholds need tuning (separate card if validator fails)

## Single-profile caveat

Per `§Anti-patterns §7`, the plan-criticer/planner/impler split is "same head with different prompt text" on this host. The KEEP verdict is a self-audit. The 3 bugs are unambiguous (each maps to a specific C++ source line), so the alignment risk is low. If a human-in-the-loop reviewer were available, they would likely KEEP this plan as-is.

## Why this is NOT anti-pattern §5

The 6-role pipeline's anti-pattern §5 warns against running it on small fixes. v242 fixes 3 lines, but each line is the resolution of a distinct root cause (1 = wrong path constant, 2 = missing required CLI arg, 3 = wrong filename glob). Per `§Anti-patterns §5`'s own framing, the relevant axis is whether the fix requires the pipeline's full state-machine discipline to land correctly, not how many lines change. Each v242 fix maps to a C++ contract that the verifier must re-read; without the discipline, the verifier (and the impler) would skip this verification.

## Feedback for planner (none — KEEP)

The plan is correctly scoped, names 3 concrete external bugs with file:line citations to the C++ source that defines the correct behavior, and proposes 3 minimal fixes that can be verified first-hand via `read_file` after `patch`. The 6-row semantic-correctness verifier is the right shape for catching this class of bug.

## Hard invariants compliance

- **#1 (PENDING_PICK.md authoritative)**: PICK §Active items is empty pre-this-turn; v242 plan adds the new actionable item.
- **#2 (test files trigger reviewer)**: v242 produces no test files; reviewer gate honored anyway.
- **#3 (impler deviates and documents)**: impler deviations (if any) will be documented in PENDING_COMMIT_v242.md's `## Plan Deviations`.
- **#4 (plan-criticer FIX loops to planner)**: KEEP verdict — no loop needed.
- **#5 (single-instance lock)**: this is one cron tick; sibling-session race acknowledged in risks.
- **#6 (never silently exit)**: this PIPELINE_HEALTH doc IS the non-silent exit.
- **Append-only discipline**: v242 markers APPENDED to v232-v241 chain; no prior markers modified.

## Anti-patterns explicitly avoided

- `§Anti-patterns §5`: v242 is 3 fixes with 3 distinct root causes, not a 1-line surgical patch.
- `§Anti-patterns §6`: not silently pivoting modes; v242 is a planned, named cycle.
- `§Anti-patterns §7`: single-profile caveat acknowledged.
- `§Anti-patterns §8`: NOT trusting v241's stale `8/8 PASS` existence verifier — v242's verifier is semantic-correctness, re-derived from first-hand `read_file` of both the recipe and the C++ contract.
