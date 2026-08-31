# Pending Plan v227

- task: Determine why `terminal` is refused in this cron tick, given that
  `approvals.cron_mode: allow` and `security.tirith_fail_open: true` are
  BOTH already set — the two remedies ticks 563/569 prescribed.
- source: no bundle — direct source read of the approval implementation
- approach: The lineage has ~580 ticks all blocked on the same envelope
  (`pending_approval` / `tirith:unknown` / `exit_code -1`). Ticks 563 and
  569 each prescribed an operator remedy; **both remedies are already
  applied in `~/.hermes/config.yaml` and the block persists.** That is a
  refutation of both, and it means the true cause has never been found.
  Rather than emit a 580th closure doc (`§Anti-patterns §6` drift), read
  `tools/approval.py` and establish, by code path rather than by
  inference, which branch emits this exact envelope and what predicate
  selects it.
- diff_estimate: +0 / -0 engine source (determination cycle)
- skip_plan_review: no
- test_strategy: every load-bearing claim re-derived by the tester with a
  same-scope positive control; per tick-526 no `|` alternation, per v225/v226
  no `output_mode=count` at directory scope.
- risks: the lineage has twice produced a confident remedy that did not
  work. The failure mode both times was reading PART of a code path or
  config block and inferring the rest. Mitigation: trace the branch from
  function entry to the literal return that produces the observed keys,
  and require that the emitted dict's keys match the observed envelope
  field-for-field before accepting the branch as the source.

## Acceptance for this cycle

Not the 7 job gates — those need execution. This cycle's deliverable is a
correctly-identified blocker with a remedy whose mechanism is verified in
code, so that the 581st tick is not another restatement.
