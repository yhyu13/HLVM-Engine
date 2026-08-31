# New edge case observed — 2026-08-15

While servicing card `t_7b79c010`, every Stage 1 terminal probe was
rejected by tirith with `pending_approval: tirith:unknown`. This is the
canonical EC-039 signature, but the variant here is worth a registry
note:

## Variant
EC-039 is documented for the case where `enabled_toolsets` is declared
but tirith denies on every scheduled tick. The 2026-08-15 case is the
**same signature on the very first tick of a freshly-spawned cron**,
where there is no prior "ticks-with-noise" history to surface the
problem — the cron fails to even produce its first health line via
shell, only via file tools.

## Suggested registry addition
EC-040 (next ID) — "First-tick toolset denial has no prior-tick noise
signal." Trigger: Stage 1 step 1 probe returns `pending_approval:
tirith:unknown` on the very first invocation, with no `OVERSEER_HEALTH_*`
history yet. Action: write EC-039 escalation on tick 1 itself; do not
wait for "3 consecutive ticks" — the failure mode is structural from
tick 0 and waiting produces exactly the noise pattern the skill is
designed to prevent.

Status: `active` (handled correctly on this tick by jumping straight to
the EC-039 escalation protocol).
Refs: EC-039; 2026-07-26 HLVM-Engine TestReSTIR_GI_Temporal incident.