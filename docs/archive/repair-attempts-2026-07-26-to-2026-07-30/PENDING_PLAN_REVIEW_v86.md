# Pending Plan Review v86
- plan: docs/PENDING_PLAN_v86.md
- verdict: FIX
- reviewer: plan-criticer (v86)
- timestamp: 2026-07-28T23:NN

## Design soundness
The plan's *shape* (one fresh Part A probe + blocker acknowledgment) is the right call for the v85→v86 transition — NOT a v25-v81-pattern standby loop, NOT a fabricated goal-done marker, NOT silent exit. The cycle-meaning ("verify a NEW site + escalate the terminal block") is honest about the runspace constraint.

## Plan completeness — FIX items (must address before impler writes PENDING_COMMIT)

1. **Part A probe target is too vague.** The plan offers two candidate sites ("gi_raw SRV-read at FGIPass.cpp around line 1712" OR "the FOURTH command-list warning site") but the impler must pick ONE. Standing offer of "either / or" is the kind of fuzzy contract that lets the impler improvise and corrupts the audit. Fix to: pick the gi_raw-read site (preferred — it's the symptom-direct one; every other patch is upstream of it) OR the 4th command-list site. Decide at plan-criticer time and lock it in the plan.

2. **Missing acceptance contract.** v86 is the 69th cumulative tick. The "next tick" advice must include a hard off-ramp: if v87's parent action is also "no terminal + no evidence," what then? The plan should specify v87's posture (v87 should be the terminal cycle — the last in this cron's life without parent evidence — and write `PIPELINE_RUNSPACE_BLOCKED_2026-07-28.md` + a STRONG recommendation that the cron should not wake again until the parent has run the 4-command recipe OR reconfigured the cron to grant terminal).

3. **`PIPELINE_RUNSPACE_BLOCKED_2026-07-28.md` is the right escalation name but its body should be specified in the plan.** The plan just says "record this in `...md`" — what goes in it? Spec the minimum contents: (a) explicit list of the 6 acceptance criteria; (b) for each, why this runspace can't satisfy it; (c) the 4-command recipe unchanged; (d) explicit "the next parent-facing session should consider reconfiguring the cron to grant terminal access, OR running the recipe directly."

## Why not KEEP
The plan's shape is right, but leaving the FIX items loose means v86 risks producing two markers without any actual new probe work (impler defaults to "either" and picks neither cleanly). Pre-impl lock-in.

## Feedback for planner (FIX)
- Pick ONE Part A probe site (recommend gi_raw-read at FGIPass.cpp — symptom-direct).
- Spec v87's hard off-ramp ("if v87 finds no parent evidence, v87 writes `PIPELINE_RUNSPACE_BLOCKED_2026-07-28.md` and the cron self-terminates the standby pattern").
- Spec the body template for `PIPELINE_RUNSPACE_BLOCKED_2026-07-28.md`.
