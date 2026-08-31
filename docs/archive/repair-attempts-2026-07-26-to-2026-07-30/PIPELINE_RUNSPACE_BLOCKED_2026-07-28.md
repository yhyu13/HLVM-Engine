# Pipeline Runspace Blocked — 2026-07-28 (v87 escalation, terminal-cron cap)

## What this document is

The six-role pipeline has cycled 69 times (v25-v87 cumulative) in this project. This is the FINAL escalation for "terminal structurally blocked in this cron session." Previous escalations are:

- `PIPELINE_BLOCKER_2026-07-28.md` (v82) — 4-command parent recipe.
- `PIPELINE_AWAITING_PARENT_2026-07-28.md` (v83) — explicit deadline.
- `PIPELINE_PAUSED_2026-07-28.md` (v84) — deadline-pause fired.
- `PIPELINE_CRON_RESUMED_2026-07-28.md` (v85) — parent's fresh "continue" instruction re-engaged cron.
- `PIPELINE_HEALTH_2026-07-28.md` (every tick) — running health audit.

This document is the seventh in that family, and it is the terminal one:

> **The cron cannot satisfy any of the 6 acceptance criteria from this runspace, regardless of how many ticks it runs. The parent must take terminal action. The cron should not wake again until the terminal situation is resolved.**

## Why this is the terminal escalation

The cron's prompt for this cron session says: "autonomous until complete ... continue cycles ... until the acceptance criteria are actually met."

The 6 acceptance criteria are:

1. Debug target builds cleanly.
2. Fresh `HLVM_DUMP_RGI=1` run with `HLVM_RGI_ACCUM>=8`.
3. No `Cannot open a command list that is already open` in fresh log.
4. No Vulkan ERROR / `VUID-VkDescriptorImageInfo-imageLayout-00344`.
5. `python3 validate_restir_gi.py` passes newest stamp group only.
6. Newest display dump visibly contains recognizable non-uniform Sponza geometry with sane exposure.

All six require terminal execution of the test binary. Terminal access is structurally blocked in this cron session: every `terminal` call is rejected by tirith with `pending_approval: tirith:unknown` (verified in 7+ distinct attempts this tick: `pwd`, `date`, `echo`, `ls`, `echo v86-tick`, `git log --oneline -20 ...`, `echo v87-tick`).

The gpu-rendering-bisect-debug skill (§ Workflow note: "full auto" mode) is unambiguous:

> "If a step is blocked, report the blocker. ... Don't fabricate findings. ... Don't skip the verification step."

And the same skill (`§ Don't do these things`):

> "Distrust scalar gates until a human sees the image. ... Open the most recent dump ... yourself."

The parent's instruction "autonomous until complete" presupposes the parent will either (a) watch the run, or (b) post-terminal evidence here. Neither has happened across 69 ticks. Every additional standby tick produces zero new evidence. The cron's "do not loop indefinitely" is binding here.

## What v87 actually produced (the only diagnostic value remaining)

A NEW finding, not in v25-v86's record:

**The `gi_raw R[0,0,0] G[0,0,0] B[0,0,0]` symptom does NOT match the 2026-07-25 fixes' target symptom class.** The 2026-07-25 fixes (`2fab7d6` dump-normalize + `e6b3d52` remove-sentinels + `aa2cc53` ambient + v22 binding-layout split) targeted "GI pass produces magenta noise." The current symptom is "GI pass OutputTexture reads back as literal float 0.0 from staging copy." This is a **different bug class**.

The diagnostic comment at `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp:1695-1703` (in `DumpRGBA32FTexture`) explicitly names the next-debug question: "If gi_raw = 0 but this log shows the right handle, the GI pass's write was dropped by something OTHER than the (now-removed) HLVM-bypass — check v3 ENTER/EXIT to confirm the dispatch body was reached."

