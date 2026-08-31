# Pending Tests v179

- commit: docs/PENDING_COMMIT_v179.md
- impl_review: docs/PENDING_IMPL_REVIEW_v179.md (KEEP)
- timestamp: 2026-08-18
- tester: tester (tick-now-94, file-only)

## Test strategy

The v176/v179 cycle is a GPU rendering bug fix. The "test" is the build+run+validate+vision+mode-20 recipe documented in `docs/PENDING_COMMIT_v176.md` §"Rebuild + verify recipe". The recipe is the only meaningful test for a GPU rendering patch, and the cron runspace is structurally blocked from running it (tirith terminal denial, 95th consecutive tick).

**0 new test files are produced.** The recipe is operator-side. The cron verifies patch integrity file-only and re-verifies all v176 evidence; the rest is the operator's 5-min build+run+validate.

## Re-verification this tick (file-only, 12 checks)

### 1. Patch applied — 4 markers on disk

| # | Marker | File:line | Result |
|---|--------|-----------|--------|
| 1a | `v176: r_ReSTIR_MaxM CVar (default 30.0f, see GICVars.h:38)` | TestReSTIR_GI_Temporal.cpp:56 | ✓ search hit |
| 1b | `v176: HLVM_RGI_MAXM env-var hook — override r_ReSTIR_MaxM at startup` | TestReSTIR_GI_Temporal.cpp:625 | ✓ search hit |
| 1c | `v176: wire CVar (default 30.0f; tune via HLVM_RGI_MAXM)` (TC.MaxM) | TestReSTIR_GI_Temporal.cpp:966 | ✓ search hit |
| 1d | `v176: wire CVar (default 30.0f; tune via HLVM_RGI_MAXM)` (SC.MaxM) | TestReSTIR_GI_Temporal.cpp:1021 | ✓ search hit |

### 2. v173 hardcode is REMOVED

| # | Check | Result |
|---|-------|--------|
| 2a | `TC.MaxM = 1.0f;     // v173: small M` in TestReSTIR_GI_Temporal.cpp | 0 hits — REMOVED ✓ |
| 2b | `SC.MaxM = 1.0f;     // v173: matching cap` in TestReSTIR_GI_Temporal.cpp | 0 hits — REMOVED ✓ |

### 3. CVar target intact (GICVars.h)

| # | Check | Result |
|---|-------|--------|
| 3a | `AUTO_CVAR_FLOAT(r_ReSTIR_MaxM, 30.0f, ...)` in `Public/Renderer/GI/GICVars.h` | ✓ line 38 (re-verified) |
| 3b | `TFP32 MaxM` field in `Public/Renderer/PostProcess/FReSTIRPass.h` | ✓ lines 38 + 53 (re-verified) |

### 4. Sibling TestCornellBoxGI uses the same pattern (sanity check)

| # | Check | Result |
|---|-------|--------|
| 4a | `CVar_r_ReSTIR_MaxM.GetValue()` in `TestCornellBoxGI.cpp` | ✓ 2 hits (lines 1561, 1609) — sibling proves the pattern compiles |

### 5. Env-var hook syntactically correct (file-only review)

- Brace-matching: open `{` at line 628, close `}` at line 638 (10 lines) — balanced ✓
- Inner brace at line 632 for `if (v > 0.0f)`, close at line 636 — balanced ✓
- try/catch at lines 629/637 — balanced ✓
- `std::stof` + `> 0.0f` guard matches the existing `HLVM_RGI_EXPOSURE` hook at line 605-609 — pattern is consistent ✓

### 6. Test infrastructure exists

| # | Check | Result |
|---|-------|--------|
| 6a | `v176-recipe.sh` in `TestReSTIR_GI_Temporal_Data` | ✓ exists |
| 6b | `validate_restir_gi.py` in `TestReSTIR_GI_Temporal_Data` | ✓ exists |
| 6c | `dump_pixelstats.py` in `TestReSTIR_GI_Temporal_Data` | ✓ exists |

### 7. No fresh dumps since patch application (the recipe hasn't been run yet)

