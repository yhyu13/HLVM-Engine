# Pending Test Audit v5

- tests: docs/PENDING_TESTS_v5.md
- commit: docs/PENDING_COMMIT_v5.md
- verdict: SOME_RELAX
- verifier: testing-verifier (single-head autonomous cron — see software-development-practices §"Full auto" anti-pattern #7 caveat; gating is mechanical evidence not independent judgment)
- timestamp: 2026-07-27T05:00:00Z (estimated; cron tick wall clock)

## Broken-pattern audit

- [x] No from-x-import-y patch propagation bugs (no Python tests added; v5 is pure C++ revert)
- [x] No test-bug-in-itself (validator unchanged; v5's acceptance check is the same validator that v1 verified)
- [x] No source-incomplete-relative-to-test (v5 is a revert of v1's HLVM-bypass, restoring the 2026-07-25 source shape that the validator already passed)
- [x] No missing test isolation fixture (parent-driven single-run verification)
- [x] No AsyncMock on sync function (or vice versa) (no Python changes)

## Per-test verdict

- `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` — KEEP (unchanged from v1, 3 structural checks: non-black mean > 5, spatial std > 30, cell-variance std > 8). v5 does not modify the validator. The validator was failing 0/3 on the v1 binary run; it should pass 3/3 on a v5 binary run IF the GI dispatch actually writes to the dump.
- v5 patch (HLVM-bypass removal) — KEEP (surgical revert of v1-introduced code, restoring 2026-07-25 working shape). The patch:
  - Removes the `close+execute+waitForIdle+open` block (the v1 regression).
  - Removes the v3 `post-waitForIdle` diagnostic that bracketed the removed waitForIdle.
  - Adds an 8-line NOTE comment explaining why we don't split the CL mid-frame.
  - Preserves bug-088 fix at line 675.
  - Preserves bug-075 binding-layout split.
  - Touches only `TestReSTIR_GI_Temporal.cpp`.

## Honest assessment

v5 is a REVERT cycle. The acceptance is mechanical evidence (parent-driven build + run + log + validator + vision), not an automated test. The cron's terminal is blocked by tirith ("User denied this command" on every probe), so the cron cannot:

- Run the build (cannot run `./Build.sh`).
- Run the binary (cannot run `./TestReSTIR_GI_Temporal`).
- Capture the fresh log.
- Run the validator (`python3 validate_restir_gi.py`).
- Vision-analyze the new dump (no `terminal`, no `display_image` tool — file-only mode).

This audit's verdict is SOME_RELAX (not ALL_KEEP, not SOME_DELETE) because:

- The v5 patch itself is correct and safe (surgical revert; preserves all working parts of v1; restores 2026-07-25 working shape).
- The validator exists and is correct (3 checks, calibrated against the v1 broken-baseline).
- The acceptance criteria are concrete and mechanically checkable.
- BUT the renderer is still BROKEN until the parent runs the build+test cycle with v5 patches. Acceptance for the broader task (renderer produces visible Sponza geometry) is NOT yet met.

SOME_RELAX routes the state machine to the next PICK item. There is no next PICK item — v5 was the last `[ ]` in PENDING_PICK. The pipeline is at a verification checkpoint:

- v5 patch is LANDED IN THE WORKING TREE (verified by re-reading the file at lines 1505-1534 after the patch).
- The parent must run the build+test cycle to capture the data needed to confirm v5 is the right fix.
- If v5 fixes the renderer: pipeline complete.
- If v5 doesn't fix the renderer: v6 needed with a different fix targeting the actual evidence.

## Pipeline state after this audit

- `docs/PENDING_PICK.md`: v5 still `[ ]` (audit SOME_RELAX complete; the [x] mark goes on the next tick when parent confirms).
- `docs/PENDING_*_v5.md`: 6 markers, all on disk (plan, plan-review, commit, impl-review, tests, this audit).
- Source patches: v3 (4 logs) + v4a (1 log) + v5 (1 revert) all in source.
- No commit (cron rules).
- No build/run/validator executed by cron (terminal blocked).

## Recommendations for the parent

1. **Build and run with v3+v4a+v5 patches** (already in the working tree, no further edits needed):

   ```bash
   cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
   ./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test
   cd Engine/Source/Runtime/Binary/Debug
   HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 timeout 180 ./TestReSTIR_GI_Temporal
   ```

2. **Capture the fresh log** (`TestReSTIR_GI_Temporal.log` after the run).

3. **Verify the v5 fix lands correctly**:
   - Log should NOT contain `warning: A command list should be executed before it is reopened` (the v1-era warning should be gone).
   - Log SHOULD still contain v3's diagnostic markers per frame.
   - Log should NOT contain `RenderGBuffer: post-waitForIdle, queue idle; reopening CommandList` (the diagnostic that bracketed the removed waitForIdle).
   - gi_raw normalized per-channel SHOULD be non-zero (e.g., R[0.5, 1.5] G[0.5, 1.5] B[0.5, 1.5]).
   - gbuffer_worldpos normalized per-channel SHOULD be unchanged (R[-15, 15] G[-12, 8] B[-14, 0]).

4. **Vision-analyze the new `display_frame8.png`**: should show recognizable non-uniform Sponza geometry (not uniform gray/magenta/black).

5. **Run the validator**: `cd Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data && python3 validate_restir_gi.py`. Expected: `3/3 checks PASSED`.

6. **Paste the relevant log lines and validator result back to the cron**:

   ```text
   # Required lines from the new log (each should appear 8 times, once per frame):
   Pre-GIPass: CommandList=0x{CL} OutputTex=0x{GI} Frame=N
   FGIPass::DispatchRays ENTER: OutputTex=0x{GI} OutputW=800 OutputH=600 Frame=N CmdList=0x{CL}
   FGIPass: per-frame binding set created OK (handle=0x{BS})
   FGIPass::DispatchRays EXIT: dispatch returned, OutputTex=0x{GI}
   Post-GIPass: returned Frame=N

   # Required line from the dump frame:
   DumpRGBA32FTexture: dumping gi_raw Texture=0x{GI} Frame=8
   DumpRGBA32FTexture: gi_raw normalized per-channel — R[?,?] G[?,?] B[?,?]

   # Validator result:
   validate_restir_gi.py → 3/3 checks PASSED  (or 0/3, 1/3, 2/3 with details)

   # Vision check on display_frame8.png:
   display shows recognizable Sponza geometry: yes/no
   ```

7. **Decision matrix based on the parent's report**:

   - If `3/3 PASSED` + display shows Sponza: pipeline complete. v5 marked done.
   - If `0/3` or `1/3` + gi_raw still 0,0,0: v5 didn't fix the bug. v6 needed.
   - If `2/3` + display still bad: v5 partially worked but accumulate/ReBLUR still off. v6 needed.
   - If build fails: log the compile error; v6 needed.

## If parent cannot run v3+v4a+v5 (out of session)

v5 is BLOCKED. The pipeline cannot make further progress without terminal access. The cron tick must exit with the v5 audit SOME_RELAX and the patch landed on disk, awaiting parent verification.

## Next pipeline tick (after parent verification)

- If parent confirms v5 fixed the renderer: mark v5 [x] in PENDING_PICK.md, append PIPELINE_HEALTH_2026-07-27.md with the verification evidence, exit [SILENT] or report PASS.
- If parent reports v5 didn't fix the renderer: write v6 plan based on the actual log evidence (different fix targeting whatever the log shows), proceed through plan-criticer/impler/reviewer/tests/audit.