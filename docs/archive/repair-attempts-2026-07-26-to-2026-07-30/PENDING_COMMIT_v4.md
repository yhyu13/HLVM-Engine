# Pending Commit v4

- plan: docs/PENDING_PLAN_v4.md
- files: Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp
- source: no bundle
- target: working tree (no commit per cron rules)
- task: v4a — add one diagnostic log at the dump's setTextureState site to correlate the GI pass's write with the dump's read. v4b is conditional and NOT included in this commit; it will only be applied after v4a's log evidence supports the hypothesis.
- verify: parent must run `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test` and `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Binary/Debug/TestReSTIR_GI_Temporal` then paste the new log lines (DumpRGBA32FTexture: dumping gi_raw, FGIPass::DispatchRays ENTER/EXIT, Pre-GIPass/Post-GIPass, RenderGBuffer post-waitForIdle, gi_raw normalized per-channel).
- skip_impl_review: no — even though v4a is tiny (one log line + comment), the impl must not regress anything else and must compile cleanly.
- produces_test_files: no
- notes:
  - The cron cannot run the build/test/validate (terminal blocked by tirith). Parent must drive verification.
  - v4a's diagnostic log fires BEFORE setTextureState for every DumpRGBA32FTexture call (display, spatial, denoised, gi_raw, gbuffer_worldpos, gbuffer_normal, gbuffer_material). On the last frame (bDumpRequested && bLastFrame), it fires 7 times. The relevant log line for the v4b decision is the gi_raw one: its texture handle must match the OutputTexture handle from the FGIPass::DispatchRays ENTER log.
  - The patch is one info-level log; it does NOT modify GPU state, command recording, or binding layouts. Pure observability.

## Plan Deviations (impler fills this in if it deviated)

None — patch exactly matches v4a in the plan.

## Implementation Evidence (impler fills this in)

- File modified: `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp` — added 1 HLVM_LOG info-level call bracketing DumpRGBA32FTexture's setTextureState, with a 7-line comment explaining the correlation with v3 logs and v4b's gating. +9 lines, -0 lines.
- Build/run verification: BLOCKED by tirith (cannot run terminal commands). Parent must run verify with diagnostic logging.
- Expected post-build behaviour: identical to v3 (gi_raw=0,0,0). v4a's value is NOT a code fix; it's the additional diagnostic data needed for v4b's gated fix decision.

## Diagnostic Log Output Expected After Parent Runs Verify

After parent builds+reruns with v3+v4a patches, the log will show (for the dump frame):

```
[per-frame, frame N=AccumFrameCount]
Pre-GIPass: CommandList=0x... OutputTex=0xHGI Frame=N
FGIPass::DispatchRays ENTER: OutputTex=0xHGI OutputW=800 OutputH=600 Frame=N CmdList=0x...
FGIPass: per-frame binding set created OK (handle=0x...)
FGIPass::DispatchRays EXIT: dispatch returned, OutputTex=0xHGI
Post-GIPass: returned Frame=N
RenderGBuffer: post-waitForIdle, queue idle; reopening CommandList (frame=N+1)

[at the dump boundary, bLastFrame only]
DumpRGBA32FTexture: dumping display Texture=0xHDISPLAY Frame=8
DumpRGBA32FTexture: dumping spatial Texture=0xHSpatial Frame=8
DumpRGBA32FTexture: dumping denoised Texture=0xHDENOISED Frame=8
DumpRGBA32FTexture: dumping gi_raw Texture=0xHGI Frame=8
DumpRGBA32FTexture: dumping gbuffer_worldpos Texture=0xHWORLDPOS Frame=8
DumpRGBA32FTexture: dumping gbuffer_normal Texture=0xHNORMAL Frame=8
DumpRGBA32FTexture: dumping gbuffer_material Texture=0xHMATERIAL Frame=8
DumpRGBA32FTexture: gi_raw normalized per-channel — R[0.000,0.000] G[0.000,0.000] B[0.000,0.000]
DumpRGBA32FTexture: gbuffer_worldpos normalized per-channel — R[-15.228,15.264] G[-11.811,8.193] B[-14.291,0.025]
```

**Key correlation**: 0xHGI (OutputTexture) must appear in BOTH the FGIPass::DispatchRays ENTER log AND the `DumpRGBA32FTexture: dumping gi_raw` log. If both show the same handle, the dump is reading from the correct texture; if gi_raw is still 0, the bug is the GI pass's write being dropped (likely by the HLVM-bypass at lines 1516-1531, which v4b proposes to remove).

If the dump's gi_raw texture handle differs from the FGIPass ENTER's OutputTexture handle, that's a different bug (texture recreated between passes), and v4b's removal won't help — different fix needed.

## v4b Status: NOT INCLUDED IN THIS COMMIT

v4b (removing the HLVM-bypass close+execute+waitForIdle+open at lines 1516-1531) is gated on v4a's log evidence. It will only be applied in a separate v5 commit AFTER the parent verifies:
1. FGIPass::DispatchRays ENTER + binding-set + EXIT all fire (dispatch returned normally).
2. The dump's gi_raw handle matches the FGIPass ENTER's OutputTexture handle.
3. gi_raw is still 0,0,0 despite the handle matching.

If any of 1-3 is false, the parent must report back with the actual log so the pipeline can pivot to a different fix.