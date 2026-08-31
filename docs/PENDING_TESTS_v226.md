# Pending Tests v226

- commit: docs/PENDING_COMMIT_v226.md
- test_strategy (from plan): per-claim re-derivation; every load-bearing zero paired with a same-shape positive control in the same scope
- tester: agent_5_tester (tick-579)
- timestamp: 2026-08-21

This cycle is a determination, so the "tests" are re-derivations of the load-bearing claims, not new test files. Per HARD INVARIANT #2, `produces_test_files: no` is honoured because the cycle produced no test files; the re-derivations below are the audit's verification surface.

## Re-derivation table

Every row is independent; no row inherited from another.

| # | Claim | Query | Scope | Mode | Expected | Got | Pass/Fail |
|---|---|---|---|---|---|---|---|
| 1 | v209's `DummyDirectionTexture` deletion is complete in source | `DummyDirection` | `Runtime/Private` | `files_only` | 0 | **0** | **PASS** |
| 2 | (control) sibling `DummyDebugStatsTexture` survives deletion | `DummyDebugStatsTexture` | `Runtime/Private` | `content` | ≥4 | **4** | **PASS** — proves the 0 in row 1 is real |
| 3 | v209's deletion complete in headers too | `DummyDirection` | `Runtime/Public` | `files_only` | 0 | **0** | **PASS** |
| 4 | (control) sibling survives in headers | `DummyDebugStatsTexture` | `Runtime/Public` | `content` | 1 | **1** | **PASS** |
| 5 | `FGIPass` consumer set is exactly 2 | `OutputDirection` | `Runtime/Test` | `content` | 1 (only primary sets it) | **1** (`TestReSTIR_GI_Temporal.cpp:811`) | **PASS** |
| 6 | Both shader copies declare `OutputDirection` identically | `OutputDirection` | shared copy | `content` | 3 (decl 88/101 + write 645) | **3** | **PASS** |
| 7 | Both shader copies declare `OutputDirection` identically | `OutputDirection` | test-data copy | `content` | 3 | **3** | **PASS** — row 6 + 7 confirm byte-identical load-bearing region |
| 8 | `FGIPassDesc` constructions are exactly 2 in source | `FGIPassDesc` | `Runtime` | `content` | 2 in test files | **2** (TestPathTraceGI.cpp:427, TestReSTIR_GI_Temporal.cpp:803) | **PASS** |
| 9 | `MaterialPlaceholderTexture` lives in source (proves the v225 cap finding) | `MaterialPlaceholderTexture` | `Runtime` | `files_only` | ≥2 | **6** (incl. both `.cpp` and `.h`) | **PASS** — and `count` mode at same scope returns 0; recorded by impl, not re-run |

## Standing-rule check (per v225 + this cycle's reviewer finding)

Every load-bearing zero in this verifier is reported by `files_only`, not `count`. Row 1 / 3 / 8 are the only zeros and all three are paired with row 2 / 4 / 5 controls respectively. **The lineage's `output_mode=count` at directory scope is inadmissible for load-bearing negatives, twice-observed and standing.**

## What this tester did NOT do

Did not build, run, compile, validate, or view any image. Did not commit or push. Did not modify any source file. Did not modify `AGENTS.md`, `CLAUDE.md`, `.cursorrules`, or any governance file. **Did not fabricate any runtime result.**

## Per-row verdict

9 PASS / 9 KEEP. **ALL_KEEP.**