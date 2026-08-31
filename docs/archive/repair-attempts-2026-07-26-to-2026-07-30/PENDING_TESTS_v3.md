# Pending Tests v3

- plan: docs/PENDING_PLAN_v3.md
- commit: docs/PENDING_COMMIT_v3.md
- tester: tester (single-head autonomous cron)
- timestamp: 2026-07-27T02:10:00Z

## Test strategy

v3 is a diagnostic-instrumentation cycle. No new test files. The acceptance test is parent-driven log capture.

For v3 to be considered COMPLETE, the parent must:
1. Apply the three patches (already in working tree as of this commit).
2. Build: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test`.
3. Run with `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Binary/Debug/TestReSTIR_GI_Temporal` (or `cd Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal`).
4. Capture the new log (at `Binary/Debug/TestReSTIR_GI_Temporal.log`).
5. Verify the diagnostic logs appear:
   - `RenderGBuffer: post-waitForIdle, queue idle; reopening CommandList` appears once per frame (8 times for ACCUM=8).
   - `Pre-GIPass: CommandList=0x...` and `Post-GIPass: returned Frame=...` appear once per frame.
   - `FGIPass::DispatchRays ENTER: ...` appears once per frame.
   - `FGIPass: per-frame binding set created OK ...` appears once per frame.
   - `FGIPass::DispatchRays EXIT: dispatch returned ...` appears once per frame.
   - The `EARLY-RETURN` warning does NOT appear.
6. Verify gi_raw dump still shows R[0,0] G[0,0] B[0,0] (expected — v3 doesn't fix the bug, just instruments it).
7. Run the validator: `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py`.
8. Paste the relevant log lines back to the cron for v4 analysis.

## Test files (in working tree)

- `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` (existing, unchanged)
- No new test files this cycle.

## Run command (for parent session)

```
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test
cd Binary/Debug
HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal
cd -
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py
# Inspect the new TestReSTIR_GI_Temporal.log for the diagnostic log lines
```

## Expected log output (after parent runs verify)

Once per frame for ACCUM=8, the following should appear in `Binary/Debug/TestReSTIR_GI_Temporal.log`:

```
RenderGBuffer: post-waitForIdle, queue idle; reopening CommandList (frame=N)
Pre-GIPass: CommandList=0x... OutputTex=0x... Frame=N
FGIPass::DispatchRays ENTER: OutputTex=0x... OutputW=800 OutputH=600 Frame=N CmdList=0x...
FGIPass: per-frame binding set created OK (handle=0x...)
FGIPass::DispatchRays EXIT: dispatch returned, OutputTex=0x...
Post-GIPass: returned Frame=N
```

If all 5 lines appear per frame:
- Q1 (early-return): NO → dispatch body is reached.
- Q2 (binding set): YES → binding set is valid.
- Q3 (dispatch return): YES → dispatch returned.
- Q4 (queue idle): YES → waitForIdle completed.

If any of these lines are MISSING in the captured log, the missing line is the diagnostic clue for v4.

## Risk / caveat

The cron session cannot run the build/test/validate (tirith blocks terminal). The v3 cycle's value is in the patches themselves; the verification step is parent-driven.

The instrumentation is purely additive (HLVM_LOG info/warning calls). No risk of regression. The patches can be reverted by removing the HLVM_LOG lines if they ever become noise.