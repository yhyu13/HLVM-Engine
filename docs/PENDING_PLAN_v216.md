# Pending Plan v216

- task: Re-derive the terminal blocker's mechanism; test v215's prescribed remedy before the operator spends effort on it
- source: no bundle — direct investigation of `~/.hermes/config.yaml`, `~/.hermes/cron/jobs.json`, filesystem probes
- approach: v215 refuted 562 ticks of "file-only host / pipeline dormant" and re-diagnosed the blocker as
  "missing `tirith` binary → `tirith:unknown` → `approvals.mode: manual` → no human in cron → 60s expiry".
  It then prescribed: *install `tirith`, or set `security.tirith_enabled: false`, or reconcile `cron_mode`*.
  **That remedy was never tested against the rest of the config block it was read from.** This cycle reads the
  full `security:` and `approvals:` blocks and asks whether the prescribed remedy is necessary, sufficient,
  or already-present-and-failing. No engine source is touched.
- diff_estimate: +0 / -0 source; markers + health doc only
- skip_plan_review: no
- test_strategy: file-only verifier — every load-bearing claim re-derived by an independent query, each zero
  controlled by a same-shape positive (v205 rule), no `|` alternation (tick-526 rule), `path` at a directory
  where a negative is load-bearing (v199 rule, both directions per v215)
- risks: the lineage's dominant failure mode is *inheriting* a prior tick's conclusion. v215 is one tick old and
  its findings are the most cited in this marker; every one of them must be re-derived here, not cited.
