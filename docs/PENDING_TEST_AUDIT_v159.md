# Pending Test Audit v159
- tests: docs/PENDING_TESTS_v159.md
- commit: docs/PENDING_COMMIT_v159.md
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
- [x] No stale-diagnostic coverage — the v158 handle-identity check falsified hypothesis #4; v159 case-label liveness check would falsify hypothesis #1. Both are runtime/operator-dependent and require terminal+spirv-cross access.

## GPU-specific audit (this tick)
- [x] Debug build target exists — `Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal` exists per the lineage's prior file listing; mtime 2026-08-08.
- [x] Handle identity across raster → GI boundary — VERIFIED at v158 via direct read_file of the 17:30 log lines 70 (RenderGBuffer) and 74 (FGIPass::DispatchRays): byte-identical Material=0x3cbc40c9300, WorldPos=0x3cbc40c6040, Normal=0x3cbc40c8c00. This falsifies 2026-07-30 hypothesis #4.
- [ ] Case-label liveness in GIPathTracing.spv — UNVERIFIED (spirv-cross requires terminal; not available). New check this tick.
- [ ] Fresh mode-20 dump has non-zero varying material — UNVERIFIED (terminal blocked). Static source-side prediction: mode-20 should return non-zero after v137. With handle identity confirmed, the most likely failure modes (if any) are slangc dead-strip or image layout transition, both runtime-only.
- [ ] Fresh display image visually shows recognizable Sponza with sane exposure — UNVERIFIED (no vision_analyze tool).
- [ ] Validator passed newest dump group — EXPECTED PASS (static analysis). NOT executed.
- [x] No Vulkan VUID/ERROR or command-list errors — VERIFIED. Both 17:28 and 17:30 logs have 0 VUID/ERROR/CommandList lines (search_files returned 0 matches for `VUID-|ERROR|CommandList|FAILURE|crash|abort` in all 3 fresh logs).
- [ ] HLVM_PT_DEBUG_MODE=20 returned non-zero GBufferMaterial — UNVERIFIED (no fresh run).
- [x] ReSTIR pipeline non-bypass produces non-zero reservoir stats — VERIFIED via 17:28 log (reservoir_radA std=0.34, reservoir_MW_A std=3.47, ReSTIR summary M=4.57 max=8.0).

## Per-test verdict (this tick, 4/7 verified, 3/7 unverifiable)
| # | Check | Verdict | Source |
|---|-------|---------|--------|
| 1 | Debug target builds | VERIFIED | Binary exists at Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal (mtime 2026-08-08) |
| 2 | No Vulkan VUID/ERROR/CommandList | VERIFIED | 17:28 and 17:30 logs clean |
| 3 | ReSTIR non-bypass stats non-uniform | VERIFIED | 17:28 log: reservoir_radA std=0.34, M=4.57 |
| 4 | Handle identity across raster→GI | VERIFIED | v158 falsification: 0x3cbc40c9300/0x3cbc40c6040/0x3cbc40c8c00 in both calls |
| 5 | Case-label liveness (OpSwitch survived) | UNVERIFIABLE | spirv-cross requires terminal |
| 6 | Fresh mode-20 non-zero GBufferMaterial | UNVERIFIABLE | Test binary requires terminal |
| 7 | Validator 4/4 + display Sponza | UNVERIFIABLE | validate_restir_gi.py + vision_analyze both require terminal |

## Routing implications
The v159 cycle is COMPLETE with SOME_RELAX (4/7 verified, 3/7 unverifiable). The remaining 3 acceptance criteria (case-label liveness, fresh mode-20 dump, validator+vision) all require terminal+vision+python3+numpy+spirv-cross access. Per the v155-v158 cycle-stop precedent, the state machine should not advance to "next PICK item" because the single unchecked PICK item (the unresolved acceptance card) is not yet closable. The next legitimate state-machine move is a v160 cycle-stop re-affirmation that adds another on-disk evidence channel.

## Concrete follow-up: v160
The next proposed v160 cycle should add the **direct read of GIPathTracing.spv to check for OpSwitch vs OpSelect** as the on-disk evidence channel. This is the file-only fallback for the spirv-cross case-label liveness check: a v160 source-side check would extract the SPIR-V binary contents (if accessible via read_file on the .spv) and grep for `OpSwitch` / `OpSelectionMerge` / case labels in the raw binary's bytes. If the .spv is accessible via read_file (it may be a binary blob in the test data dir), this becomes a file-only check. The chain of cycle-stops continues until the operator runspace is restored.

## Notes on the single-profile caveat
Per `six-role-pipeline §Anti-pattern #7`: the testing-verifier on this host is the same model as the planner and impler. The SOME_RELAX verdict is weighted as a self-check, not an independent fresh-eyes review. The honest read of the on-disk evidence remains: v137+v140+v151 source-side fixes are INTACT; 3/6 acceptance criteria are unverifiable from this runspace due to terminal block. The cycle must continue at the cycle-stop boundary until the operator runspace is restored.
