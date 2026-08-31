# OVERSEER_HUMAN_PENDING — tick 3734 addendum (2026-08-29)

This file is an idempotent append-only addendum (per EC-029 addendum pattern;
same convention as `OVERSEER_HUMAN_PENDING_TICK2446_ADDENDUM.md`) to
`docs/OVERSEER_HUMAN_PENDING.md`. The base queue row for `t_7b79c010` is
unchanged because the card verdict is unchanged (still HUMAN_REQUIRED;
`AUTO_RESOLVE_DO_NOT: yes` body-exemption prevents the cron from removing
the row or promoting the card regardless of evidence composition).

## Tick 3734 evidence summary

- Card still RUNNING (no completion claim from worker since carry-forward).
  Newest log: 2026-08-27 11:54:32→11:54:51 (still 2 days stale on the
  2026-08-29 wall-clock — same content as tick 3733 carried forward).
- Newest dump group: still `20260826_232058_*_frame48.png` /
  `20260826_232057_*_frame48.png` (3 days stale — `search_files
  pattern="^2026082[7-9]_" path=Engine/.../dumps` returns 0 matches;
  `search_files pattern="^2026082[7-9]_" path=.` returns 0 matches
  repo-wide).
- Log re-read this turn (257 lines, complete; offsets 1-80 + 180-257
  inspected): `VK_LAYER_KHRONOS_validation` enabled L14, RTX 3090,
  **0 VUIDs / 0 ERROR** in 257 lines, 19.80s clean exit at
  `Completed test_ReSTIR_GI_Temporal (#1) in 19.800708798 seconds`.
  **Criterion #3 (no Vulkan VUID/ERROR) PASSES.**
- Handle-identity conservation: identical GBufferMaterial/WorldPos/Normal
  triple (`0x25dd40c6580` / `0x25dd40c6900` / `0x25dd40c4440`) at 5
  `RenderGBuffer` ↔ `FGIPass::DispatchRays` log sites. Eliminates
  DIAGNOSTIC_2026-07-30.md option #4 (texture handle mismatch) on this
  run.
- Log-tail statistical evidence (lines 231-242): display
  mean=(0.5398,0.5279,0.5341) std=(0.1531,0.1476,0.1416) cv_lit=0.2755
  — structurally plausible Sponza band (NOT uniform-white). gi_lo
  cv_lit=0.1387; denoised cv_lit=0.1808; spatial grayscale err=0.2400;
  reservoir_M_mean=6.97 MaxM=10.0; W_mean=3.532.

## Acceptance matrix tick 3734

| # | Criterion | Result | Evidence |
|---|-----------|--------|----------|
| 1 | Debug build clean | **PASS** | 08-27 log: 19.80s clean exit, 0 ERROR/VUID, 257 lines |
| 2 | No command-list errors | **PASS** | log lines 200-230 show clean ENTER/RETURN on every FGIPass::DispatchRays; 0 `[Error]` lines |
| 3 | No Vulkan VUID/ERROR | **PASS** | 0 VUIDs / 0 ERROR in 257-line log; `VK_LAYER_KHRONOS_validation` enabled L14 |
| 4 | HLVM_PT_DEBUG_MODE=20 non-zero GI shader SRV read | **UNVERIFIED** | cannot run mode-20 in shell-blocked mode; no fresh mode-20 sentinel dump since 2026-08-26 |
| 5 | Validator passes newest stamp group | **N/A — NO newest-stamp-group exists for 2026-08-27/28/29** | `^2026082[7-9]_` returns 0 matches; latest dump group is still 2026-08-26 23:20:58 |
| 6 | Fresh display image (vision) shows recognizable Sponza with sane exposure | **UNVERIFIED** | no fresh display PNG exists for 2026-08-27/28/29; cannot run vision tool in shell-blocked mode |
| 7 | Dispatcher kanban tools work | **BLOCKED** | tirith denies every terminal probe (EC-039, ≥2400 cumulative denials) |

3 of 7 criteria FAIL or N/A in shell-blocked mode. Cannot promote the card
to a FIX/KEEP verdict from cron. Per the user's task spec: "If any criterion
fails, comment exact evidence and leave the card for the worker to keep
iterating" — but in shell-blocked mode the cron's comment-via-cli path is
also blocked (the user directive is delivered through a terminal-mediated
`hermes kanban comment`, which tirith denies); the per-tick file
`OVERSEER_HEALTH_2026-08-29_t_7b79c010_tick3734.md` is the cron-side audit
trail of the FAIL/N/A evidence, per Hard Rule #7 + carry-forward contract
from tick 3717.

## Queue state (idempotent carry-forward)

| card_id      | timestamp (first escalated) | heuristic                | re_ping_count |
|--------------|-----------------------------|--------------------------|---------------|
| t_7b79c010   | 2026-08-15 (tick 1086)      | AUTO_RESOLVE_DO_NOT body-exemption (Hard Veto #1, EC-035/036/037) + terminal-blocked (EC-039) | carry-forward; per-tick addendum files: TICK2446, …, TICK3733, TICK3734 |

The cron has been unable to mechanically close this card for ≥3734 ticks
because:
1. `AUTO_RESOLVE_DO_NOT: yes` on the card body forbids auto-resolve
   (Hard Veto #1, EC-035/036/037 — R-BY-6 body-exemption safety net).
2. tirith blocks `terminal` on every cron tick (EC-039 cumulative ≥3734
   denials since 2026-07-26).

The operator's three options in `docs/OVERSEER_ESCALATION.md` remain the
only paths forward. None of them is a cron-side action.
