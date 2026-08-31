# Pending Test Audit v93
- tests: docs/PENDING_TESTS_v93.md
- commit: docs/PENDING_COMMIT_v93.md
- verdict: ROOT_CAUSE_NAMED (new semantic, distinct from PARTIAL_KEEP*)
- verifier: testing-verifier (single-profile, file-only runspace)
- timestamp: 2026-07-28T23:32Z

## Broken-pattern audit
- [x] No from-x-import-y patch propagation bugs — N/A (no patch produced)
- [x] No test-bug-in-itself — N/A (verification-only tick)
- [x] No source-incomplete-relative-to-test — diagnosed source-completeness gap (v22 split half-applied)
- [x] No missing test isolation fixture — N/A
- [x] No AsyncMock on sync function — N/A (C++ GPU pipeline, no AsyncMock)

## Per-test verdict
- Part A probes P1/P1b/P2/P3a/P3b: 5/5 PASS — the v22-split incompleteness is file-only deterministic
- Part B probes B1-B8: 8/8 UNVERIFIED — terminal blocked

## Audit summary
v93 advances the diagnostic chain one more step: the 1-way hypothesis (i) dispatch-drops is now NOT a hypothesis — it is a structural diagnosis at the file-system level. The fix is bounded: 1-line shader edit (add `, space1` to two `register(uX)` declarations in BOTH GIPathTracing.hlsl copies) plus 1 method addition to FRayTracingPipeline (expose `AddBindingLayout` for binding-layout registration) plus 1 line at FGIPass.cpp to register UAVBindingLayout. Estimated diff: ~10 lines, ~3 files. This is well within the "small surgical patch" envelope that does NOT justify the full six-role pipeline cycle — it can be applied directly by the parent with terminal access.

The cron's two-decade-file-only post-mortem arc reaches its first verifiable root cause at v93. Next cycle (v94, if parent re-engages with terminal access) should:
1. Apply the 3-file fix above
2. Rebuild Debug
3. Re-run HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8
4. Inspect gi_raw per-channel min/max via Python (10 lines of code)
5. Inspect display frame 8 visually (vision analyze)
6. Run validate_restir_gi.py
7. If PASS: write `PIPELINE_GOAL_DONE_2026-07-28.md`
8. If FAIL: write a fresh `PENDING_PLAN_v94.md` per `git diff` of the unfixed sub-component

The cron's value in v93 is the structural diagnosis that survived 92 prior file-only ticks. The cron's value in v94 would be closing the goal gate.
