# Pending Tests v54

## Part A — Static textual-substitution verification (file-only, runs in this runspace)

| # | Probe | Expected | Actual |
|---|-------|----------|--------|
| A1 | `search_files pattern="near line 1531" path=Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp` | 2 hits at lines 407 + 676 | 2 hits ✅ |
| A2 | `search_files pattern="near line 1531" path=Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/fresh-evidence-scan.sh` | 1 hit at line 60 (was "near line 1521") | 1 hit ✅ |
| A3 | `search_files pattern="near line 1516" path=Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp` | 0 hits (was 2 — lines 407 + 676 prior to mid-flight patch) | 0 hits ✅ (mid-flight patch discovered by this probe — see COMMIT deviations section) |
| A4 | `search_files pattern="near line 1521" path=Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/fresh-evidence-scan.sh` | 0 hits (was 1, line 60) | 0 hits ✅ |
| A5 | `search_files pattern="^\\- \\[x\\] v54" path=docs/PENDING_PICK.md` | 2 hits (v53 entry at line 187 + v54 entry at line 170) | 2 hits ✅ |
| A6 | `search_files pattern="^\\- \\[ \\] v54" path=docs/PENDING_PICK.md` | 1 hit (newly-staged v54 standby at line 188-189) | 1 hit ✅ |

A1-A4 verify the textual replacements landed correctly.
A5-A6 verify the PICK state machine is consistent (v53 marked done, v54 staged).

## Part B — Test surface checks (file-only, runs in this runspace)

| # | Check | Expected | Actual |
|---|-------|----------|--------|
| B1 | Cumulative 21-patch inventory still intact | 21/21 sites present | 21/21 ✅ (verified by fresh search_files probes this tick at FGIPass.h:106, FGIPass.cpp:183/311/612, FImageDump.cpp:27, FGIPass.cpp:487, GIPathTracing.hlsl:593+604+694 in BOTH copies) |
| B2 | `fresh-evidence-scan.sh` CHECKS array still has 27 entries | 27 | 27 ✅ (22 pre-v43 + 5 added by v43 at lines 81-85) |
| B3 | `dump_pixelstats.py` v40 alpha-classification still present | functions `compute_alpha_stats`, `classify_alpha_sentinel` + `[v40-alpha]` print | ✅ (verified at dump_pixelstats.py:96 + 187/190/193/197/201) |
| B4 | `validate_restir_gi.py::check_alpha_sentinel` still present | function at line 134 | ✅ (verified at validate_restir_gi.py:134; referenced in fresh-evidence-scan.sh CHECKS line 81) |

## Part C — Acceptance criteria (parent-driven; terminal blocked in cron)

| # | Criterion | Status |
|---|-----------|--------|
| C1 | (a) Debug build cleanliness | UNVERIFIED (terminal blocked) |
| C2 | (b) Fresh HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM>=8 run | UNVERIFIED |
| C3 | (c) No "Cannot open a command list that is already open" warnings | UNVERIFIED |
| C4 | (d) No Vulkan ERROR / VUID-VkDescriptorImageInfo-imageLayout-00344 | UNVERIFIED |
| C5 | (e) `validate_restir_gi.py` passes newest dump group | UNVERIFIED |
| C6 | (f) Visual recognition of sane-exposure non-uniform Sponza geometry | UNVERIFIED |

Part C acceptance criteria are parent-driven per cron-prompt "do not silently stop" instruction and gpu-rendering-bisect-debug playbook. v54 is a documentation-only patch; it does NOT change C1-C6 status.

## Verdict
ALL_KEEP contingent on Part A + Part B passing in this runspace.
