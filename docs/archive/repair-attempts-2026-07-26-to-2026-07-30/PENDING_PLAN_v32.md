# Pending Plan v32

- task: v32 — structural standby tick + add `fresh-evidence-scan.sh` one-shot helper (parent-action-gated continuation; terminal-block persists). The 7th consecutive evidence-blocked tick is the right moment to consolidate the canonical 10-step parent-triage recipe into a single shell-script entry point.
- source: no bundle — direct disk audit + 1 new helper file
- approach: (1) Write `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/fresh-evidence-scan.sh` (75 lines, 1 new file, 0 source-code lines modified) — a single-command evidence-scanner that prints fresh-dump timestamps, newest `stderr.log` line count, presence of `display_frame8.png` / `gi_raw*` / `gbuffer_*` PNGs, v22/v28 patch presence at expected line numbers via grep, and a verdict banner ("fresh-build-evidence-PASS" or "evidence-stale-or-missing"). The script bundles the v31 canonical parent-triage recipe's "evidence-freshness" steps (steps 1, 4, 5, 8) into a single bash file. Parent runs `bash fresh-evidence-scan.sh` and pastes the output back; cron routes from there. (2) Append v32 standby tick to `docs/PIPELINE_HEALTH_2026-07-27.md` recording (a) the new helper file, (b) terminal-probe blockers, (c) cumulative 17-patch inventory unchanged from v31, (d) updated parent-triage recipe (one-command bootstrap). (3) Update `docs/PENDING_PICK.md` to mark v32 [x] and re-stage v33 as parent-evidence-gated continuation. (4) Write 6 markers following discipline.
- diff_estimate: +1 new file (fresh-evidence-scan.sh ~75 lines) / -0 source-code lines. Total file-system footprint: 1 new shell script + 6 marker files + 1 PICK update + 1 PIPELINE_HEALTH tick append.
- skip_plan_review: no — documentation + new helper file changes still follow marker discipline
- test_strategy: cron file-only (Part A static audit). Part B runtime gate is parent-driven (parent runs the script).
- risks: low. The script is read-only (no `rm`, no `mv` of originals; only `cp` to /tmp). The script is shell-only and uses `grep`/`stat`/`find` — no GPU, no compilation, no permission-gated ops. The 75-line size keeps it reviewable in 2 minutes. The single-head caveat applies (same cron wrote the plan and will write review); the verifier can re-read the script and confirm structure.

## Why this cycle is documentation + helper-script (not pure standby)

Per the v31 audit's verdict (`docs/PENDING_TEST_AUDIT_v31.md` and `docs/PIPELINE_HEALTH_2026-07-27.md`):

> "v31 cycle is complete. v32 is parent-evidence-gated per PICK's re-staging. The pipeline's file-only work space remains exhausted: v22 binding-layout-split is the load-bearing corrective candidate already on disk; v28 alpha-channel sentinel is the last meaningful file-only diagnostic-surface expansion already on disk; everything past v31 requires parent-driven terminal access."

The v31 verdict was correct on the renderer-fix front: every conceivable binding-layout / RT-shader diagnostic is already on disk in BOTH Private master and data-dir HLSL copies, and v22 binding-layout-split is intact. The verdict was conservative on the orchestration front: parent still has to perform a multi-step recipe (rebuild + run + dump + validate + alpha-check + vision). Each step is small but the recipe accumulates operator-time.

A freshness-scan helper that collapses 4 of those steps (steps 1, 4, 5, 8 of v31's parent-triage recipe) into one bash invocation reduces the bar to a single command + a single paste-back. This is the kind of file-only work that the cron's prompt authorizes ("terminal-enabled crons may... roles must still produce its marker and never claim success without evidence") without crossing into fabrication.

## What this plan does NOT do

- Does NOT introduce a corrective renderer fix (would require parent terminal to verify).
- Does NOT introduce another diagnostic sentinel (v28 alpha-channel sentinel is the meaningful last expansion; HLSL probe ladder is fully populated).
- Does NOT commit, push, archive, pause, create Kanban cards, or modify governance.
- Does NOT fabricate parent evidence.
- Does NOT modify any source code (only adds 1 new helper script).

## Plan Deviations (impler fills this in if it deviated)

None. +1 new file (75 lines) + 0 source-code modifications.
