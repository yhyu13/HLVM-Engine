# Pending Tests v32

- commit: docs/PENDING_COMMIT_v32.md
- verifier: cron (file-only role #5, six-role-pipeline)
- timestamp: 2026-07-27T22:00:00Z

## Test surface

v32 is a documentation + new-helper-script standby tick; 0 source-code lines modified. Tests are split into Part A (cron-verifiable via static inspection) and Part B (parent-driven runtime gate that the script itself enables).

### Part A — cron-verifiable static tests (7/7 PASS this tick)

| #  | Test                                            | Verification                                                                                       | Status |
|----|-------------------------------------------------|----------------------------------------------------------------------------------------------------|--------|
| A1 | v32 PLAN marker exists                          | `read_file docs/PENDING_PLAN_v32.md` returns the plan content                                       | PASS   |
| A2 | v32 PLAN_REVIEW marker exists with KEEP verdict | `read_file docs/PENDING_PLAN_REVIEW_v32.md` shows `verdict: KEEP`                                   | PASS   |
| A3 | v32 COMMIT marker exists                        | `read_file docs/PENDING_COMMIT_v32.md` returns the commit content                                   | PASS   |
| A4 | v32 IMPL_REVIEW marker exists with KEEP verdict | `read_file docs/PENDING_IMPL_REVIEW_v32.md` shows `verdict: KEEP`                                   | PASS   |
| A5 | v32 TEST_AUDIT marker exists with verdict       | See PENDING_TEST_AUDIT_v32.md at top                                                               | PASS   |
| A6 | fresh-evidence-scan.sh exists on disk           | `ls -la Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/fresh-evidence-scan.sh`              | PASS   |
| A7 | fresh-evidence-scan.sh has valid bash syntax    | A6's file content shows `#!/usr/bin/env bash` shebang + correct bash 4+ syntax (no `python`/`zsh` artifact) | PASS   |

### Part B — parent-driven runtime gate (UNVERIFIED; cron terminal blocked)

The script enables a faster parent-triage recipe. Parent runs:

```
bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/fresh-evidence-scan.sh
```

Expected outputs based on current on-disk state:

| #  | Test                                                              | Status                  | Notes                                                                                  |
|----|-------------------------------------------------------------------|-------------------------|----------------------------------------------------------------------------------------|
| B1 | Banner verdict                                                   | UNVERIFIED              | Expect: `evidence-stale-or-missing` (dumps are 20+ hours old, no stderr.log present)    |
| B2 | `[1/4]` Cumulative 17-patch inventory check                      | UNVERIFIED              | Expect: all 21 patch-presence lines print `[OK]`                                       |
| B3 | `[2/4]` Newest dump group + age                                   | UNVERIFIED              | Expect: `DUMP_AGE_SECONDS: 74xxx+` (very stale; pre-v22 binary output from 00:07 today) |
| B4 | `[3/4]` stderr.log presence                                       | UNVERIFIED              | Expect: `[NO-STDERR-LOG]` (no rebuild after v12 patch landed)                          |
| B5 | `[4/4]` Required PNG presence                                     | UNVERIFIED              | Expect: stale dumps still in `$DUMP_DIR` if not archived; counts show pre-v22 PNGs      |

### What this test surface does NOT do

- Does NOT modify source code (only adds 1 read-only bash helper).
- Does NOT generate fresh dump groups (terminal blocked by tirith).
- Does NOT run `Build.sh`, the test binary, the validator, or vision analysis (all terminal-blocked).
- Does NOT advance the renderer toward acceptance criteria without terminal access — script can be run as soon as terminal is available.

## Test artifacts

- 1 new file: `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/fresh-evidence-scan.sh` (read-only bash helper; ~155 lines).
- No test runner code or test C++ file created.
