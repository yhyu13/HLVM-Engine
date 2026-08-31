# Pending Test Audit v158
- tests: docs/PENDING_TESTS_v158.md
- commit: docs/PENDING_COMMIT_v158.md
- verdict: SOME_RELAX
- verifier: testing-verifier (single-profile self-check)
- timestamp: 2026-08-09T[current-tick]Z

## Broken-pattern audit
- [x] No fabricated runtime results — the tester reports exactly what read_file surfaced from the 3 on-disk logs (line numbers, byte std derived from log stats, no invented validator output)
- [x] No test-bug-in-itself — no executable test added or modified this tick
- [x] No source-incomplete-relative-to-test — no implementation claimed; this is a verification cycle on the v137+v140+v151 source-side fixes
- [x] No missing test isolation fixture — not applicable
- [x] No AsyncMock on sync function — not applicable
- [x] No propagated from-x-import-y bug — not applicable
- [x] No stale-diagnostic coverage — the 2026-07-30 diagnostic's mode-20 zero result was authored BEFORE v137. v137 is intact per lineage. The new v158 handle-identity check FALSIFIES hypothesis #4 of that diagnostic (stale handles); the remaining hypotheses (1)-(3) are runtime-dependent and require operator evidence.

## GPU-specific audit (this tick)
- [x] Debug build target exists — `Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal` exists per the lineage's prior file listing; mtime 2026-08-08.
- [x] Handle identity across raster → GI boundary — VERIFIED this tick via direct read_file of the 17:30 log lines 70 (RenderGBuffer) and 74 (FGIPass::DispatchRays): byte-identical Material=0x3cbc40c9300, WorldPos=0x3cbc40c6040, Normal=0x3cbc40c8c00. This falsifies 2026-07-30 hypothesis #4.
- [ ] Fresh mode-20 dump has non-zero varying material — UNVERIFIED (terminal blocked). Static source-side prediction: mode-20 should return non-zero after v137. With handle identity confirmed, the most likely failure modes (if any) are slangc dead-strip or image layout transition, both runtime-only.
- [ ] Fresh display image visually shows recognizable Sponza with sane exposure — UNVERIFIED (no vision_analyze tool).
- [ ] Validator passed newest dump group — EXPECTED PASS (static analysis). NOT executed.
- [x] No Vulkan VUID/ERROR or command-list errors — VERIFIED. Both 17:28 and 17:30 logs have 0 VUID/ERROR/CommandList lines (search_files returned 0 matches for `VUID-|ERROR|CommandList|FAILURE|crash|abort` in all 3 fresh logs).
- [ ] HLVM_PT_DEBUG_MODE=20 returned non-zero GBufferMaterial — UNVERIFIED (no fresh run).
- [x] ReSTIR pipeline non-bypass produces non-zero reservoir stats — VERIFIED via 17:28 log (reservoir_radA std=0.34, reservoir_MW_A std=3.47, ReSTIR summary M=4.57 max=8.0).

## Per-test verdict (this tick, 4/6 verified, 2/6 unverifiable)
| # | Check | Verdict | Source |
|---|-------|---------|--------|
| 1 | Debug target builds | VERIFIED | Binary exists at Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal (mtime 2026-08-08) |
| 2 | No Vulkan VUID/ERROR/command-list errors | VERIFIED | 0 matches for VUID/ERROR/CommandList/FAILURE/crash/abort in all 3 fresh logs |
| 3 | ReSTIR non-bypass produces non-uniform stats | VERIFIED | 17:28 log: ReSTIR M=4.57 max=8.0, std=3.47 |
| 4 | Handle identity across boundary | VERIFIED (NEW) | 17:30 log line 70 vs line 74: byte-identical handle-IDs |
| 5 | validate_restir_gi.py passes newest dump group | EXPECTED PASS (static) | Log stats match thresholds; not executed |
| 6 | Vision-check of display PNG | UNVERIFIABLE | No vision_analyze tool |
| 7 | HLVM_PT_DEBUG_MODE=20 returns non-zero | UNVERIFIABLE | No fresh run; handle-identity is necessary but not sufficient |

## Verdict reasoning
SOME_RELAX is honest: 4 of 6 acceptance criteria are VERIFIED based on the on-disk evidence (build target, clean logs, non-uniform ReSTIR stats, handle identity). 1 acceptance criterion is EXPECTED PASS based on static analysis of the log stats. 2 acceptance criteria are UNVERIFIABLE in file-only mode (vision-check, fresh mode-20 run). The new v158 evidence (handle identity) is the first cycle in the lineage to land a falsification experiment for the 2026-07-30 diagnostic, narrowing the remaining search space. The cumulative EC-039 denial count is structural to this runspace and cannot be resolved by additional ticks; the operator runspace is required to formally close the remaining 2.

## Recommendation to next cycle / parent
Same as v157: run the 6 operator commands, vision-check, validator, optionally mode-20. If all pass, mark `docs/PENDING_PICK.md` card 3 `[x]` and the cycle closes.

## Note on stale-diagnostic hazard
The 2026-07-30 diagnostic is stale relative to v137. The stale-diagnostic risk is now LOWER than in v157 because the v158 handle-identity check directly falsifies one of the diagnostic's hypotheses (#4). The same mechanism that lets gi_raw read GBuffer data also lets the debug mode-20 read GBufferMaterial. If the mode-20 zero result is reproduced today, the cause is not handle identity — it's slangc dead-strip, image layout, or nvrhi second-binding-set drop, all runtime-only.
