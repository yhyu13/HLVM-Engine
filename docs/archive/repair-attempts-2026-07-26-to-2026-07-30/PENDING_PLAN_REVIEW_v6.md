# Pending Plan Review v6

- plan: docs/PENDING_PLAN_v6.md
- verdict: KEEP
- reviewer: plan-criticer (single-head autonomous cron — see software-development-practices §"Full auto" anti-pattern #7 caveat)
- timestamp: 2026-07-27T05:30:00Z (estimated; cron tick wall clock)

## Design soundness

v6 is a contingency plan that explicitly does NOT execute any code change. It documents four sub-plans (v6a/b/c/d) based on parent's v5 verification outcome. The contingency structure is sound: each sub-plan targets a specific evidence shape (gi_raw pixel-stats, validator output, vision-check) that the parent's verification will produce. The "do not execute" framing prevents premature code changes.

The four-bucket decision matrix is grounded in:
- gpu-rendering-bisect-debug §3 sentinel pattern (output texture recreation)
- nvrhi auto-barrier ordering bug-075 pattern (VUID-00344)
- slangc RT payload dead-strip (2026-07-25 session's root cause)
- 4-check structural validator granularity

## Plan completeness

The plan correctly identifies the verification bottleneck (terminal access blocked in cron; parent must drive) and structures sub-plans to consume the parent's evidence. Missing:
- v6b sub-plan needs more specificity on which validator check maps to which pass. Suggest when v6b is triggered, the cron pre-reads the validator code to map checks 1/2/3 → display/spatial/denoised textures.
- v6c sub-plan needs more specificity on the "auto-barrier ordering bug-075" pattern. The 2026-07-25 SESSION_HANDOFF documents that bug-075 was fixed by splitting TemporalLayoutSRV + TemporalLayoutUAV. The display blit is a separate pass; bug-075 may not apply.

## Feedback for planner (FIX only)

Minor (informational only — no code change required for this KEEP):
1. v6b should pre-read `validate_restir_gi.py` to map checks → textures.
2. v6c should cross-reference the 2026-07-25 SESSION_HANDOFF to confirm bug-075's exact mechanism.
3. Add a v6e sub-plan: if v5 build fails (compile error after the HLVM-bypass removal), revert the v5 patch and pivot to a different fix.

These are improvements for when v6 is triggered, not blockers. The current KEEP stands.

## Honest caveats

- v6 is staged but dormant. The cron cannot decide which sub-plan to execute without terminal access.
- The cron's terminal-blocked status is unchanged from v3/v4/v5. Same tirith hook denial.
- Per software-development-practices §"6-role pipeline on a single-profile host", this KEEP is from the same head that wrote the plan. The parent's verification is the actual gate.