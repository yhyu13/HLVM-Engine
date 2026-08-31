# Pending Plan v240 — closure-surface completion

- task: Verify the operator-side closure surface for the TestReSTIR_GI_Temporal GBuffer SRV binding fix is genuinely complete on disk. Honest audit of what prior v237/v238 cycles claimed vs. what is verifiably on disk in this turn.
- source: docs/DIAGNOSTIC_2026-07-30.md (binding-broken hypothesis, refuted by v182), docs/PENDING_PLAN_v238.md (claimed operator-shim creation), docs/PENDING_TEST_AUDIT_v237.md (v237 closure surface claims), Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal.log (fresh 2026-08-27 11:54:32 evidence)
- approach:
  1. **Honest ground-truth verification (this turn)**: re-derive the on-disk state from actual `read_file`/`search_files` calls against the file system, not by inheriting prior audit claims.
  2. **Identify gaps**: v235 (v176-recipe.sh restoration) and v238 (_OPERATOR_RECIPE_v176.sh + Operator_Closure.md creation) both claimed artifacts that `search_files` showed as absent in initial v240 scans.
  3. **Create the missing artifacts**: write_file produced _OPERATOR_RECIPE_v176.sh (46 lines) and Operator_Closure.md (128 lines) at repo root with first-hand content verification.
  4. **Confirm v176-recipe.sh exists at canonical path**: the sibling-agent warning on my write_file calls revealed another concurrent session (likely v239 audit) had produced v176-recipe.sh at the canonical path between my initial search and my write_file calls. Read confirms 273 lines with full gate_* functions and exit codes 0-7.
- diff_estimate: +447 lines across 2 new files (46 + 128, with the v176-recipe.sh at 273 attributed to the sibling)
- skip_plan_review: yes — v240 is a closure-verification cycle, not a design cycle. The plan is "verify what's on disk, fix what's missing, document the honest state." No design decisions to critique.
- test_strategy: tester role #5 runs a 6-row file-only verifier confirming: (a) shim exists at repo root with correct pass-through shape, (b) closure doc exists with 7-gate status table, (c) canonical recipe exists at canonical path with all 7 gate_* functions, (d) validator exists and is referenced by the recipe, (e) no source files modified, (f) no governance files modified.
- risks:
  1. **The honest state is messier than prior audits claimed.** v237 audit said "v176-recipe.sh 273 lines on disk, 8/8 verifier rows PASS." v238 audit said "_OPERATOR_RECIPE_v176.sh exists at repo root + Operator_Closure.md 152 lines." Initial v240 `search_files` returns 0 matches for all three. The truth (verified this turn by read_file): v176-recipe.sh was created by a concurrent sibling session during this same tick; _OPERATOR_RECIPE_v176.sh and Operator_Closure.md were created by this session's write_file calls. The artifacts DO exist now, but were missing at the start of v240.
  2. **The audit chain's "ALL_KEEP" verdicts inherited claims rather than re-verifying.** v238's plan-criticer gate was skipped (`skip_plan_review: yes`), and v238's reviewer gate was a self-audit (same model, same runspace). Both accepted the impler's claim that the shim + doc were created. The single-instance lock (HARD INVARIANT #5) doesn't apply across concurrent cron sessions, so multiple sessions may have been writing to the same target paths simultaneously.
  3. **No code changes.** v240 is operator-tooling only. The actual fix (v182 gbPixel + v232 W-clamp + v233 normal rotation) is on disk and unchanged from prior cycles.
- relation to v232-v237 + v238: this is the FINAL cycle of the chain. v237 completed the empirical closure surface (8/8 file-only verifier rows); v238 added the operator-tooling layer (claims only, partially missing on disk at the start of v240); v240 actually produces the missing operator-tooling files with first-hand verification.
- relation to the user's "Continue iterating until all criteria met" instruction: the criteria are now genuinely met on disk for the 5 file-only gates (1, 2, 3, 4, 7). Gates 5/6 (validator + vision) remain operator-side (terminal + human eye), but the artifacts to invoke them are now in place.

## Anti-patterns explicitly avoided

- `§Anti-patterns §5`: not running a 6-role cycle on documentation. v240 PRODUCES the missing operator tooling that v238 claimed to have produced but didn't.
- `§Anti-patterns §6`: not silently pivoting modes. The pipeline is running; this tick completes the operator-tooling gap.
- `§Anti-patterns §8`: not trusting stale verdicts. v240 re-verifies first-hand (read_file on _OPERATOR_RECIPE_v176.sh, Operator_Closure.md, v176-recipe.sh) instead of inheriting v238's "ALL_KEEP" claim.
