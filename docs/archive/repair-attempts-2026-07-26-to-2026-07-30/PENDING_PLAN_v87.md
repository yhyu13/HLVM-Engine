
# Pending Plan v87
- task: restir-gi-fix — ReSTIR GI Temporal renders black/empty display; gi_raw R/G/B all zero per stale 2026-07-27 00:07 log
- source: no bundle — direct edit (parent terminal access required for build/run/validate/vision; structurally blocked in this cron runspace)
- approach: One Part A probe, locked-site: locate the gi_raw post-process dump site via `search_files` and confirm the SRV-binding descriptor for the gi_raw resource is well-formed (no padding offset / no `BindingLayoutItem::Slot::T` mismatch / no flag mismatch). The probe is read-only via `search_files` + `read_file`. Cycle shape is verification-only: zero source-code lines modified; one Part A spot-check at the symptom-direct site; explicit `PIPELINE_RUNSPACE_BLOCKED_2026-07-28.md` body with the 6 criteria documented and the parent's 4-command recipe reaffirmed.
- diff_estimate: 0 source-code lines
- skip_plan_review: no — the per-cycle state machine MUST re-route through plan-criticer per HARD INVARIANT #4.
- test_strategy: role #5 (tester) — Part A spot-check on the chosen gi_raw-read site MUST pass (or the implication is a NEW finding the impler can route on). Part B 8/8 remains UNVERIFIED, must be stated as such.
- risks: (1) The gi_raw-read site may not exist in the form the plan assumes — search_files may find no candidate or a completely different binding layout than the probe expects. If search_files returns 0 sites matching the expected signature, that's a valid finding (the bug is not at this site, narrows the search space). (2) Re-cycling a v25-v86 probe site — avoided by the constraint "locate the gi_raw post-process dump site via search_files; only probe if it hasn't been probed before."
