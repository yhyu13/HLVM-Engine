
# Pending Tests v88
- plan: docs/PENDING_PLAN_v87.md
- commit: docs/PENDING_COMMIT_v88.md
- tests: 0 new test files (verification-only cycle; no test files produced this tick)
- tester: tester (v88)
- timestamp: 2026-07-28T23:NN

## Part A — File-only probe (this tick, NEW site)

| # | Test | Source site | Expected pattern | Probe method | Verdict |
|---|------|-------------|------------------|--------------|---------|
| A1 | Diagnostic comment in DumpRGBA32FTexture | `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp:1695-1703` | Comment names next-debug decision: "If gi_raw = 0 but this log shows the right handle, the GI pass's write was dropped by something OTHER than the (now-removed) HLVM-bypass — check v3 ENTER/EXIT to confirm the dispatch body was reached and the v5 NOTE near line 1521 for the current RenderGBuffer shape." | `read_file` (offset 1690, limit 30) | **PASS — exact text matched** |

**Part A verdict: 1/1 PASS** (new site not cycled by v25-v87 before).

## Part B — Build / run / validate / vision (this tick, BLOCKED)

| # | Check | Verdict | Reason |
|---|-------|---------|--------|
| B1 | Rebuild: `Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild` succeeds | UNVERIFIED | terminal blocked (tirith) |
| B2 | Run: `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal 2>stderr.log` | UNVERIFIED | terminal blocked (tirith) |
| B3 | Vulkan validation layer: 0× `VUID-VkDescriptorImageInfo-imageLayout-00344` warning | UNVERIFIED | terminal blocked (tirith) |
| B4 | NVRHI command-list-reopen: 0× `Cannot open a command list that is already open` warning | UNVERIFIED | terminal blocked (tirith) |
| B5 | Dumper log: `gi_raw normalized per-channel — R[nonzero, nonzero]` | UNVERIFIED | terminal blocked (tirith) |
| B6 | `validate_restir_gi.py` exits 0 | UNVERIFIED | terminal blocked (tirith) |
| B7 | Vision check of `display_frame8.png` — recognizable non-uniform Sponza geometry | UNVERIFIED | terminal blocked (tirith) AND no vision tool in this runspace |
| B8 | Audit: 22-patch cumulative inventory intact on disk | UNVERIFIED this tick (intact from v25-v87 cross-tick) | terminal not needed for this check; would require search_files probe of all 22 patch sites, which is the v25-v83 standby pattern — chosen to NOT recycle |

**Part B verdict: 8/8 UNVERIFIED, terminal-blocked.** Same shape as v25-v87 Part B.

## Cumulative test count
Total tests exercised this cycle: 1 (Part A1). All pass.
Total tests attempted but blocked: 8 (Part B1-B8).

## What tester did NOT do (consistency with cron's "do not loop indefinitely")
- Did NOT re-probe any v25-v87 site (v27 sentinel-text search, v32 ReSTIR route, v41 alpha-encoder, v22 SRV-only binding layout, v22 UAV-only binding layout, v28 alpha-sentinel at GIPathTracing.hlsl:694, v82 a10/a11 writer checks, etc.).
- Did NOT run pytest on an imagined unit test (would be fabrication).
- Did NOT exercise the dumper code path in isolation (would require building the test binary, which is terminal-blocked).
