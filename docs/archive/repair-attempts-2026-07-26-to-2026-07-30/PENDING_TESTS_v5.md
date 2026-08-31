# Pending Tests v5

- commit: docs/PENDING_COMMIT_v5.md
- files: Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py (unchanged from v1; already exists with 3 structural checks)
- test_strategy: parent-driven log capture + validator run. v5 adds no new test files. The acceptance check is the fresh log + fresh dump + validator result (see PENDING_PLAN_v5.md acceptance criteria).
- rationale: per `software-development-practices §Test-Driven Development`, every fix should have a failing test first. v5 is a REVERT — it removes the v1-introduced HLVM-bypass code that was added with a hypothesis ("isolate raster pass from later-pass validation errors") but was the actual regression. The "test" for v5 is the structural validator `validate_restir_gi.py` (already in place from v1, 3 checks: non-black mean > 5, spatial std > 30, cell-variance std > 8). The validator was failing 0/3 on the v1 binary run (gi_raw=0,0,0, display uniform gray); after v5, it should pass 3/3.
- red_phase: N/A — v5 is a revert of a known-bad change, not a new behavior change.
- green_phase: N/A — same reason. The "pass" is the validator returning 3/3 + the vision-analyzed display showing recognizable Sponza.
- verification commands (parent-driven, terminal blocked in cron):
  ```bash
  cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
  ./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test
  cd Engine/Source/Runtime/Binary/Debug
  HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 timeout 180 ./TestReSTIR_GI_Temporal
  # Capture log: TestReSTIR_GI_Temporal.log
  # Expected new lines per frame:
  #   Pre-GIPass: CommandList=0x... OutputTex=0x... Frame=N
  #   FGIPass::DispatchRays ENTER: OutputTex=0x... OutputW=800 OutputH=600 Frame=N CmdList=0x...
  #   FGIPass: per-frame binding set created OK (handle=0x...)
  #   FGIPass::DispatchRays EXIT: dispatch returned, OutputTex=0x...
  #   Post-GIPass: returned Frame=N
  # Expected ABSENT lines (the v1-era warning that v5 fixes):
  #   warning: A command list should be executed before it is reopened
  # Expected ABSENT diagnostic (was bracketing removed waitForIdle):
  #   RenderGBuffer: post-waitForIdle, queue idle; reopening CommandList
  cd Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data
  python3 validate_restir_gi.py
  # Expected after v5 (fix lands): 3/3 PASSED
  ```
- vision check (parent-driven):
  ```bash
  # Open the new display_frame8.png and confirm:
  # 1. Image is NOT uniform gray (should look like dim/dark Sponza geometry with walls/floor/ceiling visible)
  # 2. Image is NOT uniform black
  # 3. Image is NOT all-magenta or all-noise
  # 4. Per-region brightness varies: floor darker, ceiling lighter, columns visible
  # 5. No obvious banding or quadrant artifacts
  ```

## What this cycle did NOT do

- Did not write a new test file.
- Did not modify `validate_restir_gi.py`.
- Did not modify any shader (`GIPathTracing.hlsl`, `GIAccumulate_cs.hlsl`, etc.).
- Did not modify any binding layout.
- Did not modify the bug-088 fix at line 675 (preserved).
- Did not modify any FGIPass code (only RenderGBuffer).

## What this cycle DID do

- Removed the HLVM-bypass `close+execute+waitForIdle+open` block (3 executable statements + 1 redundant `CommandList->open()` call) at lines 1516-1531 (pre-v5 numbering).
- Removed the v3 `RenderGBuffer: post-waitForIdle` diagnostic log that bracketed the removed waitForIdle.
- Added an 8-line NOTE comment explaining why we don't split the CL mid-frame (forward-looking guidance to prevent regression).
- Net -8 lines (was 1936 lines, now 1928 lines).
- Single file modified: `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp`.

## What the parent must verify

The cron's terminal is blocked by tirith. The parent must:

1. **Build the test**: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test`. Should succeed (no shader changes, no API surface changes; pure C++ test-file edit).

2. **Run with the canonical env vars**: `cd Engine/Source/Runtime/Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal`. Should complete 8 frames in ~7-10 seconds (matching the v1 binary run's timing).

3. **Inspect the fresh log** (`TestReSTIR_GI_Temporal.log`). Look for:
   - 8 occurrences each of `Pre-GIPass: ...`, `FGIPass::DispatchRays ENTER: ...`, `FGIPass: per-frame binding set created OK`, `FGIPass::DispatchRays EXIT: ...`, `Post-GIPass: ...` — proves v3's diagnostic logs still fire and the GI dispatch is called and returns.
   - **0 occurrences** of `warning: A command list should be executed before it is reopened` — proves the v5 fix removed the v1-era warning.
   - **0 occurrences** of `RenderGBuffer: post-waitForIdle, queue idle; reopening CommandList` — proves the v3 diagnostic that bracketed the removed waitForIdle is also gone.
   - gi_raw normalized per-channel: **NON-ZERO** (e.g., `R[0.5, 1.5] G[0.5, 1.5] B[0.5, 1.5]`). If still `R[0.000, 0.000] G[0.000, 0.000] B[0.000, 0.000]`, v5 didn't fix the bug; v6 needed.
   - gbuffer_worldpos normalized per-channel: still `R[-15, 15] G[-12, 8] B[-14, 0]` (real Sponza geometry). If this regresses, v5 broke the raster pass.

4. **Run the validator**: `cd Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data && python3 validate_restir_gi.py`. Expected: `3/3 checks PASSED`. If `0/3` or `1/3`, v5 didn't fully fix the renderer; v6 needed.

5. **Vision-analyze the new display_frame8.png**: open the file in `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps/`. Should show recognizable Sponza geometry (walls, floor, ceiling, columns) with sane exposure. If still uniform gray/black/magenta, v5 is incomplete; v6 needed.

If any of 1-5 fail, paste the relevant log/dump back to the cron. The pipeline pivots to v6 with that evidence.

## Pipeline state after this audit

- 6 v5 markers on disk.
- v5 patch applied to source (verified by re-reading the file).
- 3 source patches total in working tree: v3 (4 logs), v4a (1 log), v5 (1 revert).
- No commit (cron rules).
- No build/run/validator executed by cron (terminal blocked).