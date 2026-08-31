# Pending Plan Review v126

- plan: docs/PENDING_PLAN_v126.md
- verdict: KEEP
- reviewer: file-only plan-criticer (no terminal access, same runspace as planner per EC-039)
- timestamp: 2026-07-30 (tick 106, second pass)

## Design soundness
The v126 plan correctly addresses both gaps from the v125 FIX review:
1. **Step 0 (sblob mtime precondition)** is now the explicit first step. The 5-second check is committed before any source edits. If the sblob is stale, the entire bisect closes via a single rebuild.
2. **Step 3 (Experiment C) outcome C2** now has a committed fix path: "drop `, space1` from Output and DebugStatsTexture, rebuild". The committed fix has a known side effect (loses the nvrhi-deferred-barrier fix, requiring v22 split revert), but the side effect is documented.

The plan is now actionable by a parent runspace with terminal access. Each step is single-variable, each predicted outcome is falsifiable, each step has a concrete time cost in seconds.

## Plan completeness
The v126 plan is complete enough to proceed. Remaining minor gaps:

- **Step 4 (slangc-leak test) has an ambiguity**: "keep the `, space1` but ensure nvrhi's binding layout actually declares them in set 1 too" — this requires a fix that may not be obvious to the impler. A clearer instruction would be: "If L1 triggered (modes 20/21/22 non-zero after dropping `, space1`), keep the revert (no `, space1`) AND revert v22 (combine the two binding layouts back into one). The single-binding-layout configuration is the v1 state that originally produced the nvrhi-deferred-barrier warnings, but with v101's additional fixes (e.g., the CommandList isolation at line 1531-1537) those warnings may not fire anymore."

  This is a refinement, not a blocker. The impler can decide based on the post-step-3 reflection.

- **No explicit "abort and report blocker" path.** If the parent's terminal probe also fails (e.g., the rebuild itself fails with a different error), the impler should write `PENDING_IMPL_REVIEW_v127.md` with verdict DELETE and a one-line note. The dispatcher will then route to "next item from PICK" or "exit SILENT" if nothing else is pending.

## Acknowledged state-machine caveat
Per six-role-pipeline HARD INVARIANT #4, "Plan-criticer FIX always loops to planner." The v126 plan addresses the v125 FIX review. This v126 review is KEEP — the plan is ready for impler dispatch.

**Critical caveat:** per the dispatcher's instructions and the gpu-rendering-bisect-debug skill, the impler role MUST have terminal access to execute the build/run/inspect cycle. The cron runspace on this host is structurally terminal-blocked per EC-039 / `docs/OVERSEER_ESCALATION.md`. So the state machine routes to impler, but the impler CANNOT act.

The plan remains valid for parent-side execution. The impler role in this runspace will mark itself as "blocked at file-only runspace" and exit, which is the honest action per the destroy-vs-honest anti-pattern from `software-development-practices`.

## Feedback for planner
None — KEEP. (If planner iterates again without new evidence, see HARD INVARIANT #4 about avoiding indefinite plan-critique loops. The dispatcher's instruction was to continue until bisect yields a fix OR report concrete blocker with evidence. This v126 plan is concrete enough to be the final iteration of the planner role in this runspace.)

## Self-review checklist
- [x] Step 0 added per v125 review feedback.
- [x] Step 3 C2 fix path committed per v125 review feedback.
- [x] Each step single-variable.
- [x] Each step has falsifiable predicted outcome.
- [x] Plan does not require test files.
- [x] Plan does not commit, push, or modify governance files.
- [x] Plan acknowledges file-only runspace limitation at every step.