# Pending Commit v3

- plan: docs/PENDING_PLAN_v3.md
- files: Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp, Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp
- source: no bundle
- target: working tree (no commit per cron rules)
- task: Apply three diagnostic HLVM_LOG instrumentation patches that capture runtime state needed for v4+ fix cycles. NO behavior change; pure observability.
- verify: parent must run `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test` and `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Binary/Debug/TestReSTIR_GI_Temporal` then paste the new log.
- skip_impl_review: no
- produces_test_files: no
- notes:
  - The cron cannot run the build/test/validate (terminal blocked by tirith). Parent must drive verification.
  - Three patches: FGIPass::DispatchRays ENTER/EXIT/binding-set logs + TestReSTIR_GI_Temporal Pre-GIPass/Post-GIPass bracket + RenderGBuffer post-waitForIdle log.
  - The patches capture four diagnostic questions: Q1 (does dispatch reach body?), Q2 (is binding set created?), Q3 (does dispatch return?), Q4 (does RenderGBuffer's queue-idle handshake complete?).

## Plan Deviations (impler fills this in if it deviated)
None — patches exactly match plan.

## Implementation Evidence (impler fills this in)
- File modified: `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp` — added 4 HLVM_LOG info-level calls bracketing DispatchRays (ENTER, binding-set, EXIT) + 1 HLVM_LOG warning for the early-return path. +19 lines, -0 lines.
- File modified: `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp` — added 2 HLVM_LOG info-level calls bracketing GIPass.DispatchRays (Pre-GIPass, Post-GIPass) + 1 HLVM_LOG info-level after RenderGBuffer's waitForIdle. +11 lines, -0 lines.
- Build/run verification: BLOCKED by tirith (cannot run terminal commands). Parent must run verify with diagnostic logging.
- Expected post-build behaviour: identical to v2 (gi_raw=0,0,0). v3's value is NOT a code fix; it's the diagnostic data needed for v4's targeted fix.

## Diagnostic Log Output Expected After Parent Runs Verify

If the patches land correctly and the binary builds, the parent will see (once per frame for ACCUM=8):

```
RenderGBuffer: post-waitForIdle, queue idle; reopening CommandList (frame=1)
Pre-GIPass: CommandList=0x... OutputTex=0x... Frame=0
FGIPass::DispatchRays ENTER: OutputTex=0x... OutputW=800 OutputH=600 Frame=0 CmdList=0x...
FGIPass: per-frame binding set created OK (handle=0x...)
FGIPass::DispatchRays EXIT: dispatch returned, OutputTex=0x...
Post-GIPass: returned Frame=0
```

This output answers:
- Q1: ENTER log appears → dispatch body is reached, NOT early-returning.
- Q2: binding-set-created log appears → binding set is valid, NOT null.
- Q3: EXIT log appears → dispatch returned, NOT hung.
- Q4: RenderGBuffer post-waitForIdle log appears → queue-idle handshake completes.

If ANY of the above logs are MISSING in the captured output, that tells us which link in the chain is broken. The next cycle (v4) will target that specific link.