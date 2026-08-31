# Pending Tests v2

- plan: docs/PENDING_PLAN_v2.md
- commit: docs/PENDING_COMMIT_v2.md
- tester: tester (single-head autonomous cron)
- timestamp: 2026-07-27T01:05:00Z

## Test strategy
v2 does not apply a code change. The acceptance test for this cycle is parent-driven diagnostic logging.

For v2 to be considered COMPLETE, the parent must:
1. Add the diagnostic patch from PENDING_PLAN_v2.md (3 HLVM_LOG lines around FGIPass::DispatchRays line 540).
2. Rebuild: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test`.
3. Run with `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8`.
4. Capture the fresh log and report whether:
   - The "FGIPass::DispatchRays" pre/post logs appear (dispatch is being called)
   - Any Vulkan validation errors appear between pre and post log lines
   - The gi_raw dump still shows R[0,0] G[0,0] B[0,0]
   - The "command list should be executed" warning still fires
5. Paste the new log back so the cron can analyze.

## Test files (in working tree)
- `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` (existing)
- No new test files this cycle.

## Run command (for parent session)
```
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py
# Also inspect the new gi_raw dump and compare to the 2026-07-27 00:07 baseline
```

## Expected
- Same broken render as v1 (gi_raw = 0,0,0). The v2 cycle is investigation-only.
- The diagnostic logs added by the parent will reveal whether the dispatch is reached and what state nvrhi sees.

## Risk / caveat
Without the parent's diagnostic run, the cron cannot make further progress. The next concrete action is on the parent side, not the cron.