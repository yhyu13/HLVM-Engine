# Pending Test Audit v2

- tests: docs/PENDING_TESTS_v2.md
- commit: docs/PENDING_COMMIT_v2.md
- verdict: SOME_RELAX
- verifier: testing-verifier (single-head autonomous cron)
- timestamp: 2026-07-27T01:05:00Z

## Broken-pattern audit
- [x] No from-x-import-y patch propagation bugs (no Python changes this cycle)
- [x] No test-bug-in-itself (validator is unchanged from v1)
- [x] No source-incomplete-relative-to-test (no source change this cycle)
- [x] No missing test isolation fixture (validator is independent per-run)
- [x] No AsyncMock on sync function (N/A)

## Per-test verdict
- `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` — KEEP (unchanged from v1)

## Note on actual test run
The cron session cannot run the build/test/validate command (tirith blocks all terminal). The v2 cycle is investigation-only — no code patch was applied.

## Some-relax rationale
SOME_RELAX (not ALL_KEEP) because the v2 cycle did not produce a working renderer. The acceptance criteria (gi_raw non-zero, validator passes, Sponza visible) are NOT met. The cron cannot satisfy these criteria without terminal access and diagnostic data from the parent.

## Acceptance criteria (deferred to v3 or interactive debugging)
1. Test target builds → parent must run `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test`
2. Fresh `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` run produces 7 PNG dumps
3. **`DumpRGBA32FTexture: gi_raw` log line shows non-zero range** ← UNMET. v1 produced R[0,0] G[0,0] B[0,0]. v2 made no source change so this will still be unmet.
4. No "A command list should be executed before it is reopened" warnings ← UNMET. Will likely still fire.
5. Validator: 3/3 checks PASS ← UNMET. Will likely still fail because gi_raw propagates through display/spatial/denoised.
6. Display dump visually shows recognizable non-uniform Sponza geometry ← UNMET.
7. Sane exposure (no full-white or full-black frames) ← UNMET.

## Recommendation for v3 or interactive session
The cron has hit its limit without terminal access. Three paths forward:
1. **Parent-driven interactive debugging**: parent adds diagnostic logging, runs the test, shares the log, and we collaborate on the fix in the next session.
2. **Cron with terminal toolset re-enabled**: a future cron tick where the dispatcher has terminal access can do the bisect directly.
3. **Re-spawn as fresh cron with new diagnostic instructions**: the cron can file a v3 plan that explicitly says "add diagnostic logging then run."

The current state of TestReSTIR_GI_Temporal.cpp is functionally identical to v1 (only documentation comment changed) — no regression risk.