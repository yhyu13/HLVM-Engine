# Pending Plan Review v228

- plan: docs/PENDING_PLAN_v228.md
- verdict: KEEP
- reviewer: agent_2_plan_criticer (tick-581)
- timestamp: 2026-08-21

## Design soundness

The plan replaces a conjecture with a decision procedure, and the procedure is sound in a way the lineage's previous attempts were not. Ticks 563/564/569/580 all reasoned *forward* from config values ("`cron_mode` is `allow`, therefore cron commands should be allowed") and then had to explain why the prediction failed. Reasoning **backward from the emitted envelope** to the set of branches capable of emitting it is strictly stronger: it needs no hypothesis about the environment, only the control flow, which is fully readable with file tools.

The key structural question the plan identifies — *does the non-interactive/cron branch end in an unconditional `return`?* — is decidable by reading ~65 lines, and it is dispositive. If yes, then any remedy living inside that branch is unreachable from our path, and 18 ticks of operator advice were addressed to dead code.

## Plan completeness

One gap, and the plan already names it as a risk rather than a finding, which is the correct epistemic status: the dot-directory false zero. I am raising it to a **required row** for the tester, because if it is real it retroactively taints every lineage claim of the form "`search_files path=~/.hermes ...` → 0 hits." Tick-580's own operator action cites `~/.hermes/config.yaml:478-479`, so the lineage has at least one claim about that tree that a false zero would contradict in the *opposite* direction (a real line that a search cannot see). Both polarities must be tested with a positive control.

Second required addition: the plan proposes re-probing `terminal` in the bare form. Record the **envelope fields**, not just "it failed" — the field set is what discriminates the two candidate branches (`:2705-2714` cron-deny vs `:2999-3012` pending fallback), and the lineage has conflated them before.

## Feedback for planner (FIX only)

n/a — KEEP.
