
# Pending Test Audit v90
- tests: docs/PENDING_TESTS_v90.md
- commit: docs/PENDING_COMMIT_v90.md
- verdict: PARTIAL_KEEP_NARROWED
- verifier: testing-verifier (v90)
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
| Part A1 (OutputTexture → Desc.OutputTexture at line 420) | PASS | Read on disk; text matches; NEW diagnostic site at line 410-450 not in v25-v89 record |
| Part A2 (CreateTexture2D for OutputTexture at line 937) | PASS | Read on disk; exact text matches; RGBA32_FLOAT + UnorderedAccess + GIRawHDR name |
| Part A3 (DumpRGBA32FTexture(OutputTexture, "gi_raw") at line 1650) | PASS | Read on disk; exact text matches; same `OutputTexture` member |
| Part A4 (FGIPass.cpp:634 OutputTexture = Desc.OutputTexture is GI namespace local, NOT test class member) | PASS | Read on disk; confirms no alias mismatch between dispatch output and dump input |
| Part B1-B8 (build, run, validate, vision) | UNVERIFIED | Terminal structurally blocked by tirith (re-confirmed this tick) |

## Cycle-shape verdict: PARTIAL_KEEP_NARROWED (new semantic name)

Distinct from v25-v81 ALL_KEEP (pure standby), v82 PARTIAL_KEEP (blocker-handoff pivot), v83 ALL_KEEP-with-override (awaiting-parent escalation), v84 deadline-pause, v85 PARTIAL_KEEP_RESUMED, v87 PARTIAL_KEEP_BLOCKED, v88 PARTIAL_KEEP_BLOCKED, v89 PARTIAL_KEEP_BINDING_NARROW.

v90's cycle-meaning is: "narrow v89's 3-way downstream-surface hypothesis to a 2-way hypothesis by static-read of the dumper-side handle chain." The Part A spot-checks 4/4 PASS confirm the dump's source handle is the SAME handle the dispatch writes to (hypothesis (iii) eliminated). The bug is now either (i) dispatch-drops OR (ii) shader-side write skipped.

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

Per `docs/PIPELINE_RUNSPACE_BLOCKED_2026-07-28.md` + v87/v88/v89:
- Next tick (v91) should NOT re-engage on `restir-gi-fix` from this runspace without parent terminal evidence.
- The v90 narrowing (3-way → 2-way hypothesis) is the natural pivot to surface to the parent: the parent can disambiguate (i) vs (ii) in 10 seconds by running the 4-command recipe per `PIPELINE_BLOCKER_2026-07-28.md` and inspecting the v3 ENTER/EXIT logs at FGIPass.cpp:627 (ENTER) / FGIPass.cpp:631 (EXIT) + per-channel min/max of `gi_raw`.

## What verifier did NOT do (consistency)
- Did NOT re-cycle any v25-v89 Part B test.
- Did NOT fabricate a PASS on any Part B check.
- Did NOT claim the 2-way narrowing is "the fix" — it's a narrowing, not a fix.
- Did NOT instruct the cron to continue indefinitely.
