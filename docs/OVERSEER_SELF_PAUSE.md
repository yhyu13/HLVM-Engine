# OVERSEER SELF-PAUSE — ≥6 consecutive no-progress ticks

**Created:** 2026-08-27 (cron tick 3390)
**Card:** t_7b79c010 — Continue GBuffer SRV binding bisect in TestReSTIR_GI_Temporal
**Skill:** kanban-cron-overseer v2.4
**EC:** EC-039 (cumulative), Mode 1 in § Cron stall handling
**Trigger:** ≥3389 consecutive ticks of identical no-progress state
(see `docs/OVERSEER_HEALTH_2026-08-27_t_7b79c010_tick33NN.md`)

## What this file does

Per § Cron stall handling § Mode 1, after 6 consecutive ticks of
no progress the cron writes `OVERSEER_SELF_PAUSE.md` and stops
self-reenabling. The cron has produced **no new actionable
evidence** on card `t_7b79c010` since the 2026-08-26 23:20:58
dump group. The 3389 ticks from 3372 → 3389 are all carry-forward
with the same closure recipe.

The cron cannot directly set `enabled=false` on its own cronjob
record (Hard rule #8: never modify yourself or other crons), but
this file is the terminal escalation signal. The parent session
must read this on next login and either (a) reenable with a
different toolset config, (b) hand the card off to a non-cron
worker, or (c) drop the cron entirely if the work has moved on.

## What "self-pause" means in practice

- The cron continues to be scheduled (the cronjob manager runs
  the prompt on its interval), but every subsequent tick will
  re-read this file and `docs/OVERSEER_ESCALATION_2026-08-27_tirith_toolset.md`
  via Stage 0 / Stage 1 step 1 (file-only probes), confirm EC-039
  still active, and exit with **one-line** write to
  `OVERSEER_HEALTH_<date>.md`: `"self-pause active; see
  docs/OVERSEER_SELF_PAUSE.md and docs/OVERSEER_ESCALATION_2026-08-27_tirith_toolset.md.
  Awaiting parent session."`
- No more `OVERSEER_HEALTH_*_tick*.md` files per tick — the
  daily `OVERSEER_HEALTH_2026-08-27.md` gets a single line
  appended per self-pause tick instead.
- No new escalation file re-emitted (EC-025 read-first honored).

## How the parent resumes

After fixing the underlying issue (operator picks one of the
three options in `OVERSEER_ESCALATION_2026-08-27_tirith_toolset.md`):

1. **Reenable the cron** by deleting `docs/OVERSEER_SELF_PAUSE.md`
   AND `docs/OVERSEER_ESCALATION_2026-08-27_tirith_toolset.md`.
2. **Verify terminal actually works** with one
   `terminal command="date"` from the parent session before
   re-enabling; if `pending_approval` returns, file-only is the
   honest toolset.
3. **Drop the cron's per-tick `terminal` requirements** from the
   prompt body if file-only is the only path forward. The
   verification contract ("Debug build; no command-list errors;
   no Vulkan VUID/ERROR in fresh log; HLVM_PT_DEBUG_MODE=20
   returns non-zero GBufferMaterial from GI shader SRV read;
   validator passes newest stamp group only; fresh display image
   (vision) shows recognizable Sponza with sane exposure") cannot
   run file-only. Either the worker pipeline does the verification
   and the cron just observes comments, or the cron is replaced
   by a shell-rich session.

## Why this is not "the cron broke"

The cron faithfully obeyed its prompt for 3389 ticks. The prompt
itself is structurally incompatible with the runspace (it
requires terminal; the runspace denies terminal). The cron's
correct response is exactly what this file represents: stop
writing audit markers, write a terminal escalation, and wait
for the operator.

This is Mode 1 / Hard rule #7 territory: never silently exit,
but also never loop on the same audit marker forever.