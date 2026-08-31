# Pending Test Audit v4

- tests: docs/PENDING_TESTS_v4.md
- commit: docs/PENDING_COMMIT_v4.md
- verdict: SOME_RELAX
- verifier: testing-verifier (single-head autonomous cron — same caveat as the rest of this pipeline)
- timestamp: 2026-07-27T03:40:00Z (estimated; cron tick wall clock)

## Broken-pattern audit

- [x] No from-x-import-y patch propagation bugs (no Python tests added; no propagation risk)
- [x] No test-bug-in-itself (validator unchanged; v4's acceptance check is the same validator that v1 verified)
- [x] No source-incomplete-relative-to-test (v4a only adds observability; v4b is gated and NOT applied)
- [x] No missing test isolation fixture (parent-driven log capture is single-frame single-run)
- [x] No AsyncMock on sync function (or vice versa) (no Python changes)

## Per-test verdict

- `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` — KEEP (unchanged from v1, 3 structural checks: non-black mean > 5, spatial std > 30, cell-variance std > 8). Already verified by v1's parent run to correctly FAIL on gi_raw=0,0,0.
- v4a diagnostic patch (1 info-level log + comment) — KEEP (pure observability, no risk of test-bug-in-itself).
- v4b conditional fix (HLVM-bypass removal) — DEFERRED until v4a's log evidence supports it.

## Honest assessment

v4 is a diagnostic-upgrade cycle. The acceptance check is mechanical evidence (parent-driven log capture), not an automated test. The cron's terminal is blocked by tirith ("User denied this command" on every probe), so the cron cannot run the binary, capture the log, run the validator, or vision-analyze the new display dump.

This audit's verdict is SOME_RELAX (not ALL_KEEP, not SOME_DELETE) because:
- The diagnostic patch itself is correct and safe (no broken-pattern risk).
- The validator exists and is correct (3 checks, calibrated against the uniform-gray baseline).
- The acceptance criteria are concrete and mechanically checkable.
- BUT the renderer is still broken (gi_raw still 0,0,0) — no fix has landed. Acceptance for the broader task (renderer produces visible Sponza geometry) is NOT met.

SOME_RELAX routes the state machine to the next PICK item. There is no next PICK item — v4 was the last [ ] in PENDING_PICK. The pipeline is at a checkpoint: v4a is in source; the parent must run it to capture the data needed for v5 (v4b fix or a different fix). The pipeline cannot make further progress without terminal access.

## Recommendations for the parent

1. **Build and run with v3+v4a patches** (already in the working tree, no further edits needed):
   ```bash
   cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
   ./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test
   cd Engine/Source/Runtime/Binary/Debug
   HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 timeout 180 ./TestReSTIR_GI_Temporal
   ```
2. **Capture the log** (`TestReSTIR_GI_Temporal.log` after the run).
3. **Vision-analyze the new `display_frame8.png`** — does it show recognizable Sponza geometry, or is it still uniform gray/magenta?
4. **Run the validator**: `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py`. Expected: `0/3 PASSED` (gi_raw still 0).
5. **Paste the relevant log lines** to the cron:
   - `Pre-GIPass: CommandList=0x... OutputTex=0x... Frame=N`
   - `FGIPass::DispatchRays ENTER: OutputTex=0x... OutputW=800 OutputH=600 Frame=N CmdList=0x...`
   - `FGIPass: per-frame binding set created OK (handle=0x...)`
   - `FGIPass::DispatchRays EXIT: dispatch returned, OutputTex=0x...`
   - `Post-GIPass: returned Frame=N`
   - `RenderGBuffer: post-waitForIdle, queue idle; reopening CommandList (frame=N+1)`
   - `DumpRGBA32FTexture: dumping gi_raw Texture=0x... Frame=8`
   - `DumpRGBA32FTexture: gi_raw normalized per-channel — R[...], G[...], B[...]`
6. **Decision matrix based on the log**:
   - If all v3 ENTER/EXIT logs fire AND the v4a gi_raw handle matches the v3 ENTER's OutputTex handle AND gi_raw is still 0 → v4b is the right fix (HLVM-bypass removal). Cron can apply v4b in the next tick.
   - If the v4a gi_raw handle DIFFERS from the v3 ENTER's OutputTex handle → different bug (texture recreation); v4b won't help. Different fix needed; report back.
   - If the v3 ENTER/EXIT logs do NOT fire → the GI dispatch isn't being called; v4b won't help. Different fix needed; report back.
   - If `validate_restir_gi.py` returns 3/3 PASSED AND the display shows visible Sponza → renderer is fixed; v4b is unnecessary. Pipeline complete.

## Pipeline state after this audit

- `docs/PENDING_PICK.md`: v4 is now `[x]` (audit SOME_RELAX complete).
- `docs/PENDING_*_v4.md`: 5 markers, all on disk.
- Source patches: v3 (3 patches) + v4a (1 patch) all in source.
- No source patches applied for v4b (gated).
- No build/run/validator executed by cron (terminal blocked).
- No new test files added.
- No commit (cron rules).

## Next pipeline tick

If the parent runs v3+v4a and pastes the log, the next cron tick (or this same tick on a re-run) can:
1. Correlate the v4a gi_raw handle with v3's FGIPass::DispatchRays ENTER handle.
2. If they match AND gi_raw is 0, write v5 plan = v4b (HLVM-bypass removal at lines 1516-1531) and proceed to impl/review/tests/audit.
3. If they differ or ENTER is missing, write v5 plan = a different fix targeting the actual root cause revealed by the log.
4. If validate returns 3/3 PASSED, pipeline complete.

If the parent does NOT run v3+v4a, the pipeline is blocked. The cron cannot unblock itself without terminal access. The next cron tick will see "audit SOME_RELAX, no next PICK item, no terminal" and exit [SILENT] per the dispatcher rules.