The 22-patch cumulative inventory (verified intact at v25-v87) targeted the magenta-noise bug class. **The current all-zeros bug class may not be covered by any of those patches.** This is consistent with v82 PARTIAL_KEEP's and v85 PARTIAL_KEEP_RESUMED's audits but more specific: the parent should treat the current symptom as a potential NEW bug, not as a regression of a fixed bug.

## What the parent should do

Three options, in order of decreasing cost:

### Option A (recommended): reconfigure the cron to grant terminal access, then let the next tick run.
- The cron prompt specifies `enabled_toolsets: ["terminal", "file"]` for GPU repair. The dispatcher's toolset must actually be terminal-enabled in the running session. Verify with `cronjob action="list"` and inspect the cronjob's `enabled_toolsets`.
- If the cron job's `enabled_toolsets` is `["file"]` or missing, the cron will keep burning file-only standby ticks forever. Fix the cronjob config, then re-engage.
- After reconfig, the next tick will be able to run `./Build.sh`, run the binary, capture stdout/stderr, run `validate_restir_gi.py`, and read PNG dumps. Goal gate can move to PASS or to a specific FIX branch.

### Option B: execute the 4-command recipe from `PIPELINE_BLOCKER_2026-07-28.md` from any terminal-equipped session and paste output back.
- Recipe: `bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/fresh-evidence-scan.sh && ./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal 2>TestReSTIR_GI_Temporal_stderr.log && python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py`
- 2 minutes wall-clock on a patched tree.
- The v87 finding above tells the parent which output lines to look for in the run output (v3 ENTER/EXIT logs if they still exist; per-channel min/max of gi_raw; presence/absence of VUID warnings; and the display PNG to visually inspect).
- With the recipe output, this cron's next tick can route directly to a specific FIX cycle on the named residual defect.

### Option C: pause the cron permanently and continue interactive debugging.
- The cron's `deliver` field defaults to `local` (no auto-delivery to chat). To stop future ticks entirely: `cronjob action="pause"` on the HLVM-Engine cron job.
- Continue debugging from any interactive session with `software-development-practices §Path-Tracing / RT Debugging Methodology` (read code → run test → look at dump → form hypothesis → repeat in 5 min). The v87 finding is a head-start for that interactive session.

## Required cron posture change

**If the parent takes no action within the cron's next tick window, the cron should NOT re-engage on this PICK item in this runspace.** The PICK should be updated to reflect the escalation:

- Mark `[x] v87 terminal-blocked escalation (69th cumulative file-only tick v25-v87) — wrote PIPELINE_RUNSPACE_BLOCKED_2026-07-28.md; produced 1 NEW finding (gi_raw=0,0,0 may be a different bug class than the magenta-noise the 22-patch inventory fixed); 6 acceptance criteria UNVERIFIED in this runspace; cron posture: terminal-blocked.`
- Mark `restir-gi-fix` as `[ ] parent evidence required (terminal action from parent per Option A or B above)`.

The cron's "do not silently stop" is satisfied by this `RUNSPACE_BLOCKED` marker: the cron has documented the block, named the escalation path, and produced one new diagnostic finding. The cron's "do not loop indefinitely" is satisfied by NOT running another standby tick on `restir-gi-fix` until the parent supplies terminal evidence.

## Linked files
- `docs/PIPELINE_BLOCKER_2026-07-28.md` — original 4-command recipe (still applicable; Option B above).
- `docs/PIPELINE_AWAITING_PARENT_2026-07-28.md` — earlier escalation marker.
- `docs/PIPELINE_PAUSED_2026-07-28.md` — v84 deadline-pause (superseded by v85 re-engagement, still in history).
- `docs/PIPELINE_CRON_RESUMED_2026-07-28.md` — v85 re-engagement marker (superseded by v87 terminal-blocked).
- `docs/PIPELINE_HEALTH_2026-07-28.md` — running audit; v87 append at bottom.
- `docs/PENDING_PICK.md` — `restir-gi-fix` remains `[ ]` parent-evidence-gated.
- All v25-v87 PENDING_*_v<N>.md markers — preserved as audit trail.
