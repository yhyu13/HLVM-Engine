# Pending Plan v222

- task: Determine why `terminal` is refused, from the RUNNING agent source rather than from config
  inference — and decide whether any remedy exists that this job may legitimately apply.
- source: no bundle — direct source read of the hermes-agent tree + `~/.hermes/config.yaml`
- approach: v215 through v221 all reasoned about the block from `~/.hermes/config.yaml` plus a
  *partial* read of `tools/approval.py`. Every one of them concluded with an operator action on
  config. None of them ever established that the observed tool envelope is actually PRODUCED by the
  code path their remedy targets. That link is the whole plan. Take the four fields the runtime
  actually returns (`status`, `pattern_key`, `description`, `allow_permanent`) and find, by direct
  contiguous read, the unique source expression that emits each one. Then check whether the branch
  the standing remedy targets is upstream or downstream of that expression.
- diff_estimate: +0 / -0 lines of engine source. Marker files only.
- skip_plan_review: no
- test_strategy: role #5 re-derives each field-to-source binding independently, with a positive
  control for every load-bearing zero (tick-526 alternation rule, v219 dotfile-scope rule).
- risks:
  - **The dotfile-scope hazard is live in this cycle's own method.** `search_files` under
    `~/.hermes` returns 0 for strings that `read_file` proves are present. Any zero taken from that
    tree is worthless. Mitigation: `read_file` for everything under `~/.hermes`; `search_files` only
    against the hermes-agent source tree, where it is demonstrably sound.
  - The installed/running agent may not be the tree at `Documents/Gitrepo-My/hermes-agent`. If the
    running code differs, every line number here is decorative. Must be checked, not assumed.
  - v221 asserted a `command_allowlist` entry at `config.yaml:479`. If that is wrong the remedy set
    changes again. Re-read it.
