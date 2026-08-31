# Pending Plan v215

- task: Re-derive the blocker classification that 562 consecutive ticks have asserted, using a corrected search SCOPE
- source: no bundle — direct investigation
- approach: The lineage has emitted 562 closure documents with an identical conclusion resting on three
  load-bearing negative claims: (a) "no `jobs.json` / crontab / `*.cron` on disk", (b) "pipeline DORMANT —
  no cronjob registered", (c) "terminal blocked by tirith EC-039, categorical". Claims (a) and (b) were
  derived by searching the PROJECT ROOT (`/home/hangyu5/Documents/Gitrepo-My/HLVM-Engine`). Cron state does
  not live in the project root; it lives in the Hermes home (`~/.hermes/cron/`). This plan re-runs those
  queries at the correct scope and re-classifies the blocker accordingly. No source file is touched.
- diff_estimate: +0 / -0 source; docs only
- skip_plan_review: no
- test_strategy: file-only verifier — each claim re-derived by a query whose scope is stated explicitly,
  every zero controlled by a same-shape positive (v205 rule), no `|` alternation (tick-526 rule)
- risks:
  - The v205 rule matters most here: an uncontrolled zero is exactly what produced the 562-tick error.
    Every negative in this cycle must carry a positive control at the same scope.
  - tick-526 established that `|` alternation silently returns 0. My first `jobs.json` query used
    `six-role|HLVM|name` and returned 0 — a vacuous result I nearly recorded. Re-run without alternation
    returned the full file. This trap fired once inside this very cycle; it must be called out.
  - Distinguish "terminal is blocked" (a permission verdict) from "terminal approval is pending with
    nobody present to answer" (a liveness condition). These have different operator remedies.
