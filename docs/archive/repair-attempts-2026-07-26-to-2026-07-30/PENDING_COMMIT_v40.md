# Pending Commit v40 — extend dump_pixelstats.py to read alpha channel

## Plan
- docs/PENDING_PLAN_v40.md

## Files
- Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dump_pixelstats.py (modified)

## Source
- No source bundle; the patch extends an existing helper from the same project tree.

## Target
- working tree (no commit, no push — per cron's "do not commit/push/rewrite history" rule)

## Task
Extend `dump_pixelstats.py` (v24 fast-first-look helper) so it inspects the alpha channel of any RGBA-mode PNG and classifies the alpha stats against the v28 sentinel ladder. Pre-v40 the helper used `Image.open(path).convert("RGB")` which stripped alpha; the v28 alpha-channel sentinel was therefore invisible. v40 closes this gap, parallel to v37's `validate_restir_gi.py::check_alpha_sentinel()` extension.

## Verify
- python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dump_pixelstats.py
- Expected: same RGB output as before, plus a per-frame `A:` line and a `[v40-alpha]` diagnostic line

## skip_impl_review: no
- Modifying existing Python helper with non-trivial alpha-handling logic. Full audit trail invoked.

## produces_test_files: no
- Extending diagnostic helper, not a test file. HARD INVARIANT #2 does NOT fire.

## Diff estimate
- +59 lines / -0 lines (12 lines docstring + 47 lines new functions and alpha block)

## Notes
- Pure Python helper extension; no source-code (C++/HLSL) modifications.
- The new functions (`compute_alpha_stats`, `classify_alpha_sentinel`) mirror the structure of v37's `check_alpha_sentinel` so the two helpers classify evidence identically.
- The RGBA-mode read uses `Image.open(path).convert("RGBA")`; for RGB-only PNGs (no alpha), the alpha block is skipped without affecting the RGB output.
- The patch is best-effort: alpha-inspection failures are swallowed so RGB reporting is preserved.

## Plan Deviations
None — patch matches plan exactly.