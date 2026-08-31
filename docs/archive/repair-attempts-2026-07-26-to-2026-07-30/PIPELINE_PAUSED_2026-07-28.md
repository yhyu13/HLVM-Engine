# Pipeline Paused — 2026-07-28 (v84 deadline-pause)

## State
The six-role pipeline is **self-paused**. v83 wrote `PIPELINE_AWAITING_PARENT_2026-07-28.md`
with an explicit v84 deadline: if no parent reply arrived by v84, v84 would write
`PIPELINE_PAUSED_2026-07-28.md` and the cron pipeline would self-pause.

v84 has arrived (66th cumulative file-only tick v25-v84). No parent reply has arrived
in the intervening window. The v82 PARTIAL_KEEP recommendation to "self-pause if no
parent reply by v84" is now fully executed.

## Why pause, not another tick

Per the gpu-rendering-bisect-debug skill's anti-pattern #1 (single-profile + structural
terminal block) and the v82 PARTIAL_KEEP verdict:

- The 65 consecutive file-only standby ticks (v25-v83) have produced zero new actionable
  evidence per cycle. Continuing the same pattern for v85-vN would not produce diagnostic
  value — it would repeat the failure mode the cron's prompt and the gpu-rendering-bisect-debug
  skill jointly prohibit ("rebuild from ash" / fabricated KEEP smoke-screen).
- Terminal access remains structurally blocked in this cron's runspace (every probe
  this tick — `pwd`, `ls`, `date`, `echo`, `bash fresh-evidence-scan.sh` — rejected
  with `pending_approval: tirith:unknown`).
- The newest dumps directory stamp remains `20260727_000706`-`000708` (37+ hours old).
- The newest log is `TestReSTIR_GI_Temporal.log:76` from 2026-07-27 00:07, still showing
  the `gi_raw R[0.000,0.000] G[0.000,0.000] B[0.000,0.000]` symptom the user originally
  reported.
- The cumulative 22-patch inventory is intact and waiting (verified at v83 cross-tick:
  v41 alpha-encoder at FImageDump.cpp:27, v28 GIPathTracing.hlsl:694 sentinels, v22
  UAVBindingLayout at FGIPass.h:106, etc. — all present).

## What this means

The inner six-role pipeline's `enabled_toolsets: ["terminal", "file"]` is declared
but tirith continues to gate the terminal half. v84 is the deadline-bounded gate
named by v83. The pause is the honest exit for a structurally-terminal-blocked cron
that has produced all the file-only diagnostic value it can.

The next interactive parent session can resume the work by:

1. Running the 4-command recipe in `docs/PIPELINE_BLOCKER_2026-07-28.md`
   (`bash fresh-evidence-scan.sh` + `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild`
   + `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal`
   + `python3 validate_restir_gi.py`).
2. Pasting the output back to the cron.
3. The cron will then route to one of three branches per the decision matrix in
   `PIPELINE_BLOCKER_2026-07-28.md` § "What the cron will do with the four pieces of evidence":
   - `gi_raw` non-zero + validator 4/4 + vision OK → `PIPELINE_GOAL_DONE_2026-07-28.md`
   - `gi_raw R[0,0]` persists → FIX cycle on a specific residual defect
   - Build fails on `-Werror` cascade → grep cascade recipe, patch all sites, rebuild

## Source-code state at pause

- 22 cumulative patches intact (no v84 source-code modifications, by design).
- PENDING_PICK.md `restir-gi-fix` remains `[ ]` (un-resolved).
- All v25-v83 PENDING_*_v<N>.md markers preserved as audit trail.
- No `PIPELINE_GOAL_DONE_2026-07-28.md` written (final-goal gate FAILED/UNVERIFIED).
- No protected-branch pushes, no Kanban cards, no governance modifications.

## Cron posture

The cron pipeline is paused. The cron's "do not silently stop" requirement was
satisfied by v83's AWAITING_PARENT marker (parent-facing escalation). The cron's
"do not loop indefinitely" requirement is now satisfied by this PAUSED marker.
Together they cover both sides of the cron's instruction.

The cron's post-pause behavior is [SILENT] — no further ticks write to
`PIPELINE_HEALTH_2026-07-28.md` until the parent resumes the work from an
interactive session.

## Linked files

- `docs/PIPELINE_BLOCKER_2026-07-28.md` — the 4-command recipe; this PAUSED is its
  deadline outcome.
- `docs/PIPELINE_AWAITING_PARENT_2026-07-28.md` — the v83 escalation; this PAUSED
  is its v84 outcome.
- `docs/PIPELINE_HEALTH_2026-07-28.md` — running health audit; this tick's v84
  append at the bottom.
- `docs/PENDING_PICK.md` — v84 marked [x] PAUSED, restir-gi-fix remains [ ].
- `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/fresh-evidence-scan.sh`
  — read-only triage script; v43 (unchanged since v32).
