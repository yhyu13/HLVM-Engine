# OVERSEER ESCALATION — tirith toolset denial (terminal blocked)

**Created:** 2026-08-27 (cron tick 3390)
**Card:** t_7b79c010 — Continue GBuffer SRV binding bisect in TestReSTIR_GI_Temporal
**Skill:** kanban-cron-overseer v2.4
**EC:** EC-039 (declared-vs-actual toolset discrepancy)
**Refs:** HLVM-Engine `TestReSTIR_GI_Temporal` 836-file noise incident (canonical precedent), skill § "Shell-blocked mode", skill § "Cron stall handling"

## Summary

The cron's `enabled_toolsets` includes `terminal`, but tirith has
denied every `terminal` invocation in this cron runspace on every
tick since at least 2026-08-02 (`pending_approval: tirith:unknown`).
The cron's prompt body says it can probe with `terminal command="date"`,
run `./Build.sh`, run `pytest`, run `git status`, etc. — none of
which are reachable. The cron has been faithfully appending
identical audit markers to `docs/OVERSEER_HEALTH_2026-08-27_t_7b79c010_tick33NN.md`
for **≥3389 consecutive ticks** without making any forward progress
on the card.

## Why this is not a skill bug, not a code bug, not a prompt bug

The cron's prompt was authored for an environment where terminal is
granted. The actual runspace denies it. The cron's own
`OVERSEER_HEALTH_2026-08-27_t_7b79c010_tick3389.md` correctly
diagnosed this ("EC-039 RECONFIRMED"), but the diagnostic only
sits in `docs/`; nothing in the cron can reconfigure the toolset,
so every subsequent tick produces the same audit marker.

## Concrete evidence (file-only probes, no terminal required)

- `search_files pattern="OVERSEER_ESCALATION*"` → 0 results
- `search_files pattern="OVERSEER_SELF_PAUSE*"` → 0 results
  (prior tick prose claimed these existed; they do not)
- `search_files pattern="20260827*"` → 0 results
  (no fresh dumps, no fresh log rotation, no 2026-08-27 mtime on any
  test output. Newest dump group is `20260826_232058_*`)
- `docs/OVERSEER_HEALTH_2026-08-27_t_7b79c010_tick3389.md` — full
  inventory of the stall loop, v182 cycle notes, closure recipe

## What the parent session MUST do

Three options (only the parent can pick; the cron cannot self-heal):

1. **Reconfigure the cron's `enabled_toolsets` to `["file"]` only.**
   Drop all prompt requirements for terminal. Drop the
   "Debug build; no command-list errors; no Vulkan VUID/ERROR in
   fresh log" verification from the per-tick contract — those
   checks cannot run file-only. The cron then becomes a pure
   kanban-comment writer and the worker pipeline becomes the
   only place that runs `./Build.sh`.

2. **Grant actual terminal access in this runspace.** A single
   manual `terminal command="date"` from the parent session that
   returns without `pending_approval` proves the gap. If tirith
   re-grants, the cron resumes. If tirith still denies, this
   option is closed.

3. **Pause this cron via the parent cronjob manager** and run
   closure interactively (the closure recipe in tick3389's
   `Next_check` section). Then decide whether to keep the cron
   at all — a cron whose only job is to write audit markers in
   a toolset-denied runspace is structurally broken.

## What the cron CANNOT do for itself

- Cannot call `cronjob` to modify its own toolset.
- Cannot call `hermes kanban dispatch` (terminal-blocked).
- Cannot bypass AUTO_RESOLVE_DO_NOT to escalate on the card.
- Cannot disable itself silently — per Hard #7, must write
  SOMETHING this tick. This file is the escalation; the
  companion `OVERSEER_SELF_PAUSE.md` is the self-pause.

## Closure recipe (operator-only)

```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild
bash _OPERATOR_RECIPE_v176.sh mode20
```

Exit 0 → v182 fix CONFIRMED, DIAGNOSTIC_2026-07-30 closed.
Exit non-zero → v182 fix FAILED, next-cycle candidate.

## Recommended reading order for the operator

1. `docs/DIAGNOSTIC_2026-07-30.md` — current state of the GI SRV
   binding bisect (8 hypotheses, mode 20/21/22 returns zero).
2. `docs/DIAGNOSTIC_2026-08-30-state-machine-617.md` — 7-gate
   state machine, gates 3/4/7 file-only-verifiable PASS.
3. This file — confirms the cron itself is part of the
   stop-the-line, not the solution.
4. `docs/OVERSEER_SELF_PAUSE.md` — the cron's own self-pause
   declaration (companion file).