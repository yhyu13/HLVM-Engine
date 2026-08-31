# Pending Tests v43 — fresh-evidence-scan.sh 17→21 patch inventory extension

## Test status by part

### Part A — Static verification (this tick, file-only)

- A1: 4 new file variables (VALIDATOR_PY, DECODE_V38_PY, DUMP_PIXELSTATS_PY, FIMAGEDUMP_CPP) added to script → PASS (verified via read_file lines 41-47)
- A2: 4 new case statement branches added → PASS (verified via read_file lines 101-105)
- A3: 5 new CHECKS entries appended → PASS (verified via read_file lines 80-85)
- A4: Script header bumped from v32 to v43 attribution → PASS (verified via read_file line 49)
- A5: All 22 prior entries unchanged → PASS (verified via read_file lines 57-79)
- A6: Script syntax is valid bash → UNVERIFIED (cron is file-only; cannot `bash -n` the script)
- A7: v37 pattern `def check_alpha_sentinel` matches validate_restir_gi.py:134 → PASS (search_files confirmed 1 match)
- A8: v38 pattern `DebugMode effective=` matches FGIPass.cpp:487 → PASS (search_files confirmed 1 match)
- A9: v39 pattern `decode_v38_evidence` matches decode_v38_evidence.py:22 → PASS (search_files confirmed 1 match)
- A10: v40 pattern `v40-alpha` matches dump_pixelstats.py:187 → PASS (search_files confirmed 1 match)
- A11: v41 pattern `rgbaData\[i \* 4 \+ 3\] \* 255.0f` matches FImageDump.cpp:27 → PASS (search_files confirmed 1 match)

**Part A: 10/10 PASS, 1/1 UNVERIFIED (A6 bash-syntax requires terminal)**

### Part B — Runtime verification (parent-driven, terminal blocked by tirith)

- B1: Parent runs `bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/fresh-evidence-scan.sh` → expected MISSING=0 across all 27 CHECKS entries (was 22 pre-v43)
- B2: Banner emits `BANNER: source-patch-missing` if any of v37/v38/v39/v40/v41 patches regress → expected PASS (was silently PASS pre-v43)
- B3: Reverting any of v37/v38/v39/v40/v41 patches triggers MISSING=N>0 → expected correct detection (was undetected pre-v43)

**Part B: 3/3 UNVERIFIED (terminal blocked)**

### Part C — Goal gate (unchanged from prior ticks)

- C1: Debug target builds cleanly → UNVERIFIED
- C2: Fresh `HLVM_DUMP_RGI=1` run with `HLVM_RGI_ACCUM>=8` → UNVERIFIED
- C3: No command-list-already-open errors → UNVERIFIED
- C4: No Vulkan ERROR/VUID in fresh log → UNVERIFIED
- C5: Validator passes newest dump group → UNVERIFIED
- C6: Display visibly contains recognizable non-uniform Sponza → UNVERIFIED

**Part C: 6/6 UNVERIFIED (parent-driven)**

## Test summary

- Part A (static): 10/10 PASS, 1 UNVERIFIED
- Part B (runtime): 3/3 UNVERIFIED
- Part C (goal gate): 6/6 UNVERIFIED

## Recommendation

PASS Part A. UNVERIFIED Part B and Part C (parent-driven). v43 closes the file-only diagnostic-surface completeness gap by extending the script's patch inventory from 22 to 27 entries covering all 21 cumulative pipeline patches (v3-v41 + bug-088 + bug-075).