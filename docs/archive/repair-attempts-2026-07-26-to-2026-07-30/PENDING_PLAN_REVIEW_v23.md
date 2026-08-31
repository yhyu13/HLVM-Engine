# Pending Plan Review v23

- plan: docs/PENDING_PLAN_v23.md
- verdict: KEEP
- reviewer: cron (single-head; same model as planner; freshness caveat applies per six-role-pipeline anti-pattern #7)
- timestamp: 2026-07-27

## Design soundness

The v23 plan addresses the dump-rotation bug in `run_rgi_diagnostic.sh` identified by the v24 outer-watchdog heartbeat. The plan's root-cause analysis is rigorous:

1. The buggy code at lines 88-92 archives `dumps/` BEFORE each run, naming the archive with the CURRENT mode's name. This causes off-by-one mislabeling across the per-mode dumps.
2. The plan's re-trace corrects one detail in the watchdog's analysis: `dumps_default` after the loop IS the actual default-mode output (because iter 2's move overwrites the stale iter-1 archive). The validator input is therefore correct; the per-mode diagnosis is broken.
3. The fix design (archive AFTER run, name with the mode that PRODUCED the dumps) is the canonical fix for this kind of pipeline-rotation bug.
4. The plan enumerates 4 risks with mitigations: pre-existing stale dumps (handled by prerun archive), per-mode run failure (empty archive is correct), parent's existing mislabeled archives (re-run after `rm -rf dumps_*`), and the validator-input concern (which the plan correctly identifies as already-working-but-per-mode-broken).

The proposed fix is mechanical, surgical, and well-scoped. The diff is small (+7/-5 net), the touched file is a single shell script, and the patch is fully reversible.

## Plan completeness

The plan has two minor gaps:

1. **The plan does not enumerate how the `rgi_evidence.txt` composition at lines 143-168 will reflect the new archive names.** Looking at the script, lines 154-162 already iterate `MODE_NAMES` and check both `dumps/` (for default) and `dumps_${mode_name}/` (for other modes). After the fix, both lookups resolve to the correct mode's output, so the evidence composition is correct WITHOUT changes. The plan should explicitly note "no changes to lines 143-168".

2. **The plan does not address the v20 audit's "SOME_RELAX" verdict propagation.** The v20 TEST_AUDIT_v20.md verdict SOME_RELAX was issued for the broken script. After v23, the script is correct but parent-driven verification is still required. The plan should note that v23 does NOT change the v20 audit verdict (it remains SOME_RELAX until parent re-runs and verifies).

3. **The plan does not stage the v22 PICK item's status post-v23.** v22 is currently `[ ]` and gated on parent v20 evidence. After v23 fixes the script, v22 remains `[ ]` (still gated on parent re-running the FIXED script). The plan should note that v23 unblocks the path to v22 but does not advance v22 itself.

These gaps are non-blocking — the plan is sound and the fix is mechanically correct. The gaps can be addressed in the v23 impl cycle.

## Feedback for planner (FIX only)

None at the plan level. The minor gaps (evidence composition unchanged, v20 audit verdict unchanged, v22 status unchanged) are observable in the v23 impl commit's notes section. The plan is sound.

## Verdict rationale

KEEP because:

1. The plan correctly identifies the dump-rotation bug as a real defect in the v20 script (confirmed by static file inspection).
2. The plan's root-cause trace is more accurate than the watchdog's (correcting one detail about `dumps_default`).
3. The proposed fix is the canonical pattern for archive-after-run rotation.
4. The fix is file-only, surgical, and fully reversible.
5. The plan's risk enumeration is honest (4 risks with explicit mitigations).
6. The fix does NOT depend on terminal access — the cron can apply it file-only.
7. The plan does NOT modify any C++ / HLSL / CMake source.
8. The single-head freshness caveat is acknowledged.

The v23 cycle is the mechanically actionable file-only fix that the v24 outer-watchdog flagged. Applying it advances the pipeline trajectory without requiring parent intervention.