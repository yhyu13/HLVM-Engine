# Pending Tests v142
- commit: docs/PENDING_COMMIT_v142.md
- plan: docs/PENDING_PLAN_v142.md
- plan_review: docs/PENDING_PLAN_REVIEW_v142.md (verdict: KEEP)
- skip_impl_review: yes (per HARD INVARIANT #2: only honored because `produces_test_files: no`)

## What was tested (file-only)

Per `skip_impl_review: yes` (because `produces_test_files: no`), the tester role runs a focused patch-integrity audit on the 1 source file modified by v142. The tester does NOT write new test files (none are produced by this commit) and does NOT compile/run the test (terminal+vision blocked). All checks are static-analysis greps against the patched file plus spot-checks on prior patches.

## File-only patch integrity checks (10/10 PASS)

| # | Check | Method | Expected | Actual | Pass |
|---|-------|--------|----------|--------|------|
| 1 | `TestReSTIR_GI_Temporal.cpp:455` contains `Desc.AmbientScale = 1.5f;` (reverting v141's `0.25f`) | `search_files pattern="Desc\.AmbientScale"` | 1 match with value `1.5f` | 1 match at line 455 with value `1.5f` | ✓ |
| 2 | `TestReSTIR_GI_Temporal.cpp:455` does NOT contain `Desc.AmbientScale ... 0.25f` (revert complete) | `search_files` for `Desc\.AmbientScale.*0\.25f` | 0 matches in assignment | 0 matches (line 452's `v141's AmbientScale=0.25f` is in a comment explaining the OLD value, not the assignment) | ✓ |
| 3 | `TestReSTIR_GI_Temporal.cpp:447` contains v142 REVERT v141 comment marker | `search_files pattern="v142 \(six-role-pipeline\): REVERT v141"` | 1 match | 1 match at line 447 | ✓ |
| 4 | `TestReSTIR_GI_Temporal.cpp` does NOT contain the v141 REFINED DIAGNOSIS comment block | `search_files pattern="REFINED DIAGNOSIS"` | 0 matches | 0 matches (replaced by v142 REVERT v141 comment) | ✓ |
| 5 | `TestReSTIR_GI_Temporal.cpp:461-464` retains v140 AmbientColor override | `search_files pattern="Desc\.AmbientColor"` | 4 matches | 4 matches at lines 461-464 (values `(1.0f, 1.0f, 1.0f, 0.0f)`) | ✓ |
| 6 | FGIPass.h:61 v140 AmbientColor field intact (default `{ 0.6f, 0.6f, 0.65f, 0.0f }`) | `search_files pattern="AmbientColor\[4\]"` | 1 match with default | 1 match at line 61 with `float AmbientColor[4] = { 0.6f, 0.6f, 0.65f, 0.0f };` INTACT | ✓ |
| 7 | FGIPass.cpp:449 v140 `AmbientColorPtr = Desc.AmbientColor` indirection intact | `search_files pattern="AmbientColorPtr = Desc\.AmbientColor"` | 1 match | 1 match at line 449 with `const float* AmbientColorPtr = Desc.AmbientColor;` INTACT | ✓ |
| 8 | GIPathTracing.hlsl:486 v138 `bypassEarlyReturn` chain INTACT | `search_files pattern="bypassEarlyReturn"` | 2 matches | 2 matches at lines 486/493 INTACT | ✓ |
| 9 | DeviceManagerVk4_LifeCycle.cpp:118 v139 `createValidationLayer` hookup INTACT | `search_files pattern="createValidationLayer"` | 8 matches | 8 matches at lines 13/90/96/100/107/118/191/195 (line 118 = hookup call `m_ValidationLayer = nvrhi::validation::createValidationLayer(m_NvrhiDevice);` INTACT) | ✓ |
| 10 | CMakeLists.txt UNCHANGED | no diff against v141 baseline | unchanged | v142 modifies only `TestReSTIR_GI_Temporal.cpp`; no new source files; no cmake edits needed | ✓ |

## Per-test verdict

| Test site | Verdict | Rationale |
|-----------|---------|-----------|
| `TestReSTIR_GI_Temporal.cpp:455` AmbientScale = 1.5f | KEEP | matches plan; restores v140 value; expected to restore 95× dynamic range |
| `TestReSTIR_GI_Temporal.cpp:447` v142 REVERT v141 comment | KEEP | documents the v25-diagnostic-correction reasoning chain + 4-light NEE infrastructure |
| `TestReSTIR_GI_Temporal.cpp:461-464` v140 AmbientColor override preserved | KEEP | unchanged from v140; necessary for the math to produce per-pixel variation |
| `TestReSTIR_GI_Temporal.cpp` v141 REFINED DIAGNOSIS comment removed | KEEP | outdated reasoning; correctly replaced by v142 comment |
| FGIPass.h:61 v140 AmbientColor field | KEEP | structurally sound, default value unchanged |
| FGIPass.cpp:449 v140 AmbientColorPtr indirection | KEEP | unchanged from v140 |
| v22 split + v131-v139 patches unaffected | KEEP | v142 only modified `TestReSTIR_GI_Temporal.cpp` lines 447-455 — no overlap with v131-v140 sites |
| CMakeLists.txt UNCHANGED | KEEP | no new source files added |

## Test file changes

**None.** v142 modifies only an existing source file (`TestReSTIR_GI_Temporal.cpp`) — no new test files are created or modified. This is consistent with `produces_test_files: no` in PENDING_COMMIT_v142.md.

## Behavioral verification (parent runspace only — out of scope here)

The user's 7 acceptance criteria all require terminal+vision/python3 (cumulative ≥540 tirith denials in this runspace). The verify command in PENDING_COMMIT_v142.md §verify is the canonical parent-runspace recipe. Expected outcomes:

- `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Binary/Debug/TestReSTIR_GI_Temporal` → gi_raw log line shows `R[<min>,<max>]` with `min < max` and dynamic range > 5× (v141 was 2×; v142 should restore >5×, ideally ~95×)
- `dump_pixelstats.py` → gi_raw per-channel mean > 5.0 (per validate_restir_gi.py check 1)
- `validate_restir_gi.py` → passes non_black_channel_mean, spatial_std, cell_variance, alpha_sentinel (4/4)
- Fresh vision check on `display_frame8.png` → recognizable Sponza with directional shading and sane exposure

## TDD note

Per `software-development-practices §TDD Iron Law`: `NO PRODUCTION CODE WITHOUT A FAILING TEST FIRST`. v142's "failing test" is the same as v141's but in reverse: the v141 patch caused the post-v141 23:17 run to produce `R[0.0, 2.0]` (collapsed dynamic range) instead of the pre-v141 19:46 run's `R[0.9, 96.2]` (95× range). v142 reverts v141 to restore the pre-v141 behavior. The "test" is the log line that prints the per-channel range after dumping; v142 should make that line print a >5× range again.

## Routing

State machine Rule 7 matches: impl-review skipped (per `skip_impl_review: yes`), no tests, route to testing-verifier. This file IS the tester output. State machine Rule 8 matches next: route to testing-verifier.
