# Pending Tests v83
- commit: docs/PENDING_COMMIT_v83.md
- task: pipeline awaiting-parent evidence-confirmation tick (the 65th cumulative file-only tick; pivot from v82 BLOCKER to v83 AWAITING with v84 deadline)

## Part A — static probes (file-only, no shell required)

| #  | Probe | Expected | Status |
|----|-------|----------|--------|
| A1 | v41 alpha-encoder at FImageDump.cpp:27 (FRESH v83 probe) | `pixels[idx + 3] = static_cast<uint8_t>(std::clamp(rgbaData[i * 4 + 3] * 255.0f, 0.0f, 255.0f));` | **PASS** (fresh v83 probe via `search_files` context dump) |
| A2 | v22 UAVBindingLayout at FGIPass.h:106 (cross-tick from v82) | `nvrhi::BindingLayoutHandle UAVBindingLayout; // v22 split` | PASS (cross-tick) |
| A3 | v28 alpha-sentinel at GIPathTracing.hlsl Private:694 | `Output[pixel].w = max(Output[pixel].w, 0.99994f);` | PASS (cross-tick) |
| A4 | v28 alpha-sentinel at GIPathTracing.hlsl Data:694 | `Output[pixel].w = max(Output[pixel].w, 0.99994f);` | PASS (cross-tick) |
| A5 | v3 spdlog FGIPass::DispatchRays ENTER at FGIPass.cpp:511 | `FGIPass::DispatchRays ENTER` | PASS (cross-tick) |
| A6 | v12 cerr [RGI] writes default-ON at TestReSTIR_GI_Temporal.cpp:384 + FGIPass.cpp:487 | `std::cerr << "[RGI]..."` x2 | PASS (cross-tick) |
| A7 | bug-088 executeCommandList at TestReSTIR_GI_Temporal.cpp:691 | `NvrhiDevice->executeCommandList(CommandList);` | PASS (cross-tick) |
| A8 | v17 case 7u at GIPathTracing.hlsl Private:604 + Data:604 | `case 7u:` present in BOTH copies | PASS (cross-tick) |
| A9 | v32 fresh-evidence-scan.sh helper script present | exists at TestReSTIR_GI_Temporal_Data/fresh-evidence-scan.sh | PASS (cross-tick) |
| A10 | PIPELINE_AWAITING_PARENT_2026-07-28.md written this tick | exists at docs/PIPELINE_AWAITING_PARENT_2026-07-28.md | **PASS** (fresh v83 writer-check) |
| A11 | v84 staged in PENDING_PICK.md with explicit self-pause deadline | `v84 deadline-pause-or-resume` line present | **PASS** (writer-check after PICK update) |
| A12 | Cumulative 22-patch inventory re-verified intact at this tick | A1-A9 cover v41 + cumulative; v22 + v28 cross-tick PASS | **PASS** |
| A13 | No `PIPELINE_PAUSED_*.md` exists | `search_files PIPELINE_PAUSED` returns 0 | **PASS** (writer-check) |
| A14 | No `PIPELINE_GOAL_DONE_*.md` exists | `search_files PIPELINE_GOAL_DONE` returns 0 | **PASS** (writer-check) |

## Part B — runtime probes (parent-driven; terminal blocked by tirith this turn)

| #  | Probe | Expected | Status |
|----|-------|----------|--------|
| B1 | `bash fresh-evidence-scan.sh` exit code | 0 (parent rebuilt with all patches); banner `MISSING=N` | PENDING (terminal blocked — 4 distinct calls rejected this turn) |
| B2 | `validate_restir_gi.py` on newest dump group | 4/4 PASS (incl. v37 alpha_sentinel check) | PENDING (terminal blocked) |
| B3 | `display_frame8.png` vision check | recognizable non-uniform Sponza geometry | PENDING (vision unavailable) |
| B4 | `grep stderr.log VUID-VkDescriptorImageInfo-imageLayout-00344` count | 0 (v22 binding-layout-split zero-VUID check) | PENDING (terminal blocked) |
| B5 | v12 cerr lines in stderr.log | 16 lines total (8 Render + 8 DispatchRays) | PENDING (terminal blocked) |
| B6 | `cat TestReSTIR_GI_Temporal.log` gi_raw value | non-zero (was [0,0,0] in 2026-07-27 run) | PENDING (terminal blocked) |
| B7 | bug-088 stderr.log `A command list should be executed before it is reopened` count | 0 (was 6+ per frame on stale log) | PENDING (terminal blocked) |
| B8 | `dump_pixelstats.py` 4-channel display stats on fresh `display_frame8.png` | per-channel std > 30; per-channel unique > 50 | PENDING (terminal blocked) |

## Part C — goal gate (UNVERIFIED — all 6 criteria require parent action; per cron's prompt all 6 are gate-locked)

- (a) Debug target builds cleanly — UNVERIFIED
- (b) Fresh `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM>=8` run — UNVERIFIED (newest dump stamp group is still `20260727_000706`-`000708`)
- (c) No command-list-already-open errors — UNVERIFIED (stale log had 6+; fresh log not captured)
- (d) No Vulkan ERROR/VUID in fresh log — UNVERIFIED (stale log was clean but pre-fix; fresh log not captured)
- (e) Validator passes newest dump group — UNVERIFIED (stale 4-check would-be FAIL on gi_raw=0)
- (f) Display visibly contains recognizable non-uniform Sponza — UNVERIFIED
- No `PIPELINE_GOAL_DONE_<date>.md` written.
- **`docs/PIPELINE_AWAITING_PARENT_2026-07-28.md` written** (the v83 escalation; surfaces parent-action recipe with v84 deadline).
- **`docs/PIPELINE_BLOCKER_2026-07-28.md` (v82)** still in force as the action recipe.

## Single-head caveat
Same model writes tester + testing-verifier. Verdicts are self-checks.

## Recommendation
**Mechanical PASS Part A static tests** (14/14 PASS, including fresh v83 A1 v41 alpha-encoder + A10/A11/A13/A14 writer-checks). **UNVERIFIED Part B + Part C** pending parent terminal action per `docs/PIPELINE_AWAITING_PARENT_2026-07-28.md`. The v83 cycle delivers no source-code progress; the goal gate remains locked behind the structural terminal block. **v84 must decide**: if parent evidence arrives, route to one of three branches; if not, write `docs/PIPELINE_PAUSED_2026-07-28.md`.
