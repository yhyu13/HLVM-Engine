# Pipeline Outer Escalation — 2026-07-29

## State

The outer goal-loop watchdog (this skill) has confirmed terminal-blocked
posture across **80+ cumulative inner pipeline ticks** (v25 → v123) plus
this outer tick. This document is the **outer watchdog's escalation**,
parallel to but distinct from the inner pipeline's `PIPELINE_RUNSPACE_BLOCKED_2026-07-28.md`
(v87) and `PIPELINE_PAUSED_2026-07-28.md` (v84).

## Why this is a new escalation

The inner pipeline already wrote three prior escalations:

- `PIPELINE_BLOCKER_2026-07-28.md` (v82) — 4-command recipe.
- `PIPELINE_AWAITING_PARENT_2026-07-28.md` (v83) — explicit deadline.
- `PIPELINE_PAUSED_2026-07-28.md` (v84) — deadline-pause fired.
- `PIPELINE_CRON_RESUMED_2026-07-28.md` (v85) — fresh parent "continue" instruction re-engaged cron.
- `PIPELINE_RUNSPACE_BLOCKED_2026-07-28.md` (v87) — terminal structurally blocked.

The inner pipeline has since cycled through **v88-v123** (36 more cycles)
each producing identical "verification-only, terminal blocked, 0/6 gates
verified" markers. The parent's reply between v87 and v123 was: **none**.
No terminal-equipped session has executed the 4-command recipe. No
terminal-equipped session has reconfigured the inner cronjob's
`enabled_toolsets`. The dumps directory stamp remains
`20260727_000706-08` (40+ hours old); the log file remains the 2026-07-27
00:07 file with `gi_raw R[0,0] G[0,0] B[0,0]` symptom.

This outer watchdog now produces its own escalation because:
1. The inner pipeline's v84 PAUSED + v87 RUNSPACE_BLOCKED were written
   under v87-vintage prompt state. v88-v123 re-engaged the loop without
   resolving the underlying runspace problem.
2. The outer watchdog is the *final goal-gate* role. When the final
   goal-gate cannot evaluate from the runspace for 80+ consecutive
   cycles, the watchdog must surface this independently — not just
   inherit the inner pipeline's prior escalations.
3. Per the kanban-cron-overseer skill § Stage 1 step 8 (stall-loop
   detection), `>80% overlap with previous tick's findings for 3+
   consecutive ticks → write OVERSEER_ESCALATION.md`. This is the
   `OVERSEER_ESCALATION.md` analog for the GPU-repair outer watchdog.

## What the watchdog has confirmed (this tick)

- **Terminal status**: BLOCKED. Every `terminal` call in this outer tick
  returned `pending_approval: tirith:unknown` (4 calls in this tick,
  consistent with 7+ calls in v87).
- **Dumps directory freshness**: STALE. Newest stamp `20260727_000708`
  (40+ hours old). No new group produced by any v88-v123 inner tick.
- **Log freshness**: STALE. Newest log: 2026-07-27 00:07.
- **Marker progression**: v88 → v123 are file-only marker chains. Each
  cycle's PLAN/PLAN_REVIEW/COMMIT/IMPL_REVIEW/TESTS/TEST_AUDIT say
  "0/6 verified; terminal blocked; no source changes; requeue unchanged
  verification-first item."
- **Goal-done possibility**: ZERO. All 6 acceptance criteria require
  terminal execution. The watchdog skill (gpu-rendering-bisect-debug §
  "Don't accept 'PASS' when the symptom is...") prohibits fabricating
  PASS on stale evidence.

## What the parent must do

Three options (inherited from `PIPELINE_RUNSPACE_BLOCKED_2026-07-28.md`,
re-issued here with current evidence):

### Option A (recommended): reconfigure the cron job to grant terminal access

1. `cronjob action="list"` and find the inner six-role pipeline job for HLVM-Engine.
2. Update `enabled_toolsets` to include `"terminal"` (currently file-only
   per the gpu-rendering-bisect-debug skill's documented override requirement).
3. Re-engage. Next tick should be able to run `./Build.sh`, run the
   binary, capture stdout/stderr, run `validate_restir_gi.py`, and read
   PNG dumps.

### Option B: execute the 4-command recipe from a terminal-equipped session

```
bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/fresh-evidence-scan.sh && \
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild && \
HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal \
    2>TestReSTIR_GI_Temporal_stderr.log && \
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py
```

Then paste the output back to the next cron tick. 2 minutes wall-clock
on a patched tree. The cron will then route to one of three branches
per the decision matrix in `PIPELINE_BLOCKER_2026-07-28.md`:

- `gi_raw` non-zero + validator 4/4 + vision OK → `PIPELINE_GOAL_DONE_*.md`
- `gi_raw R[0,0]` persists → FIX cycle on a specific residual defect
- Build fails on `-Werror` cascade → grep cascade recipe, patch all sites, rebuild

### Option C: pause the cron permanently and continue interactive debugging

`cronjob action="pause"` on the HLVM-Engine cron jobs (both inner and
outer). Continue debugging from any interactive session with the
project's normal debugging methodology. The v87 finding
(`gi_raw R[0,0,0] G[0,0,0] B[0,0,0]` may be a different bug class than
the magenta-noise the 22-patch inventory fixed) is a head-start for
that interactive session.

## Required cron posture change

If the parent takes no action within the next outer tick window:

- The outer watchdog will continue to append audit-only ticks to
  `PIPELINE_HEALTH_2026-07-29.md` (hard rule #7 — never silently exit).
- The outer watchdog will NOT write `PIPELINE_GOAL_DONE_*.md` (cannot
  fabricate PASS; no fresh runtime evidence exists).
- The outer watchdog will NOT nudge the inner pipeline (the inner
  pipeline is already exhausted on this runspace per its own
  RUNSPACE_BLOCKED marker).
- The user-facing `OVERSEER_HEALTH_2026-07-29.md` audit will reflect
  "no progress; parent action required."

## Linked files

- `docs/PIPELINE_RUNSPACE_BLOCKED_2026-07-28.md` — v87 terminal-blocked escalation (inner).
- `docs/PIPELINE_BLOCKER_2026-07-28.md` — original 4-command recipe (still applicable).
- `docs/PIPELINE_AWAITING_PARENT_2026-07-28.md` — v83 deadline escalation.
- `docs/PIPELINE_PAUSED_2026-07-28.md` — v84 deadline-pause.
- `docs/PIPELINE_CRON_RESUMED_2026-07-28.md` — v85 re-engagement.
- `docs/PIPELINE_HEALTH_2026-07-29.md` — running audit, including outer tick 17 this turn.
- `docs/PENDING_PICK.md` — top item: `restir-gi-fix-runtime-verification-v124`.
- All v25-v123 `PENDING_*_v<N>.md` markers — preserved as audit trail.
