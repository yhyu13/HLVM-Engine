# Pending Plan Review v222

- plan: docs/PENDING_PLAN_v222.md
- verdict: KEEP
- reviewer: agent_2_plan_criticer (tick-570)
- timestamp: 2026-08-21

## Design soundness

The plan is sound and it is the first in seven cycles that is not a variation on its predecessor.
v215-v221 form a chain in which each cycle refuted a detail of the one before while preserving the
shared frame: *the block is a config problem, therefore the output is an operator config action.*
Six refutations inside one unexamined frame is the signature of a frame that was never tested. The
plan tests it, by the only method that can: derive the observed envelope from source rather than
inferring the source from the config.

The choice of instrument is right. `pattern_key`, `description` and `allow_permanent` are **emitted
strings**, not interpretations — each must have exactly one construction site. That makes the
question decidable by reading, which matters on a runspace that cannot execute anything.

## Plan completeness

Three binding additions, all of which the plan must not treat as optional:

1. **Check that the tirith binary is absent before relying on any conclusion that assumes it is.**
   v215 established "a binary that does not exist" by searching `/usr/bin`, `/usr/local/bin`, `/opt`,
   `~/.local`, `~/.hermes` — and `~/.hermes` is precisely the tree where the plan itself notes
   `search_files` returns false zeros. **The most load-bearing negative in the entire lineage was
   taken with the one query shape known to fail on the one directory that matters.** Re-derive it
   with `target=files`, which is a different code path from content search.

2. **`description` is the sharpest of the four fields and the plan under-weights it.** `pattern_key`
   only identifies a rule; the human-readable `description` distinguishes *empty findings* from
   *populated findings*, which separates "the scanner ran and objected" from "the scanner did not
   run". Require the exact string be matched to its construction site character for character.

3. **State the outcome that would make this cycle produce no operator action at all.** If the block
   is downstream of every config-reachable branch, then the correct output is that the standing
   remedy does not work — a negative deliverable. Say so up front so it cannot be quietly avoided.

## Feedback for planner (FIX only)

n/a — KEEP with the three additions above binding.
