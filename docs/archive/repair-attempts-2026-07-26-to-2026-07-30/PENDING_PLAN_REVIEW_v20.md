# Pending Plan Review v20

- plan: docs/PENDING_PLAN_v20.md
- verdict: KEEP
- reviewer: cron (single-head; same model as planner; freshness caveat applies per six-role-pipeline anti-pattern #7)
- timestamp: 2026-07-27

## Design soundness

The v20 plan is a single-file patch (one new bash script) that codifies the v20 9-branch decision matrix's evidence-capture protocol into a runnable artifact. It does not modify any source code and does not propose any renderer fixes. Its sole purpose is to give the parent (or the next interactive session) a one-shot diagnostic command that captures every probe's evidence shape into a single `rgi_evidence.txt` summary file. The cron, being file-only, cannot execute the script — but the script itself is a real artifact that advances the work by reducing the parent's manual protocol to one command.

The plan is well-grounded in actual evidence:
- v19 produced a complete 14-probe diagnostic surface on disk (verified at lines 575-684 of both GIPathTracing.hlsl copies).
- The v19 heartbeat correctly identified no more diagnostic-surface patches are possible file-only.
- The plan's only action is consolidating the 10-mode evidence-capture protocol — already documented across multiple parent-action sections in PENDING_PICK.md — into a single bash script.

## Plan completeness

The plan correctly identifies:
- 5 risks (bash portability, wall-clock, CWD independence, dump pollution, terminal block)
- The build path matches the project convention (`./Build.sh --Config=Debug --Target=...`)
- The validator invocation matches `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py`
- The dump path matches the `dumps/` subdir convention the validator expects

One minor gap noted but not blocking: the plan's "evidence summary" header omits the per-mode PNG visual-analysis slots. For example, after the validator runs, the parent would need to vision-analyze each mode's `gi_raw_frame*.png` and append the per-mode visual verdict (e.g., "mode 6: per-pixel gradient — PASS", "mode 7: scene-shape × 1.5 — PASS"). This is human-driven, not script-driven, and the script's role is to make those PNGs easy to find (one dumps/ subdir per mode) rather than to do the vision analysis itself.

The gap is non-blocking because the parent's existing vision-analysis protocol (referenced in v19's parent-action section) covers this. The script's evidence file is a complement, not a replacement.

## Feedback for planner (FIX only)

None. Plan is sound; single-head freshness caveat applies per the skill's anti-pattern #7.