# Pending Plan v2 (revised)

- task: Re-investigate the v1 KEEP that was falsified by parent verification (2026-07-27 00:07 binary's dumps show gi_raw R[0,0] G[0,0] B[0,0]). v1 added end-of-Render() close+executeCommandList(). The dump evidence proves post-raster pipeline is not producing visible OutputTexture. Need to find the actual cause.

## Investigation findings (from file-only analysis, no terminal)

### What the 2026-07-27 00:07 log definitively shows
- `RenderGBuffer frame N: drew 24 meshes` (line 1500) followed by close+execute+waitForIdle+open INSIDE RenderGBuffer at lines ~1529-1534. The reopen at line 1534 means the CommandList IS open when control returns to Render() at line 397.
- `DumpRGBA32FTexture: gi_raw normalized per-channel — R[0.000,0.000] G[0.000,0.000] B[0.000,0.000]` (line 76). Conclusive: GIPass never wrote to OutputTexture. Since the dump creates its own CL and the dump code's `requireOpenState` check passes (we see the dump succeed), the dump CL is OPEN — but the texture's storage is uninitialized UAV zero-fill (the default for never-written Vulkan storage images).
- `DumpRGBA32FTexture: gbuffer_worldpos normalized per-channel — R[-15.228,15.264] G[-11.811,8.193] B[-14.291,0.025]` (line 78). The raster pass IS writing correctly. So RenderGBuffer's close+execute+waitForIdle+open cycle is working.
- `A command list should be executed before it is reopened` warning fires once per frame (8 times across 8 frames). This is nvrhi's informational warning fired when an `open()` call sees the wrapper state in CLOSED.

### What the v1 review missed
- The v1 review shipped a KEEP verdict without examining the actual log produced by the v1 binary. This cron doesn't have terminal access, so I could not run the verify command from the v1 plan. The parent (user) DID run it (the 00:07 binary exists with dumps). The dumps prove v1 didn't fix the renderer.

### Hypotheses for why gi_raw is zero (in order of likelihood)

**H1: GIPass dispatch is being recorded but the dispatch is silently dropped by Vulkan due to a layout/validation error that fires AFTER the dispatch's record but BEFORE execute.**

This fits the symptom perfectly:
- CommandList state machine: OPEN during record → no error logs from `requireOpenState` (would say "must be opened before rendering commands")
- executeCommandList does not fire any explicit error
- gi_raw is never written
- The "command list should be executed" warning fires once per frame (which is normal for a CL that goes CLOSED→CLOSED via execute without immediate-CL state reset)

**H2: A nvrhi validation issue in the timing of `setTextureState(ReservoirTex0, ShaderResource)` after GIPass has been dispatched but before the dispatch actually runs.**

This is the bug-075 follow-up pattern. After GIPass writes OutputTexture (in UnorderedAccess state), the denoise pass needs to read it as ShaderResource. If the transition is wrong, the read happens on uninitialized UAV storage (zero). Same for the ReSTIR reservoirs that ping-pong — if their transitions are wrong, the temporal pass reads garbage.

**H3: The HLVM-bypass isolation patch at line 1529-1531 (close+execute+waitForIdle) causes a Vulkan validation issue for the post-raster pipeline.**

The waitForIdle makes the queue idle before the post-raster pipeline starts. If a binding set holds a stale descriptor, the post-raster pipeline could fail to find the textures it expects. The warning fires once per frame which matches "the queue is idle, but the next CL is not properly synchronized."

### What v2 needs to do (file-only investigation cannot complete this)

The actual fix requires running the test with fresh logging to see whether GIPass.DispatchRays is recorded, whether RTPipeline.DispatchRays succeeds, and whether any Vulkan validation errors fire. None of these can be determined from file analysis alone.

**Recommendation**: Since the cron has no terminal access (tirith blocks all `terminal()` calls), and the v1 cycle's KEEP verdicts were falsified by parent verification, the right move is to **NOT apply another speculative fix**. Instead, document the investigation findings, mark v1 as falsified, and request parent-run verification with FRESH DIAGNOSTIC LOGGING.

### Diagnostic patch to request from parent

Add the following to FGIPass::DispatchRays around line 540 to capture dispatch state:

```cpp
HLVM_LOG(LogGI, info, TXT("FGIPass::DispatchRays: w={} h={} OutputTexture={} CmdList state via requireOpenState check"),
    Desc.OutputWidth, Desc.OutputHeight, Desc.OutputTexture ? 1 : 0);
// ADD:
HLVM_LOG(LogGI, info, TXT("FGIPass::DispatchRays: pre-dispatch CmdList ptr={}"),
    (void*)CmdList);
RTPipeline.DispatchRays(CmdList, Desc.OutputWidth, Desc.OutputHeight, 1, BindingSet);
HLVM_LOG(LogGI, info, TXT("FGIPass::DispatchRays: post-dispatch returned"));
```

This will show whether RTPipeline.DispatchRays is reached and returns. Combined with checking the nvrhi validation layer output, the parent can identify whether the dispatch is being recorded and whether Vulkan drops it.

- diff_estimate: +0 lines (this v2 cycle deliberately does NOT apply a speculative patch)
- skip_plan_review: no — this is a meta-cycle (investigation cycle, not fix cycle)
- test_strategy: parent must add diagnostic logs, rebuild, and run with `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8`. Capture the fresh log and report back.
- risks:
  - The actual fix is unknown until diagnostic data is captured. Speculative patches based on file-only analysis have a high chance of introducing new bugs.
  - The cron is not the right tool for this final-stage diagnosis without terminal access. Interactive debugging with the user (or a cron with terminal toolset) is required.