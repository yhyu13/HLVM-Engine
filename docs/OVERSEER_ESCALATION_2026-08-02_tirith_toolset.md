# Overseer Escalation (per-tick) — 2026-08-02 — EC-039 (tirith toolset denial)

This file complements `docs/OVERSEER_ESCALATION.md` (2026-08-01) with the
specific 2026-08-02 tick evidence. The 2026-08-01 escalation listed three
parent options; as of this tick, **none of the three has been acted on**, so
the same EC-039 failure mode persists.

## What happened this tick (2026-08-02)

- Independent terminal probes — `terminal command="date"`, `pwd`, `touch`,
  `ls`, `cat` — ALL denied by tirith with `pending_approval: tirith:unknown`,
  `pattern_key=tirith:unknown`, exit_code=-1. (4+ invocation shapes,
  same rejection signature.)
- The cron's `enabled_toolsets: ["terminal", "file"]` field declares
  terminal access; tirith denies it on every scheduled tick invocation.
- No new evidence on card `t_7b79c010` was gathered this tick: cannot list
  the board, cannot read the card body or comment thread via shell, cannot
  run `./Build.sh`, cannot invoke `HLVM_PT_DEBUG_MODE=20`, cannot run the
  validator, cannot vision the display image.

## Card state

- Card `t_7b79c010` body contains `AUTO_RESOLVE_DO_NOT: yes` (EC-035, EC-037
  body-exemption hard veto). Even with full shell access, the cron would
  NOT auto-resolve this card. The opt-in marker machinery is not probed
  on this card.
- AUTO_RESOLVE_DO_NOT is the WINNING instruction regardless of opt-in.
- No commit, push, merge, git-history rewrite performed (user directive).
- No `requires_human` or `blocked` touches (user directive).

## Why I'm escalating immediately instead of ticking through

- The 2026-07-26 HLVM-Engine `TestReSTIR_GI_Temporal` 836-file noise
  incident (EC-039 precedent) was caused by a cron faithfully obeying its
  prompt while the prompt's promised terminal access was structurally false.
  Silent file-only fallback produced 836 audit-trail markers over 4 days with
  zero real work.
- The cron's prompt body requires `terminal` (to inspect git, run Build.sh,
  invoke the test executable, set env vars, vision the display). File-only
  cannot do any of this. File-only ticking is the failure mode.
- Per EC-039 protocol: file the escalation, write the health entry, exit clean.

## Parent-session actions (in priority order)

1. **Read** this file on next login.
2. **Pick one**:
   - **Option A (reconfigure)**: Reconfigure the cron profile to actually
     grant terminal access. BEFORE recreating the cron, verify with one
     MANUAL `terminal command="date"` invocation in this chat. If it returns
     `pending_approval: tirith:unknown`, file-only is the only honest
     toolset — pick Option B or C instead.
   - **Option B (restructure)**: Move the Build.sh / log inspection /
     `hermes kanban dispatch` requirements OUT of the cron (into a parent-
     driven interactive loop using software-development-practices
     §Path-Tracing / RT Debugging Methodology). File-only cron = appropriate
     for text-only inspection of logs, docs, source reads.
   - **Option C (pause + interactive)**: Keep the cron paused (current state
     since 2026-07-30). Run the verification interactively in this chat per
     the gpu-rendering-bisect-debug skill body. Re-enable the cron after the
     bisect resolves with whatever new evidence the interactive work surfaces.
3. **If you pick Option A**: run `./Build.sh --Config=Debug
   --Target=TestReSTIR_GI_Temporal --Rebuild --Test` then
   `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 HLVM_PT_DEBUG_MODE=20
   Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal` and vision the
   fresh gi_raw PNG. If non-zero GBufferMaterial values appear, the v101
   patch fixed the SRV binding and the bisect closes; complete or close
   the card. If still zero, the v126 plan's Step 1..4 work is needed.

## Files touched this tick (no card state, no git ops)

- `docs/OVERSEER_HEALTH_2026-08-02.md` — appended second tick entry
  (audit trail).
- `docs/OVERSEER_ESCALATION_2026-08-02_tirith_toolset.md` — this file
  (the missing promised file from tick 1, now actually created).

## Reference

- `docs/OVERSEER_ESCALATION.md` — 2026-08-01 escalation (3-option tree still open).
- `docs/OVERSEER_SELF_PAUSE.md` — 2026-07-30 self-pause (Mode 1, terminal state).
- `docs/OVERSEER_HEALTH_2026-08-02.md` — today's tick log.
- `docs/DIAGNOSTIC_2026-07-30.md` — parent-driven bisect in progress.
- `docs/PENDING_PLAN_v126.md` — Step 0 mtime recipe (blocked behind terminal).
- kanban-cron-overseer skill: § Edge case registry EC-039 (declared-vs-actual
  toolset discrepancy), § Hard rules #1, #2, #5, #7, #8 (no commits, no
  pushes, no merges, no card state changes, no self-modification, no silent
  exit).
