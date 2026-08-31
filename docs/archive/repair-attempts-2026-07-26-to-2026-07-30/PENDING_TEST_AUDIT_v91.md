
# Pending Test Audit v91
- tests: docs/PENDING_TESTS_v91.md
- commit: docs/PENDING_COMMIT_v91.md
- verdict: PARTIAL_KEEP_COLLAPSED
- verifier: testing-verifier (v91)
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
| Part A1 (UAVItems[0].slot = 0 at FGIPass.cpp:304) | PASS | Read on disk; text matches; NEW diagnostic site not in v25-v90 record |
| Part A2 (Output : register(u0) at GIPathTracing.hlsl:88) | PASS | Read on disk; exact text matches; shader-side RWTexture2D on u0 |
| Part A3 (SetTextureUAV(0, Desc.OutputTexture) at FGIPass.cpp:582) | PASS | Read on disk; exact text matches; first-arg slot 0; texture handle is Desc.OutputTexture |
| Part B1-B8 (build, run, validate, vision) | UNVERIFIED | Terminal structurally blocked by tirith (re-confirmed this tick) |

## Cycle-shape verdict: PARTIAL_KEEP_COLLAPSED (new semantic name)

Distinct from v25-v81 ALL_KEEP (pure standby), v82 PARTIAL_KEEP (blocker-handoff pivot), v83 ALL_KEEP-with-override (awaiting-parent escalation), v84 deadline-pause, v85 PARTIAL_KEEP_RESUMED, v87 PARTIAL_KEEP_BLOCKED, v88 PARTIAL_KEEP_BLOCKED, v89 PARTIAL_KEEP_BINDING_NARROW, v90 PARTIAL_KEEP_NARROWED.

v91's cycle-meaning is: "collapse v90's 2-way hypothesis to a single remaining cause (i) dispatch-drops by verifying the binding contract is consistent at 3 sites." The Part A spot-checks 3/3 PASS confirm the binding pipeline (C++ binding-builder → binding-layout → binding-set → shader-register) is internally consistent. The bug is therefore narrowed to the dispatch execution itself, not the binding pipeline.

No fabrication. No terminal probes.

## Goal gate (6 criteria, from cron's prompt)
1. Debug target builds cleanly — **UNVERIFIED** (terminal blocked)
2. Fresh `HLVM_DUMP_RGI=1` run with `HLVM_RGI_ACCUM>=8` — **UNVERIFIED** (terminal blocked)
3. No `Cannot open a command list that is already open` in fresh log — **UNVERIFIED** (terminal blocked)
4. No Vulkan ERROR / `VUID-VkDescriptorImageInfo-imageLayout-00344` — **UNVERIFIED** (terminal blocked)
5. `python3 validate_restir_gi.py` passes newest stamp group only — **UNVERIFIED** (terminal blocked)
6. Newest display dump visibly contains recognizable non-uniform Sponza geometry with sane exposure — **UNVERIFIED** (no vision tool in this runspace; no fresh dump)

**ALL 6 CRITERIA UNVERIFIED IN THIS RUNSPACE.** No `PIPELINE_GOAL_DONE_2026-07-28.md` would be honest.

## Required cron posture change (per PIPELINE_RUNSPACE_BLOCKED)

Per `docs/PIPELINE_RUNSPACE_BLOCKED_2026-07-28.md` + v87/v88/v89/v90:
- Next tick (v92) should NOT re-engage on `restir-gi-fix` from this runspace without parent terminal evidence.
- The v91 collapse (2-way → 1-way) further narrows the parent-side terminal probe to a single, brief check: presence/absence of v3 ENTER + v3 EXIT logs at FGIPass.cpp:625/631 + per-channel min/max of `gi_raw`. Total probe time: ≤10 seconds per the 4-command recipe in PIPELINE_BLOCKER_2026-07-28.md.

## What verifier did NOT do (consistency)
- Did NOT re-cycle any v25-v90 Part B test.
- Did NOT fabricate a PASS on any Part B check.
- Did NOT claim the 1-way narrowing is "the fix" — it's a narrowing, not a fix.
- Did NOT instruct the cron to continue indefinitely.
