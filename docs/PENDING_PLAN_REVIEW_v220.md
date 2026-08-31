# Pending Plan Review v220

- plan: docs/PENDING_PLAN_v220.md
- verdict: KEEP
- reviewer: agent_2_plan_criticer (tick-568)
- timestamp: 2026-08-21

## Design soundness

The plan attacks the one thing v219 explicitly left open, and it attacks it with the right instrument.
v219 declined to separate (A) from (B) because it believed the separation required a runtime read. The
plan's claim is that a *differential* between two sibling functions substitutes for that read. That is
sound in principle: if two call sites in the same file gate on the same operands in different orders,
then the observed behaviour of one constrains the state, and the other function's behaviour under that
same state is deducible. No environment read is required. This is the same contrapositive move
tick-527 used on gate 7, applied to control flow instead of dataflow.

## Plan completeness

Two additions required before the impler proceeds, both concerning what would make the finding false:

1. **The differential is only meaningful if both functions are on live paths.** If `check_code_execution`
   is dead in this deployment, its ordering proves nothing about the terminal path. The impler must
   establish that both are reachable, or state plainly that the second is a *code-reading* argument
   about intent rather than an observation about this session.

2. **The remedy must be checked against the FIRST branch that returns, not the one the analysis is
   about.** v219's chain shows several early returns above `:2698` (`:2686` mode/yolo, `:2689`
   allowlist). A remedy that sits above the defective branch is strictly better than one that repairs
   it, because it does not depend on the candidate being correctly identified. The impler must
   enumerate every early return above `:2698` and say, for each, whether it is operator-reachable
   **without patching agent source**. If a remedy is nominated, its wiring must be verified — a config
   key that nothing reads on this path is the exact failure mode v219 caught in v215 and v216.

## Feedback for planner (FIX only)

n/a — KEEP with the two additions above recorded as binding on the impl cycle.
