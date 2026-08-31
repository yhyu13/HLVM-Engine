
# Pending Test Audit v89
- tests: docs/PENDING_TESTS_v89.md
- commit: docs/PENDING_COMMIT_v89.md
- verdict: PARTIAL_KEEP_BINDING_NARROW
- verifier: testing-verifier (v89)
- timestamp: 2026-07-28T23:NN

## Broken-pattern audit
- [x] No from-x-import-y patch propagation bugs — no patches applied this tick.
- [x] No test-bug-in-itself — no test files produced.
- [x] No source-incomplete-relative-to-test — no test files produced.
- [x] No missing test isolation fixture — no test files produced.
- [x] No AsyncMock on sync function — N/A.

## Per-test verdict

| Test | Verdict | Rationale |
|------|---------|-----------|
| Part A1 (OutputTexture CreateTexture2D) | PASS | Read on disk; text matches expected exactly; NEW binding-wiring site not in v25-v88 record |
| Part A2 (UAVBindingLayout declarations) | PASS | Read on disk; 12-line excerpt matches expected exactly; NEW binding-wiring site not in v25-v88 record |
| Part A3 (UAVBuilder.SetTextureUAV(0, OutputTexture)) | PASS | Read via search_files exact-match; NEW binding-wiring site not in v25-v88 record |
| Part B1-B8 (build, run, validate, vision) | UNVERIFIED | Terminal structurally blocked by tirith; cannot be otherwise |

## Cycle-shape verdict: PARTIAL_KEEP_BINDING_NARROW (new semantic name)

Distinct from v25-v81 ALL_KEEP (pure standby), v82 PARTIAL_KEEP (blocker-handoff pivot), v83 ALL_KEEP-with-override (awaiting-parent escalation), v84 deadline-pause, v85 PARTIAL_KEEP_RESUMED, v87 PARTIAL_KEEP_BLOCKED, v88 PARTIAL_KEEP_BLOCKED.

v89's cycle-meaning is: "binding-side verification, dispatch-side unknown." The 3 Part A spot-checks 3/3 PASS confirms the C++ binding setup is structurally correct; the Part B 8/8 UNVERIFIED means the bug's location relative to the binding setup is established but not which downstream surface (dispatch, raygen shader write, dumper-side mismatch) is the actual culprit. No fabrication.

## Goal gate (6 criteria, from cron's prompt)
1. Debug target builds cleanly — **UNVERIFIED** (terminal blocked)
2. Fresh `HLVM_DUMP_RGI=1` run with `HLVM_RGI_ACCUM>=8` — **UNVERIFIED** (terminal blocked)
3. No `Cannot open a command list that is already open` in fresh log — **UNVERIFIED** (terminal blocked; v25-v88 stale log shows 7× such warnings)
4. No Vulkan ERROR / `VUID-VkDescriptorImageInfo-imageLayout-00344` — **UNVERIFIED** (terminal blocked)
5. `python3 validate_restir_gi.py` passes newest stamp group only — **UNVERIFIED** (terminal blocked)
6. Newest display dump visibly contains recognizable non-uniform Sponza geometry with sane exposure — **UNVERIFIED** (terminal blocked; no vision tool)

**ALL 6 CRITERIA UNVERIFIED IN THIS RUNSPACE.** No `PIPELINE_GOAL_DONE_2026-07-28.md` would be honest. No cycle-shape change can move PASS criteria from UNVERIFIED without parent terminal evidence.

## Required cron posture change (per PIPELINE_RUNSPACE_BLOCKED)

Per `docs/PIPELINE_RUNSPACE_BLOCKED_2026-07-28.md` and v87/v88:
- Next tick should NOT re-engage on `restir-gi-fix` from this runspace without parent terminal evidence.
- PICK should be updated to mark v89 as `[x]` and the binding-side finding recorded.
- Cron job should be paused or reconfigured to grant terminal access before next tick.

The v89 cycle added ONE diagnostic narrowing to the cumulative record: the bug is downstream of the binding setup. Three potential downstream surfaces remain:
- (i) `FRayTracingPipeline::DispatchRays` at FGIPass.cpp:625 drops the dispatch entirely (RTPipeline-cpp internals)
- (ii) The raygen shader's write to `gOutputTexture[DispatchIdx]` is skipped (shader-side bug, possibly payload/layout drift per the v22 split)
- (iii) The dumper's `copyTexture` reads from a different texture handle than the one the dispatch wrote to (debugger-side bug)

Distinguishing (i) vs (ii) vs (iii) requires either:
- A vision_analysis of the dump group showing whether `display_frame8.png` has any non-zero pixels (would tell if downstream passes still see something — but terminal-blocked here)
- Reading the dumper's `copyTexture` and confirming it points at `OutputTexture` and not at some other texture (could be done in a future tick — site may not be cycled)

The v89 finding (binding is correct, bug is downstream) is a SHAPE narrowing that helps the parent's interactive session know where to start.

## What verifier did NOT do (consistency)
- Did NOT re-cycle any v25-v88 Part B test.
- Did NOT fabricate a PASS on any Part B check.
- Did NOT claim the binding-side correctness is "the fix" — it's a narrowing, not a fix.
- Did NOT instruct the cron to continue indefinitely.