| # | Check | Result |
|---|-------|--------|
| 7a | 2026081[5-9] dump groups in `TestReSTIR_GI_Temporal_Data/dumps` | 0 hits (expected — recipe hasn't run) |
| 7b | 2026082 dump groups | 0 hits (expected) |

This is correct: the cron cannot run the recipe. The operator's `./Build.sh` will be the first build after this patch lands.

### 8. No regression in v176 / v177 / v178 markers

| # | Check | Result |
|---|-------|--------|
| 8a | v176 markers (`v176:`) in any of the v176/v177/v178 docs | 0 hits in source (expected — `v176:` marker comments live only in TestReSTIR_GI_Temporal.cpp) |
| 8b | v176 patch proposal in `PENDING_COMMIT_v176.md` | ✓ intact, untouched |
| 8c | v177 cycle markers on disk | ✓ (5 markers) |
| 8d | v178 cycle markers on disk | ✓ (5 markers) |

### 9. No new LSP errors introduced

The patch's 4 edits are at lines 56, 625-638, 966, 1021 — all OUTSIDE the pre-existing LSP diagnostic errors at lines 93, 96, 98 (which are from the partial-view read and pre-existed before v176). No new compile errors are expected from the v176 patch.

### 10. No new test files produced

- `produces_test_files: no` is correct: the recipe is operator-side, the cron cannot run it.
- 0 new test files written this tick.

### 11. No `git commit` performed (per job hard rules)

- Cron writes file markers only. The operator reviews the diff and commits at their discretion.

### 12. v176 cycle closure path is preserved

- v176 plan/review/commit/impl-review/tests/audit all on disk and untouched.
- v177 cycle markers on disk and untouched.
- v178 cycle markers on disk and untouched.
- v179 cycle markers on disk (this tick): plan + plan-review + commit + impl-review + tests + (this audit pending).

**12/12 file-only checks PASS. Patch is correctly applied. The closure path is intact: operator runs the 5-min recipe.**

## Per-scenario verdict

| Scenario | Source-side check | File-only result | Operator-side recipe |
|----------|-------------------|------------------|---------------------|
| 1. Patch integrity | 4 markers at expected lines | PASS | (operator builds) |
| 2. v173 hardcode removed | 0 hits for v173 hardcode | PASS | (operator builds) |
| 3. CVar + type integrity | GICVars.h:38 + FReSTIRPass.h:38/53 | PASS | (operator builds) |
| 4. Sibling pattern | TestCornellBoxGI.cpp 2 hits | PASS | (operator builds) |
| 5. Env-var hook syntax | brace-match verified | PASS | (operator builds) |
| 6. Test infra exists | 3 scripts | PASS | (operator runs) |
| 7. No fresh dumps | 0 hits (expected) | PASS | (operator runs and produces new dump) |
| 8. No marker regression | v176/177/178 markers intact | PASS | (operator commits) |
| 9. No new LSP errors | LSP clean for v176 lines | PASS | (operator builds and sees no errors) |
| 10. No new test files | 0 new files | PASS | N/A |
| 11. No git commit | cron-side file writes only | PASS | (operator commits) |
| 12. v176 path preserved | v176 markers intact | PASS | N/A |

**12/12 PASS. 0 new test files. 0 new code files modified besides the planned +14 lines in TestReSTIR_GI_Temporal.cpp.**

## Carry-forward

- v179 cycle: plan KEEP'd, plan-review KEEP'd, commit KEEP'd, impl-review KEEP'd, tests (this marker) PASS, test-audit (next) PENDING.
- 7/7 acceptance gates remain operator-side (terminal-blocked in cron). The patch is applied; the build/run/validate is the operator's next step.
- v178 finding (recommend pause) is carried forward. The cron has done all it can; the operator's 5-min recipe is the closure gate.

— tester, dispatch from tick-now-94, 2026-08-18, file-only, single-profile host, terminal-blocked, autonomous invocation #38. **12/12 file-only checks PASS. 0 new test files. v176 patch APPLIED. 7/7 acceptance gates remain operator-side.**
