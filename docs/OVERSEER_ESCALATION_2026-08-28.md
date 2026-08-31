# Overseer Escalation — 2026-08-28

## Status
**Overseer still cannot operate. Same root cause as 2026-07-30.
Parent session must resolve before any further Overseer Stage 2
verdicts are credible.**

## Reason
EC-039 (declared-vs-actual toolset discrepancy) remains in
effect on every scheduled tick:
- The cron profile declares `enabled_toolsets: ["terminal",
  "file"]`.
- tirith denies every `terminal` invocation with
  `pending_approval: tirith:unknown`, including echo to
  /dev/null, `pwd`, `mkdir`, and the canonical
  `terminal command="date"` probe.
- The verifier checklist for `t_7b79c010` (debug build, VUID
  scan, `HLVM_PT_DEBUG_MODE=20`, validator, vision) cannot
  run without terminal.

## Why this escalation again
The 2026-07-30 escalation asked the parent to pick (a)
reconfigure overseer, (b) restructure verification, or (c)
pause overseer. 29 days later:
- A fresh log (2026-08-27 11:54) exists and ran cleanly,
  which is informative but does not close the bisect.
- The newest dump group is 2026-08-26 23:20 (~36h stale).
- The DEBUG_MODE 20/21/22 modes the diagnostic introduced are
  not visibly exercised in the most recent log.
- No `HLVM_DUMP_RGI=1 HLVM_PT_DEBUG_MODE=20` env-var run is
  visible in any log or stamp dir newer than the diagnostic.
- No new OVERSEER_HEALTH file was created between
  2026-07-30 and 2026-08-28 (this one).

Net effect: the cron has been silently failing for weeks
(because tirith blocks terminal, the cron either exits
without writing OR writes a comment with no real
verification — Hard rule #7 violation).

## What changed that re-opened verification
The 2026-08-27 log shows the test compiled and ran end-to-end
without Vulkan validation errors. Diagnostic hypothesis #4
(handle-mismatch) is empirically refuted for that build. The
remaining open hypotheses from DIAGNOSTIC_2026-07-30 are:
1. slangc dead-stripped the case labels (verify with
   spirv-cross --reflect).
2. Image layout transition wrong (no VUID observed → unlikely
   in the absence of validation errors).
3. Second binding set dropped silently (no evidence either
   way).
4. (now refuted) Handle mismatch.

A `terminal`-enabled tick could probe hypothesis 1 directly.

## Action required (re-stated)
Same three options from 2026-07-30, ordered by surgical risk:
1. **Restructure verification**. Move the verifier checklist
   into the parent session or a worktree hook. The cron then
   becomes a state-file + comment-marker only and stops
   lying about what it can do.
2. **Reconfigure overseer** so terminal actually works at
   cron-scheduled time. This requires confirming tirith will
   honour the grant; the canonical probe is a single manual
   `terminal command="date"` invocation from this profile.
3. **Pause overseer** until the structural problem is fixed.

## Why I do not silently degrade
Per EC-039 lesson: silent file-only fallback produced the
836-file noise incident on this exact cron profile / exact
card lineage. Per Hard rule #7: never exit silent. Per card
body instruction: "Never fabricate."

## Card under watch
`t_7b79c010` — Continue GBuffer SRV binding bisect in
`TestReSTIR_GI_Temporal`. Body has `AUTO_RESOLVE_DO_NOT: yes`.

## Reference
- `docs/OVERSEER_ESCALATION_2026-07-30.md` (prior escalation).
- `docs/DIAGNOSTIC_2026-07-30.md` (open hypotheses).
- `Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal.log`
  (2026-08-27, clean run, refutes handle-mismatch hypothesis).
- `docs/OVERSEER_HEALTH_2026-08-28.md` (this tick's audit).
