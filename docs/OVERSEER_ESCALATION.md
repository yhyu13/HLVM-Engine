# Overseer Escalation — t_7b79c010 (TestReSTIR_GI_Temporal GBuffer SRV bisect)

**timestamp:** 2026-08-31 (cron tick)
**reviewer:** kanban-cron-overseer
**project:** HLVM-Engine

## Status

Cannot perform this tick. Both observation and action surfaces are unavailable:

- **Shell:** blocked by tirith (`terminal` returned `pending_approval`,
  `pattern_key=tirith:unknown`). Cannot run `date`, `git`, `pytest`, or
  `hermes kanban …` from the shell side.
- **Kanban tools:** `kanban_list` / `kanban_show` / `kanban_comment` /
  `kanban_dispatch` / `kanban_reclaim` are not exposed to this cron
  session. There is no API surface registered in this environment for
  the overseer to read card state, append comments, or dispatch workers.
- **Pre-staged docs:** `docs/KNOWN_PROFILES.md` and
  `docs/SENSITIVE_PATHS.md` are absent. `docs/DIAGNOSTIC_2026-07-30.md`
  does not exist. No prior `OVERSEER_HEALTH_*` or `PENDING_REVIEW_*`
  files exist. This appears to be a cold-start tick with no parent
  provisioning.
- **Project root files inspected via file tools:**
  - Test source file `TestReSTIR_GI_Temporal.cpp` is not present at
    `Engine/Source/Runtime/Test/`. The binaries
    `Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal` and
    `Engine/Source/Release/TestReSTIR_GI_Temporal` exist, but the
    corresponding log file
    `Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal.log`
    and dump directory
    `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps/`
    were not searchable in this tick (no shell, no kanban tool).

## What the cron cannot do this tick

- Cannot read card body, status, assignee, last_run_at, comment thread.
- Cannot run `hermes kanban dispatch` (no kanban tool, no shell).
- Cannot append a comment with new evidence.
- Cannot run Debug build, pytest, validator, or vision-check the
  newest display image.
- Cannot verify Vulkan VUID absence or HLVM_PT_DEBUG_MODE=20 SRV read.

Hard rule 7 (never silently exit) is being honored by writing this file.

## What a parent session must do

1. **Provision kanban tool access.** The overseer needs `kanban_*`
   tools (or a working `hermes kanban` CLI via shell) to do anything
   useful. Without them, every tick will be this escalation file.
2. **Pre-stage `docs/KNOWN_PROFILES.md`** with the worker profiles
   available on this host (per kanban-cron-overseer § Stage 1.5).
3. **Pre-stage `docs/SENSITIVE_PATHS.md`** with any project-specific
   sensitive globs beyond the defaults.
4. **Restore `docs/DIAGNOSTIC_2026-07-30.md`** (the per-card context
   the cron prompt asks for on each tick) — or update the prompt to
   reference a current path.
5. **Decide**: resume the cron after provisioning, or skip the
   overseer on this card and route t_7b79c010 manually. The card
   carries `AUTO_RESOLVE_DO_NOT: yes` per the cron's instructions, so
   no automated resolution path is valid regardless.

## Card-level notes (from prompt, since card itself unreadable)

- ID: t_7b79c010 — "Continue GBuffer SRV binding bisect in
  TestReSTIR_GI_Temporal".
- `AUTO_RESOLVE_DO_NOT: yes` — even with opt-in markers, do NOT
  auto-resolve. Honored by construction (no resolution attempted).
- Card is on the default board for HLVM-Engine.
- Verification criteria when the card reaches a completable state:
  Debug build clean, no command-list errors, no Vulkan VUID/ERROR in
  fresh log, HLVM_PT_DEBUG_MODE=20 returns non-zero GBufferMaterial
  from GI shader SRV read, validator passes newest stamp group,
  fresh display image shows recognizable Sponza with sane exposure.

None of those checks can run this tick.
