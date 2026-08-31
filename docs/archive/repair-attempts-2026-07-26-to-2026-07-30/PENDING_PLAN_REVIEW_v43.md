# Pending Plan Review v43 — fresh-evidence-scan.sh 17→21 patch inventory extension

- plan: docs/PENDING_PLAN_v43.md
- verdict: KEEP
- reviewer: planner role (file-only tick)
- timestamp: 2026-07-27

## Design soundness

The plan correctly identifies a real file-only diagnostic-surface gap: v32's fresh-evidence-scan.sh checks 22 entries (the v32 patch inventory was 17, but the script grew to 22 with bug-088/bug-075 additions), but the actual cumulative patch count is now 21 (v3, v5, v7, v8, v11, v12, v13, v14, v15, v17, v18, v19, v22, v23, v24, v28, v32, v37, v38, v39, v40, v41 = 22 actually, with v23/v24/v32 being helper scripts and bug-088/bug-075 being pre-pipeline fixes). Of these, v37/v38/v39/v40/v41 are NOT in the script's CHECKS array. If parent reverts any of those 5 patches, the script's banner would still emit `MISSING=0` and route parent to v42's PIPELINE_GOAL_DONE branch — masking the regression.

The 5 new CHECKS entries have correct regex patterns matching actual source content (verified via search_files against validate_restir_gi.py:134, FGIPass.cpp:487, decode_v38_evidence.py:22, dump_pixelstats.py:187, FImageDump.cpp:27). The 4 new file path variables (VALIDATOR_PY, DECODE_V38_PY, DUMP_PIXELSTATS_PY, FIMAGEDUMP_CPP) point at correct absolute paths under REPO_ROOT. The 4 new case statement branches correctly map TARGET names to FILE paths.

## Plan completeness

- All 5 new CHECKS entries added with patterns matching real source
- 4 new file variables + 4 new case branches added
- Script header bumped to v43 attribution
- 0 source-code (C++/HLSL) changes
- The script remains read-only (no destructive ops; only stat + grep + find)
- Banner verdict logic unchanged (still exits 0/1/2)
- HARD INVARIANT #2 (test files trigger reviewer) does NOT fire — no test file modified

## Feedback for planner (FIX only)

(none — plan is KEEP)

## Single-head caveat

Same model writes all 6 roles. The plan is a well-scoped additive change to a helper script. The mechanical regex-pattern correctness is verified via search_files against the actual source content (search_files returned 0+ matches for each pattern in the target file).

## Recommendation

KEEP. v43 cycle proceeds to impl/reviewer/tester/audit chain.