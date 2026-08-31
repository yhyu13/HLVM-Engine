# Pending Plan v29

- task: v29 — structural standby tick documenting terminal-block status; no new source patches (per v28 audit conclusion that v28 was the last meaningful file-only diagnostic-surface expansion)
- source: no bundle — direct disk audit
- approach: (1) Append a v29 standby tick to `docs/PIPELINE_HEALTH_2026-07-27.md` that records (a) the exhaustive list of all v3-v28 patches still on disk, (b) the structural terminal-block status, (c) the canonical parent-triage recipe including the alpha-channel inspection path v28 provides, (d) the next-cycle decision matrix keyed to parent's v28 evidence shape. (2) Update `docs/PENDING_PICK.md` to mark v29 as [x] (this standby cycle is complete). (3) Re-stage v30 in PENDING_PICK.md as the parent-rebuild+`run_rgi_diagnostic.sh`+`rgi_evidence.txt`-paste-back gate that v21/v13a/v29 already documented. 0 source-code lines modified. 6 marker files written (PLAN, PLAN_REVIEW, COMMIT, IMPL_REVIEW, TESTS, TEST_AUDIT) following discipline.
- diff_estimate: +0 / -0 source-code lines (documentation-only)
- skip_plan_review: no — documentation-only changes still follow marker discipline
- test_strategy: cron file-only (Part A static audit). Part B runtime gate.
- risks: none — pure documentation. The risk of REPEATED audit-tick cycles is mitigated by `PICK`'s explicit v30 staging pointing to the same parent-evidence gate, so the next parent-action is unambiguous.

## Why this cycle is documentation-only
Per the prior v28 audit's honest verdict (PENDING_TEST_AUDIT_v28.md and PIPELINE_HEALTH_2026-07-27.md line 2366):
> "**v28 is the LAST meaningful file-only diagnostic-surface expansion.** After v28, every additional file-only patch would either be (a) a corrective fix requiring terminal to verify, or (b) a duplicate audit of unchanged source."

Confirmed this tick: terminal still blocked by tirith (`pending_approval: tirith:unknown`); v22 binding-layout-split is load-bearing; v28 alpha sentinel gives parent a binary dispatch-body signal. The only forward action is parent rebuilding and running, which is irreducibly terminal-driven.

## What this plan does NOT do
- Does NOT introduce a corrective fix (would require terminal to verify).
- Does NOT introduce another diagnostic sentinel (would be a duplicate of v28's alpha-floor probe).
- Does NOT commit, push, archive, pause, create Kanban cards, or modify governance.
- Does NOT fabricate parent evidence.

## Plan Deviations (impler fills this in if it deviated)
None. +0/-0 source-code lines.
