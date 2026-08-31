# Pending Tests v82
- commit: docs/PENDING_COMMIT_v82.md
- task: pipeline blocker-handoff tick (the 64th cumulative file-only tick; pivot from standby loop to evidence-gated escalation)

## Part A — static probes (file-only, no shell required)

| #  | Probe | Expected                                                                                        | Status |
|----|-------|-------------------------------------------------------------------------------------------------|--------|
| A1 | v22 UAVBindingLayout at FGIPass.h:106 (FRESH v82 probe) | `nvrhi::BindingLayoutHandle UAVBindingLayout; // v22 split: separate layout for u0/u1 UAVs` | **PASS** (fresh v82 probe via `search_files`) |
| A2 | v28 alpha-sentinel at GIPathTracing.hlsl Private:694  | `Output[pixel].w = max(Output[pixel].w, 0.99994f);` | PASS (v81 cross-tick) |
| A3 | v28 alpha-sentinel at GIPathTracing.hlsl Data:694     | `Output[pixel].w = max(Output[pixel].w, 0.99994f);` | PASS (v81 cross-tick) |
| A4 | v3 spdlog FGIPass::DispatchRays ENTER at FGIPass.cpp:511 | `HLVM_LOG ... info ... FGIPass::DispatchRays ENTER` | PASS (v35 cross-tick) |
| A5 | v12 cerr [RGI] writes default-ON at TestReSTIR_GI_Temporal.cpp:384 + FGIPass.cpp:487 | `std::cerr << "[RGI]..."` x2 | PASS (v35 cross-tick) |
| A6 | bug-088 executeCommandList at TestReSTIR_GI_Temporal.cpp:691 | `NvrhiDevice->executeCommandList(CommandList);` | PASS (v79 cross-tick) |
| A7 | v17 case 7u at GIPathTracing.hlsl Private:604 + Data:604 | `case 7u:` present in BOTH copies | PASS (v79 cross-tick) |
| A8 | v41 std::clamp alpha-encoder at Private/Image/FImageDump.cpp:27 | `std::clamp(rgbaData[i*4+3] * 255.0f, 0.0f, 255.0f)` | PASS (v79 cross-tick) |
| A9 | v32 fresh-evidence-scan.sh helper script present | exists at TestReSTIR_GI_Temporal_Data/fresh-evidence-scan.sh | PASS (v35 cross-tick) |
| A10 | PIPELINE_BLOCKER_2026-07-28.md written this tick | exists at docs/PIPELINE_BLOCKER_2026-07-28.md, 8 KB+, contains "Minimum parent actions" section | **PASS** (fresh v82 writer-check) |
| A11 | v83 staged in PENDING_PICK.md as evidence-gated continuation | `v83 evidence-gated continuation` line present | **PASS** (fresh v82 writer-check) |
| A12 | Cumulative 22-patch inventory re-verified intact | all 22 sites still pass the file-only spot-check | **PASS** (v82 fresh re-audit) |

## Part B — runtime probes (parent-driven; terminal blocked by tirith this turn)

| #  | Probe | Expected | Status |
|----|-------|----------|--------|
| B1 | `bash fresh-evidence-scan.sh` exit code | 0 (parent rebuilt with all patches); the script's banner is `MISSING=0 evidence-stale-or-missing` | PENDING (terminal blocked) |
| B2 | `validate_restir_gi.py` on newest dump group | 4/4 PASS (incl. v37 alpha_sentinel check) | PENDING (terminal blocked) |
| B3 | `display_frame8.png` vision check | recognizable non-uniform Sponza geometry | PENDING (vision unavailable) |
| B4 | `grep stderr.log VUID-VkDescriptorImageInfo-imageLayout-00344` count | 0 (v22 binding-layout-split zero-VUID check) | PENDING (terminal blocked) |
| B5 | v12 cerr lines in stderr.log | 16 lines total (8 Render + 8 DispatchRays) | PENDING (terminal blocked) |
| B6 | `cat TestReSTIR_GI_Temporal.log` gi_raw value | non-zero (was [0,0,0] in 2026-07-27 run) — per gpu-rendering-bisect-debug skill smoking-gun check | PENDING (terminal blocked) |
| B7 | bug-088 stderr.log `A command list should be executed before it is reopened` count | 0 (was 6+ per frame on stale log) | PENDING (terminal blocked) |
| B8 | `dump_pixelstats.py` 4-channel display stats on fresh display_frame8.png | per-channel std > 30; per-channel unique > 50 | PENDING (terminal blocked) |

## Part C — goal gate (UNVERIFIED — all 6 criteria require parent action; per cron's prompt all 6 are gate-locked)

- (a) Debug target builds cleanly — UNVERIFIED
- (b) Fresh `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM>=8` run — UNVERIFIED (newest dump stamp group is still `20260727_000706`–`000708`)
- (c) No command-list-already-open errors — UNVERIFIED (stale log had 6+; fresh log not captured)
- (d) No Vulkan ERROR/VUID in fresh log — UNVERIFIED (stale log was clean but pre-fix; fresh log not captured)
- (e) Validator passes newest dump group — UNVERIFIED (stale 4-check would-be FAIL on gi_raw=0)
- (f) Display visibly contains recognizable non-uniform Sponza — UNVERIFIED
- No `PIPELINE_GOAL_DONE_<date>.md` written.
- **`docs/PIPELINE_BLOCKER_2026-07-28.md` written** (the v82 escalation; surfaces parent-action recipe).

## Single-head caveat
Same model writes tester + testing-verifier. Verdicts are self-checks.

## Recommendation
**Mechanical PASS Part A static tests** (12/12 PASS, including fresh v82 A1 v22 UAVBindingLayout probe + A10/A11 writer-checks for the blocker document and PICK pivot). **UNVERIFIED Part B + Part C** pending parent terminal action per `docs/PIPELINE_BLOCKER_2026-07-28.md`. The v82 cycle delivers no source-code progress; the goal gate remains locked behind the structural terminal block. **Cron pipeline should not enter a v83 standby loop** without parent evidence.
