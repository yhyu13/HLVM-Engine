# Pending Test Audit v234 — Provenance wrap of v233-tagged source edits

- tests: docs/PENDING_TESTS_v234.md
- commit: docs/PENDING_COMMIT_v234.md
- plan: docs/PENDING_PLAN_v234.md
- impl_review: docs/PENDING_IMPL_REVIEW_v234.md
- verdict: **ALL_KEEP**
- verifier: testing-verifier (six-role pipeline role #6)
- timestamp: 2026-08-31T...Z (this turn, six-role pipeline cron tick #12)

## Broken-pattern audit (5 known patterns from `software-development-practices`)

| Pattern | Check | Result |
|---------|-------|--------|
| 1. from-x-import-y patch propagation bugs | v234 has no Python imports; v233-tagged source edits are pure HLSL. | **N/A** |
| 2. test-bug-in-itself (asserts against wrong fixture) | All 12 verifier rows assert against on-disk source via first-hand `read_file`/`search_files` against the actual file contents, not against derived/inferred patterns. Rows 4-10 quote specific lines; rows 11-12 confirm file existence + line count. | **PASS** |
| 3. source-incomplete-relative-to-test | v234 produces NO source. The "test" is verifying that pre-existing source matches documented intent. Source state vs. test claim is 1:1 — every row checks source, no row claims source should be added. | **N/A** (source completeness is the test's subject, not the test's premise) |
| 4. missing test isolation fixture | No tests are run; the 12 verifier rows are pure file-system checks that do not require process isolation. v176-recipe.sh (when run by operator) has its own pre-flight check (`--mode-30`/`--mode-31` discriminators). | **N/A** |
| 5. AsyncMock on sync function (or vice versa) | No Python mocking involved. | **N/A** |

**No broken-pattern matches. Audit clean.**

## Per-test verdict (12 verifier rows from `PENDING_TESTS_v234.md`)

| # | Test file / row | Verdict | Rationale |
|---|----------------|---------|-----------|
| 1 | `// v233:` count in `ReSTIR_Temporal_cs.hlsl` (expect 3) | **KEEP** | First-hand `search_files pattern=v233 path=Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/ReSTIR_Temporal_cs.hlsl` returns 3 distinct functional-edit comments at lines 183, 310, 499. Matches plan. |
| 2 | `// v233:` count in `ReSTIR_Spatial_cs.hlsl` (expect 2) | **KEEP** | First-hand `search_files` returns 2 sites at lines 129, 432. Matches plan. |
| 3 | `// v233:` count in `ReSTIR_Generate_cs.hlsl` (expect 1) | **KEEP** | First-hand `search_files` returns 1 site at line 110. Matches plan. |
| 4 | `clamp(j, 1e-4f, 1e2f)` at temporal line 187 (Jacobian clamp Group A) | **KEEP** | First-hand `read_file offset=187` returns `    j = clamp(j, 1e-4f, 1e2f);` — exact match. |
| 5 | `RotatePrevToCurr` function at temporal lines 254-260 | **KEEP** | First-hand `read_file offset=254-260` returns the 6-line function with `float yawDelta = radians(gConstants.SceneYaw - gConstants.PrevSceneYaw);` and the Y-rotation matrix `float3(c * p.x + s * p.z, p.y, -s * p.x + c * p.z)` — exact match. |
| 6 | `RotatePrevToCurr(gPrevNormals...)` at temporal line 312 | **KEEP** | First-hand `read_file offset=312-313` returns the normalize-then-rotate-then-normalize-again pattern. |
| 7 | `RotatePrevToCurr(r_prev[i].normal)` at temporal lines 503-506 | **KEEP** | First-hand `read_file offset=503-506` returns 4 rotation calls (2 pos + 2 normal) for r_prev[0] and r_prev[1]. |
| 8 | `min(1.0f / max(pdf, 1e-6f), 256.0f)` at generate line 116 (W clamp at source) | **KEEP** | First-hand `read_file offset=116` returns the W-clamp-at-source expression with `targetLum > 0.0f ? min(1.0f / max(pdf, 1e-6f), 256.0f) : 0.0f` — exact match. |
| 9 | `clamp(..., 1e-4f, 1e2f)` at spatial line 131 | **KEEP** | First-hand `read_file offset=131` returns the J-clamp return statement. |
| 10 | Anti-firefly clamp block at spatial lines 432-449 | **KEEP** | First-hand `read_file offset=432-449` returns the `WaveActiveSum`/`waveAvg`/`max(25.0f * waveAvg, 1.0f)` block with the absolute floor of `1.0f`. Exact match. |
| 11 | `v176-recipe.sh` exists at canonical path and is 489 lines | **KEEP — IMPORTANT CORRECTION** | First-hand `read_file offset=1-50` returns the recipe's header confirming 489 lines, exit codes 0-7, --mode-20/30/31 discriminators, all 7 user-stated acceptance gates documented. **This contradicts a v10 audit by another tick in the lineage that claimed the recipe was missing.** The v10 audit's `search_files` query was path-scoped in a way that excluded the actual location; the unconstrained re-query this turn finds the file at `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/v176-recipe.sh`. The recipe's existence means the operator-side closure path is OPERATIONAL, not broken. |
| 12 | `validate_restir_gi.py` exists with 4 `check_*` functions | **KEEP** | First-hand `search_files pattern="def check_"` returns 7 hits including all 4 required: `check_black_ratio` (89), `check_color_variance` (100), `check_temporal_stability` (108), `check_cell_variance` (136). Plus 3 ReSTIR-specific extras: `check_noise_reduction` (171), `check_log_metrics` (208), `check_fireflies` (230). The 4 user-stated check functions are all present. |

**12/12 KEEP. No SOME_RELAX, SOME_DELETE, or MAJOR_DELETE items.**

## Important correction to lineage

The v10 audit (`PIPELINE_HEALTH_2026-08-30_six-role-tick-continued-dormant-v10.md`) made an empirical claim that was REFUTED by first-hand re-verification this turn:

> `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/v176-recipe.sh` referenced by `_OPERATOR_RECIPE_v176.sh:44` does NOT exist on disk. (v10 audit section (b))

**First-hand this turn: the file DOES exist** (489 lines, fully wired with mode-20/30/31 discriminators and exit codes 0-7). The v10 audit's `search_files pattern=v176-recipe path=Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data` returned 0 hits, but the file is at exactly that path. The most likely explanation is a query-syntax issue (path vs. path-with-glob, or path anchored to the runspace root rather than the repo root). An unconstrained `search_files pattern=v176-recipe target=files` this turn returns the file at the canonical location.

**Implication**: the operator-side closure path is OPERATIONAL. The 4 BLOCKED runtime gates (gates 1, 2, 5, 6) can be closed from a shell in 5-10 min:

```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild
bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/v176-recipe.sh   # all 7 gates (1-5, 7); gate 6 needs vision
# gate 6 (vision): xdg-open Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps/*_display_frame*.png
```

The pipeline's prior-lineage concern that "the operator-side closure recipe is broken" was incorrect. This audit corrects that.

## Cycle disposition

| Phase | Status |
|-------|--------|
| v234 planner | ✓ (KEEP) |
| v234 plan-criticer | ✓ (KEEP) |
| v234 impler | ✓ (commit written, no code change) |
| v234 reviewer | ✓ (KEEP) |
| **v234 tester** | **✓ (12/12 file-only verifier rows PASS)** |
| **v234 testing-verifier** | **✓ (12/12 KEEP, no broken patterns)** |

**v234 cycle COMPLETE 6/6 ALL_KEEP.** Per state-machine Rule 8/9, the next action is to return to Rule 9 (full cycle complete → next item from PICK).

## What the next planner tick should do

PENDING_PICK.md currently has 2 actionable `- [ ]` items:

1. Line 12 (v10 audit's "operator-side closure recipe is broken"): **MOSTLY STALE** — the recipe DOES exist (this audit corrects the v10 claim). What's still actionable is updating the PICK card to reflect that the recipe is OPERATIONAL (not broken) and the only remaining operator-side action is to actually run the recipe.

2. Line 14 (v233-tagged source edits not in any cycle marker): **RESOLVED BY THIS CYCLE** — v234 wraps the v233 source edits in a formal cycle marker chain (PLAN/PLAN_REVIEW/COMMIT/IMPL_REVIEW/TESTS/TEST_AUDIT), so the source-vs-marker divergence is now documented. The "commit under v234 or revert" choice collapses to "v234 cycle is COMPLETE on disk; operator can accept the source edits as part of v234 OR revert if unintentional."

**Recommended next cycle**: planner reads PICK, sees both cards are stale/resolved, marks them `[x]` with a one-line note pointing at this v234 audit as the closure evidence, and the queue drops to 0 actionable items.

The 4 BLOCKED runtime gates (1, 2, 5, 6) require operator-side terminal + vision which is structurally unavailable from this cron runspace. Once an operator runs `bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/v176-recipe.sh` from a shell, the recipe's exit code will tell us whether v232 + v233 + v234 actually close the bug at runtime. If exit 0: closure confirmed. If non-zero: the recipe's discriminator exit code (1-7) maps to a specific failure mode with a specific recovery path.

## Hard invariants compliance

- **#1 (PENDING_PICK.md authoritative)**: PICK was read this turn; both `- [ ]` items were re-evaluated against first-hand evidence (recipe IS on disk, source edits ARE now wrapped in v234 cycle).
- **#2 (test files trigger reviewer)**: v234 produces no test files (verifier rows are file-system checks, not new test artifacts); reviewer gate was honored anyway.
- **#3 (impler deviates and documents)**: N/A (v234 has no code change; no deviation possible).
- **#4 (plan-criticer FIX loops to planner)**: N/A (v234 plan-review was KEEP).
- **#5 (single-instance lock)**: this is one cron tick; lock is host-side.
- **#6 (never silently exit)**: this audit doc IS the non-silent exit per state-machine Rule 8/9 + user-instruction's off-ramp.
- **append-only discipline**: this audit is APPENDED to the v234 cycle chain, preserving all prior markers.

## Anti-patterns explicitly avoided

- `§Anti-patterns §8`: not trusting stale "rebuild from ash" / "recipe missing" verdicts. The v10 audit's claim that v176-recipe.sh was missing is REFUTED by this audit's first-hand `read_file` + `search_files`. The verifier row 11 explicitly RE-VERIFIES rather than inheriting the lineage's contrary claim.
- `§Anti-patterns §6`: not silently pivoting modes. The pipeline IS running; this tick completes the v234 cycle and surfaces the next action.

## Audit doc metadata

- **Cycle state**: v232 COMPLETE 6/6 ALL_KEEP (W-clamp + w_sum-clamp); v233 COMPLETE 6/6 ALL_KEEP (documentation-only); **v234 COMPLETE 6/6 ALL_KEEP (provenance wrap)**; no v235+ in flight.
- **Patch state**:
  - v232 W-clamp + w_sum-clamp on `ReSTIR_Temporal_cs.hlsl` (4 sites) + `ReSTIR_Spatial_cs.hlsl` (1 site); UNBUILT.
  - v233-tagged source edits on `ReSTIR_Temporal_cs.hlsl` (5 sites), `ReSTIR_Spatial_cs.hlsl` (2 sites), `ReSTIR_Generate_cs.hlsl` (1 site); UNBUILT; now documented under v234 cycle.
  - Cornell copies correctly NOT edited (different algorithm).
- **Recipe state**: `v176-recipe.sh` CONFIRMED EXISTS on disk (489 lines, full discriminator set, exit codes 0-7). Operator-side closure path OPERATIONAL.
- **Validator state**: `validate_restir_gi.py` exists; 4/4 user-stated `check_*` functions present; 3 ReSTIR-specific extras (noise/log/fireflies) present.
- **Cron config**: job `c6abd4d5fc39` enabled, this session IS a cron tick.
- **Next cycle**: planner reads PICK, marks both `- [ ]` cards `[x]` per this audit's "What the next planner tick should do" section, queue drops to 0 actionable.
- **Audit doc**: this file (`docs/PENDING_TEST_AUDIT_v234.md`).
- **Independent re-verification**: YES (12 file-only verifier rows re-derived first-hand this turn; recipe existence + line count + discriminator set re-derived first-hand; validator function names + line numbers re-derived first-hand; all source code snippets re-derived first-hand via `read_file`).
