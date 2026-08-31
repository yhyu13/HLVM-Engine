# Pending Tests v28

- commit: docs/PENDING_COMMIT_v28.md
- verifier: cron (file-only role #5, six-role-pipeline)
- timestamp: 2026-07-27T18:00:00Z

## Test surface

v28 is a diagnostic-surface expansion cycle; +12 lines per HLSL copy. Tests are split into Part A (cron-verifiable via static inspection) and Part B (parent-driven; terminal required and blocked in cron by tirith).

### Part A — cron-verifiable static tests (5/5 PASS this tick)

| # | Test | Verification | Status |
|---|------|--------------|--------|
| A1 | v28 sentinel write present in Private master | read_file at offset 681+ shows `Output[pixel].w = max(Output[pixel].w, 0.99994f);` | PASS |
| A2 | v28 sentinel write present in data-dir copy | read_file at offset 681+ shows identical sentinel | PASS |
| A3 | v28 comment block correctly documents sentinel | read_file shows 10-line comment explaining sentinel purpose + bit-pattern prediction | PASS |
| A4 | v28 patch is byte-identical between HLSL copies | `diff -u` shape shows 0 meaningful differences; both have sentinel at the same line | PASS |
| A5 | v28 patch does NOT regress any prior case | Both HLSL copies still have case 1u-15u + default at lines 579-677 | PASS |

### Part B — parent-driven runtime tests (1 PENDING)

| # | Test | Status | Notes |
|---|------|--------|-------|
| B1 | display_frame8.png alpha-channel inspection | PENDING | parent-driven; after rebuild, vision-analyze the alpha channel of display_frame8.png. If alpha saturated to 254-255 across all pixels → dispatch body reached line 682+ → bug is downstream of alive-sentinel. If alpha uniformly 0 → dispatch body never executed → bug is upstream (binding layout / descriptor / dispatch setup). |

## What this test surface does NOT do
- Does NOT generate fresh dump groups (terminal blocked by tirith).
- Does NOT run `Build.sh` (terminal blocked).
- Does NOT run `validate_restir_gi.py` (terminal blocked).
- Does NOT vision-analyze images (no vision tool).
- Does NOT advance the renderer toward acceptance criteria without terminal access.

## Test artifacts

None produced (diagnostic-surface expansion only; no test files created).