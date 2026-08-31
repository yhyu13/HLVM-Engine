# Overseer Escalation — 2026-08-14

## Status: EC-039 declared-vs-actual toolset discrepancy

The kanban-cron-overseer cron for card `t_7b79c010` cannot
execute its required acceptance workflow on this host because
the cron's `enabled_toolsets: ["terminal", "file"]` declaration
is structurally false: every `terminal` invocation (including
the Stage 0 `date` probe, `git status`, `git rev-parse`, and
`hermes kanban show`) returns
`pending_approval: tirith:unknown` and is silently denied.

The cron's prompt body says it must:

1. Probe shell — `terminal command="date"` → BLOCKED
2. Inspect `git status` (no commits) → BLOCKED
3. Read newest dump group under
   `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps/`
   → file tools worked, listed via search_files
4. Read fresh `Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal.log`
   → file tools worked, read via read_file (273 lines, 50 KB)
5. Read `docs/DIAGNOSTIC_2026-07-30.md` → file tools worked
6. If ready, run `hermes kanban dispatch` → BLOCKED
7. If worker claimed completion, independently verify acceptance
   (Debug build, no Vulkan VUID/ERROR in fresh log,
   HLVM_PT_DEBUG_MODE=20 returns non-zero GBufferMaterial from GI
   shader SRV read, validator passes newest stamp group, fresh
   display image via vision is recognizable Sponza with sane
   exposure) → BLOCKED (build, run test binary, run debug modes,
   run validator, vision-analyze all require terminal or vision)

Items 3, 4, 5 (file-only inspection) succeeded. Items 1, 2, 6, 7
all returned `pending_approval: tirith:unknown`.

## What the parent session must do

Per EC-039 step 3, the parent session must take one of three
actions:

**(a) Reconfigure the cron profile** to actually grant terminal
access. This is not a config-file edit — it is a tirith policy
change. The cron cannot self-heal this.

**(b) Restructure the cron's work** so it does not need terminal
in cron. For this card that means moving the acceptance checks
(build, run, debug modes, validator, vision) into a separate
verification step that the parent session runs interactively,
while the cron only does board-health sweeps and comment routing.

**(c) Pause the cron** and run the verification interactively
from the parent session, then resume the cron when verification
is settled.

## What this cron tick did instead

Per EC-039 step 1, `toolset_requested=terminal,actual_blocked_by=tirith`
written to `docs/OVERSEER_HEALTH_2026-08-14.md`. File-only
inspection of the fresh log captured evidence the parent session
can act on: Vulkan device created cleanly, no VUID/ERROR strings,
7 GI dispatch frames ran, test completed in 21.8s, gi_raw pixel
stats are in light-energy range (mean [0.13, 0.13, 0.15]) not
solid black, ReSTIR reservoirs populated.

The diagnostic file `docs/DIAGNOSTIC_2026-07-30.md` (from a
previous session) records that HLVM_PT_DEBUG_MODE=20/21/22
returned solid black on the previous run and asks the next
session to do the binding-handle-identity bisect. That bisect
cannot be executed from a shell-blocked cron. The parent
session owns it.

## Card body constraints honored

Card body contains `AUTO_RESOLVE_DO_NOT: yes` per the user's
task brief. Per EC-035 / EC-037, the body wins — no opt-in
marker would authorize auto-resolve on this card regardless.
This tick did not attempt auto-resolve, did not issue any
verdict (KEEP/FIX/DELETE/HUMAN_REQUIRED), and did not modify
card state.

## Refs

- EC-039 (declared-vs-actual toolset discrepancy, 2026-07-26
  HLVM-Engine TestReSTIR_GI_Temporal 836-file noise incident)
- EC-035 / EC-037 (AUTO_RESOLVE_DO_NOT body exemption)
- Hard rule #1 (never auto-merge to protected branches)
- Hard rule #7 (never silently exit)
- Hard rule #8 (never modify self or other crons)