# Pending Plan v219

- task: Determine why `terminal` is refused in this cron session, from the AGENT SOURCE rather than
  from config values — 567 ticks have prescribed operator remedies derived by reading config keys
  and never once read the code that consumes them.
- source: `/home/hangyu5/Documents/Gitrepo-My/hermes-agent` (the running agent's own source tree)
- approach: Read `tools/approval.py`, `tools/tirith_security.py` and `cron/scheduler.py` and trace the
  actual decision path a `terminal` call takes, end to end, until the branch that produces the
  observed result dict is identified by its field set. Do not infer from config keys; config keys
  are inputs to that path, and v215/v216 both prescribed remedies without knowing which branch reads
  them.
- diff_estimate: +0 / -0 source lines (diagnostic cycle; the defect is not in this repo)
- skip_plan_review: no
- test_strategy: every load-bearing claim to be re-derived by the tester with a file-scoped query and
  a same-shape positive control; no claim to rest on a directory-scoped search (see risk 2)
- risks:
  1. The prior lineage's config-level conclusions may be inherited rather than re-derived. Re-read
     every value first-hand.
  2. **`search_files` with `path` at a dotfile directory returns 0 for tokens that provably exist.**
     Discovered this tick: `path=~/.hermes pattern="tirith_enabled"` → 0 hits, while
     `path=~/.hermes/config.yaml` (same token, file-scoped) → 1 hit at `:484`. This is a THIRD false-zero
     class after tick-526's alternation bug and v217's project-root scoping. Every `~/.hermes` negative
     recorded by ticks 563-566 is vacuous and must be re-derived file-scoped.
  3. The remedy must be validated against BOTH candidate causes if the evidence cannot separate them.
     A remedy that fixes only one is what v215 shipped and v216 retired.
