# Overseer Escalation — 2026-07-30

**Verdict:** Overseer cannot operate. Pause and resolve before next tick.

## Reason
EC-039 (declared-vs-actual toolset discrepancy). The overseer was
configured with shell access (`terminal`) but tirith blocked every
terminal invocation this tick with `pending_approval: tirith:unknown`,
including the Stage 1 step 1 probe (`terminal command="date"`).

## Card under watch
`t_7b79c010` — Continue GBuffer SRV binding bisect in
`TestReSTIR_GI_Temporal`. Card body has
`AUTO_RESOLVE_DO_NOT: yes` (Hard rule: do NOT auto-resolve even with
opt-in). The verifier instructions in the card body require:
- Debug build (terminal)
- Log scan for Vulkan VUID/ERROR (terminal + grep)
- `HLVM_PT_DEBUG_MODE=20` returns non-zero GBufferMaterial from GI
  shader SRV read (terminal + vision)
- Validator passes newest stamp group (terminal + python)
- Fresh display image (vision)
None of these run without terminal.

## Action required by parent session

Pick ONE:

**(a) Reconfigure overseer.** Verify tirith actually grants terminal
to this profile on this schedule. If not, drop `terminal` from
`enabled_toolsets` and accept that the overseer can only write
comments and state files — it cannot verify anything itself.

**(b) Restructure verification.** Move the "if card is ready,
run one `hermes kanban dispatch` pass" and the verifier checklist
out of cron and into an interactive session or a worktree-pr hook.
The cron then becomes a no-op except for state-file maintenance.

**(c) Pause overseer.** `cronjob action="pause"` until terminal
access is restored. Re-enable from parent session after fix.

## Why I did not silently degrade
Per EC-039: silent file-only fallback produces audit-trail noise
without making progress (the 836-file incident). Per Hard rule #7:
never exit silent. Per the card body itself: "Never fabricate."

So this tick writes the escalation and stops.

## Reference
- EC-039 in kanban-cron-overseer § Edge case registry.
- `docs/archive/repair-attempts-2026-07-26-to-2026-07-30/` (the
  836-file noise incident from this exact cron profile on this
  exact card lineage).
