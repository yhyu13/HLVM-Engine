# Pending Commit v2

- plan: docs/PENDING_PLAN_v2.md (revised)
- files: (none — v2 does NOT apply a code patch this cycle)
- source: no bundle
- target: working tree (no commit per cron rules)
- task: revert the incorrect v2 patch (CommandList->open() at line 418), document the v1 falsification, and request parent-driven diagnostic run.
- verify: parent must run `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test` and report the log output; or capture diagnostic data per the plan.
- skip_impl_review: no
- produces_test_files: no
- notes:
  - The v1 cycle's KEEP verdicts were falsified by parent verification (gi_raw dump shows zero). The v1 close+executeCommandList() at end of Render() didn't fix the renderer.
  - The v2 attempt to add `CommandList->open()` after RenderGBuffer was incorrect: RenderGBuffer already opens the CommandList at line 1534 before returning, so adding another open() would trigger nvrhi's `Cannot open a command list that is already open` error and drop the post-raster work.
  - The reverted patch has been replaced with a documentation comment explaining the RenderGBuffer internal lifecycle.
  - The actual root cause requires diagnostic data from a live run. The cron cannot acquire this without terminal access.

## Plan Deviations (impler fills this in if it deviated)
- The original v2 plan called for adding `CommandList->open()` at line 418. After investigation showed RenderGBuffer already opens at line 1534, the fix was reverted. The actual change to working tree is: comment block at line ~393 explaining the RenderGBuffer lifecycle (no functional change).

## Implementation Evidence (impler fills this in)
- File modified: `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp` — replaced the speculative v2 patch (CommandList->open() at line 418) with a 3-line documentation comment explaining RenderGBuffer's internal CommandList lifecycle. No behavior change.
- Build/run verification: BLOCKED by tirith (cannot run terminal commands). Parent must run verify with diagnostic logging.
- Expected post-build behaviour: same as v1 — renderer still produces gi_raw=0,0,0. The v2 cycle's value is NOT a code fix; it's the discovery that v1 was wrong and v2 speculative patches don't help without diagnostic data.