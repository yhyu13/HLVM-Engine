# Pending Plan Review v4

- plan: docs/PENDING_PLAN_v4.md
- verdict: KEEP
- reviewer: planner+plan-criticer (single-head autonomous cron — see software-development-practices §"Full auto" anti-pattern #7 caveat; gating is mechanical evidence not independent judgment)
- timestamp: 2026-07-27T03:30:00Z (estimated; cron tick wall clock)

## Design soundness

The v4 plan correctly identifies the fundamental constraint: **without terminal access in the cron, no fix can be applied and verified autonomously**. The previous cycles (v1, v2, v3) all hit this wall. v4 splits the work into two parts:

1. **v4a (diagnostic upgrade)** — adds ONE more info-level log at the dump's `setTextureState(OutputTexture, CopySource)` site. This is the ONE missing log that distinguishes "GI pass wrote to OutputTexture correctly" from "GI pass wrote but the dump reads from the wrong layout/stale storage." All other relevant sites (GIPass::DispatchRays ENTER/EXIT, Pre/Post-GIPass, post-waitForIdle) already have v3 logs.

2. **v4b (conditional fix)** — proposes REMOVING the HLVM-bypass close+execute+waitForIdle+open flow at lines 1516-1531 of TestReSTIR_GI_Temporal.cpp. The hypothesis is grounded in the empirical timeline:
   - 2026-07-25: test was working (per SESSION_HANDOFF).
   - 2026-07-27 00:07: gi_raw = (0,0,0) after v1's bug-088 fix.
   - The regression window is v1, which added the HLVM-bypass code.

The plan explicitly gates v4b on v4a's log evidence. This is the correct shape: a speculative fix cannot be justified without runtime data, and v4a is the minimum runtime data needed to justify v4b.

The plan does NOT propose:
- Reverting the bug-088 fix at line 675 (`executeCommandList` at end of Render) — that's correct and is what makes the post-raster pipeline work.
- Reverting the bug-075 binding-layout split — that's already verified working from a prior session.
- Changing FGIPass's binding layout — the layout is correct (u0 = OutputTexture matches the HLSL `register(u0)`).

## Plan completeness

- Missing files: none. The diagnostic (v4a) touches FGIPass.cpp (already a member of v3's set). The conditional fix (v4b) touches only TestReSTIR_GI_Temporal.cpp lines 1516-1531.
- Missing edge cases: v4a's log helper needs to be able to read nvrhi's tracked state for OutputTexture. If that's not directly available, the plan falls back to logging the texture handle + frame index and letting the v3 logs correlate. The fallback is documented.
- Missing acceptance criteria: the plan provides 6 acceptance checks for v4b (gipass dispatch returned, state tracked, HLVM-bypass removed, build, gi_raw non-zero, validator 3/3, vision display). All concrete and mechanically checkable.

## Feedback for planner (FIX only)

None — v4a is correct, v4b is correctly gated on v4a, validator (v4c) is unchanged and already verified.

## Honest assessment

This is the 4th cycle of the pipeline. v3 was diagnostic-only (KEEP/SOME_RELAX). v4 is a diagnostic upgrade with a conditional fix — also gated on parent verification. The pipeline is doing the right thing: not landing speculative fixes, building diagnostic evidence, and being honest about the terminal-blocked constraint.

The pipeline will appear to "stall" until the parent runs v3 (or v4a) and captures the log. That's not a stall — that's the correct behavior. The pipeline is waiting on data it cannot produce.

If the parent session is monitoring this output, the correct next action is:
```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test
cd Engine/Source/Runtime/Binary/Debug
HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 timeout 180 ./TestReSTIR_GI_Temporal
# Capture the log: TestReSTIR_GI_Temporal.log
# Vision-analyze the new dumps/dir/2026*_display_frame8.png
cd Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data
python3 validate_restir_gi.py
# Expect (with v3 patches): 0/3 (gi_raw still 0; fix not landed yet)
# Paste log lines matching: Pre-GIPass, Post-GIPass, FGIPass::DispatchRays ENTER/EXIT, RenderGBuffer post-waitForIdle, gi_raw normalized per-channel
```

## Note on the conditional v4b

v4b's hypothesis is grounded in mtime chronology, not in code review. The 2026-07-25 SESSION_HANDOFF explicitly documents the test as working with `e6b3d52` (WriteGBufferSentinels removal). v1 (2026-07-27 00:07) introduced the HLVM-bypass close+execute+waitForIdle+open flow. v2 (2026-07-27 01:05) reverted its speculative patch. v3 (2026-07-27 02:10) added diagnostic logs.

If v4a's log shows the GI dispatch DID execute (ENTER + EXIT both fire), AND gi_raw still = 0, then the HLVM-bypass is the regression. Removing it should restore 2026-07-25 behavior.

If v4a's log shows the GI dispatch did NOT execute (ENTER missing, or early-return fired), the bug is upstream and v4b is the wrong fix. The plan correctly handles this case by NOT landing v4b unless v4a's evidence supports it.