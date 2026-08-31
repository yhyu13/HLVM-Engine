# Pending Plan v4

- task: Diagnostic upgrade + propose a candidate fix based on the strongest hypothesis from file-only analysis, gated on parent-side verification of v3's logs.
- source: no bundle
- approach:
  1. **v4a (diagnostic upgrade — REQUIRED before any fix)**. The v3 patches already added 4 diagnostic log sites in FGIPass::DispatchRays (early-return, ENTER, binding-set, EXIT) and 2 in TestReSTIR_GI_Temporal (Pre-GIPass, Post-GIPass) plus 1 in RenderGBuffer (post-waitForIdle). v4a adds 1 more diagnostic at the dump boundary: before the dump's `setTextureState(OutputTexture, CopySource)`, log the texture's CURRENT tracked state (via a tiny helper that walks nvrhi's tracked states — fallback: log the texture handle + frame index and let the v3 logs correlate). This is the **only place** in the dump pipeline that touches OutputTexture's layout; if the GI pass's write did happen but the layout wasn't transitioned to CopySource, the dump reads stale storage. Cost: +5 lines, -0 lines. Risk: zero (info-level log only).
  2. **v4b (candidate fix — gated on v4a log output, NOT to be landed without parent verification)**. Hypothesis: the `HLVM-bypass` `close+execute+waitForIdle+open` patch added by v1 cycle (lines 1516-1531 of TestReSTIR_GI_Temporal.cpp) is the regression. The 2026-07-25 SESSION_HANDOFF said the test was working with `e6b3d52`'s `WriteGBufferSentinels` removal. Between 2026-07-25 and 2026-07-27, v1 added the HLVM-bypass. v4b proposes REMOVING the close+execute+waitForIdle+open flow at lines 1520-1531 and the comment block at 1516-1519, restoring the 2026-07-25 working shape where RenderGBuffer just ends with `CommandList->open()` (which is now a no-op because nvrhi non-immediate CLs are already open after `createCommandList`). Cost: -16 lines, +0 lines. Risk: medium — if v4a's log shows the GI pass DID execute (ENTER+EXIT both fire), the fix is wrong. DO NOT LAND v4b until v4a's log confirms the GI dispatch returned normally AND the GI pass's OutputTexture state was correctly tracked.
  3. **v4c (validator + test gate — already in place from v1)**. The 3-check structural validator at `validate_restir_gi.py` is correct. After v4b (or any fix), the parent must run the binary with `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8`, capture the new log, run `validate_restir_gi.py`, and confirm `3/3 PASSED`.

- diff_estimate:
  - v4a: +5 lines (one new diagnostic log + helper), -0 lines
  - v4b (conditional on v4a log evidence): -16 lines (HLVM-bypass comment block + close+execute+waitForIdle+open), +0 lines
  - v4c: 0 lines (validator unchanged)

- skip_plan_review: no — even though v4a is a tiny diagnostic, v4b is a 16-line removal with risk of regressing the raster pass sentinel fix from 9a09df2. Reviewer must explicitly check that v4a's log evidence supports v4b before v4b lands.

- test_strategy: parent must (1) apply v4a patches, (2) `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test`, (3) `cd Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal`, (4) capture the fresh log and the fresh dump dir, (5) paste the v4a log line + the v3 ENTER/EXIT lines + the per-frame `gi_raw` pixel-stats line, (6) `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py`, (7) vision-analyze the new `display_frame8.png` (does it show recognizable Sponza geometry with sane exposure?).

- risks:
  - **v4a: none.** Pure info-level logging.
  - **v4b: medium.** Removing the close+execute+waitForIdle could re-introduce bug-088 (dropped ~90% of pipeline work) OR could regress bug-088's sister fix (raster pass sentinel issue from 9a09df2). The `bug-088` fix that v1 landed is at line 675 (`executeCommandList` at end of Render()). That end-of-Render execute is the OUTER submit that catches both the raster work AND the post-raster work into a single submission. If the HLVM-bypass at line 1520-1522 is removed, the raster work still gets submitted (because the CL is still open when end-of-Render execute fires). The worldpos dump SHOULD still show real geometry. The gi_raw dump SHOULD start showing non-zero if the GI pass's write was being dropped by the HLVM-bypass's CL state collision.
  - **v4b is gated on v4a**: do NOT apply v4b until v4a's log shows (a) GIPass::DispatchRays ENTER fires, (b) per-frame binding set created OK, (c) GIPass::DispatchRays EXIT fires (so dispatch returned normally). If any of those is MISSING, v4b is the wrong fix — the GI pass isn't even being called, and removing the HLVM-bypass won't help. Investigate upstream instead.
  - **v4c: no new risk.** Validator unchanged from v1.

## Why this is the right shape (v4a + conditional v4b)

The pipeline has now run 4 cycles (v1 speculative fix, v2 revert, v3 diagnostic-only, v4 this plan). Each cycle has been blocked by terminal access. The empirical evidence on disk tells us:

1. **Pre-v1 (2026-07-25)**: Test was working — `SESSION_HANDOFF_2026-07-25.md` documents `2fab7d6` + `e6b3d52` landing, `validate_restir_gi.py` returns PASS, `display_frame8.png` shows Sponza. Worldpos/gi_raw dumps had correct per-channel statistics.
2. **v1 (2026-07-27 00:07)**: bug-088 fix landed (add `executeCommandList` at end of Render). Worldpos dump still real, but gi_raw = (0,0,0).
3. **v2 (2026-07-27 01:05)**: Reverted speculative patch (no behavior change). gi_raw still 0.
4. **v3 (2026-07-27 02:10)**: Diagnostic-only cycle. Patches in source (verified 2026-07-27, lines 456-475 of FGIPass.cpp, lines 433-443 and 1524-1528 of TestReSTIR_GI_Temporal.cpp). Parent has not run v3 yet.

The regression window is v1. v1 added three changes: end-of-Render executeCommandList, the HLVM-bypass close+execute+waitForIdle+open in RenderGBuffer, and a comment update. The end-of-Render execute is the bug-088 fix and is correct. The HLVM-bypass close+execute+waitForIdle+open is the suspect.

But per the v1/v2 lessons in `software-development-practices §Test-Driven Development`: speculative patches without runtime data are forbidden. v4a is the diagnostic that justifies v4b.

## Honest assessment

This cron's terminal is blocked by tirith ("User denied this command" on every probe). Without terminal, I cannot:
- Apply v4a patches (would need patch tool, which works, but no build/test verification).
- Run the binary and capture the v3/v4a log.
- Run the validator.
- Vision-analyze the new display dump.

The parent session (or a future cron tick where terminal is enabled) must drive the verification step. v4a's diagnostic is the MINIMUM information needed to justify a fix — without it, v4b is a coin flip.

The pipeline tick can advance by:
1. Writing v4a patches to disk (file-only is fine for patch tool).
2. Writing v4 markers documenting v4a's planned diagnostic + the conditional v4b fix.
3. NOT applying v4b until v4a's log evidence is captured.

## Acceptance criteria for v4

**v4a accepted when:**
1. The v4a diagnostic patch is applied (1 new info-level log + helper).
2. Parent runs `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test`.
3. Parent runs `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` and captures the log.
4. Log shows the v4a log line + v3's ENTER/EXIT lines + per-frame gi_raw pixel-stats.

**v4b accepted when:**
1. v4a log shows GIPass::DispatchRays ENTER + binding-set + EXIT all fire (dispatch returned normally).
2. v4a log shows the GI pass's recorded OutputTexture state was UnorderedAccess at dispatch time.
3. The HLVM-bypass at lines 1516-1531 is removed.
4. Parent re-builds and re-runs. The new gi_raw dump shows non-zero per-channel range (e.g., R[0.85, 1.05] not R[0.000, 0.000]).
5. `validate_restir_gi.py` returns 3/3 PASSED on the new dumps.
6. `display_frame8.png` (vision-analyzed) shows recognizable non-uniform Sponza geometry.

**Pipeline complete when v4b acceptance 1-6 are all true.**

## Files this cycle will touch (if v4b lands)

- `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp` (v4a only — +5 lines)
- `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp` (v4a + v4b conditional — +5 / -16 lines)