# Pending Plan v24

- task: write `dump_pixelstats.py` companion to validate_restir_gi.py for fast first-look evidence on stale dump groups (no build/run required)
- source: no bundle — direct edit
- approach: Python script that walks `dumps*/*frame8.png` (or any frame8 PNGs under the data dir) and emits per-channel structural statistics (mean R/G/B, std, unique value count, fraction-clamped-to-255, fraction-clamped-to-0, sentinel-pixel fraction). This complements `validate_restir_gi.py` (which only inspects the `dumps/` directory and uses 3 calibrated thresholds) by giving parent a multi-channel fingerprint on whatever dumps already exist on disk. When vision tool is unavailable and parent cannot rebuild, this is the next-best structural signal — it tells parent which dump channel is the bottleneck (e.g., `gi_raw` all-zero vs. `display` all-saturated) without requiring a fresh build. 1 new file (~80 lines, no external deps beyond PIL + numpy already used by the validator). Fully reversible (delete the file).
- diff_estimate: +80 / -0 lines (new file)
- skip_plan_review: no — script is part of the canonical evidence-collection protocol; should be reviewed for structural correctness
- test_strategy: cron verifies python syntax via static inspection + grep for required channel names (display/spatial/denoised/gi_raw); parent runs on existing `dumps/20260727_000706`–`000708` to produce a structural fingerprint of the stale group
- risks:
  1. Script might fail to import PIL/numpy if env is missing — mitigation: write the script defensively (catch ImportError and emit a clear error)
  2. Script might not find any PNGs (data dir empty) — mitigation: emit "no PNGs found" and exit 0 (informational, not error)
  3. Sentinel value used in TestReSTIR_GI_Temporal.cpp may differ from the `(99,99,99)` assumed in anti-pattern #6 — mitigation: skip the sentinel-fraction check if no dumps exist; document the sentinel convention in the script header
  4. Script may produce too much output — mitigation: per-channel one-line summary, no per-pixel output

## Why this is the right next cycle

The cron's "continue cycles ... do not silently stop" instruction authorizes mechanically-actionable file-only fixes. v23 fixed the dump-rotation bug in `run_rgi_diagnostic.sh`. The cron cannot rebuild (tirith blocked all terminal probes for 14+ ticks) and cannot vision-analyze (vision tool unavailable). The next-best structural signal is **per-channel pixel statistics on whatever dumps already exist**. The stale dump group `20260727_000706`–`000708` exists on disk; if `dump_pixelstats.py` runs against it, parent gets:

- Which channels have non-zero variance (means the GBuffer → accumulate → display chain has SOME signal somewhere)
- Which channels are saturated (means `FImageDump::DumpToPNG` clamp is hiding the data; needs per-channel normalization)
- Which channels are uniform-zero (means the GI pass wrote nothing; confirms the binding-layout-split hypothesis)

Without this script, parent's next move is "rebuild + rerun the FIXED `run_rgi_diagnostic.sh`" — which costs 10+ minutes of compile + 50+ seconds of run time. With this script, parent's first move is "run `dump_pixelstats.py` on existing dumps" — which costs <1 second and gives actionable signal in 100% of cases.

## File-level changes

```
A Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dump_pixelstats.py  (~80 lines, no source-code changes)
```

## Script header (planned)

```python
#!/usr/bin/env python3
"""Per-channel structural pixel statistics for TestReSTIR_GI_Temporal dump groups.

Walks every dumps* directory in the data dir and emits per-frame per-channel:
  - mean (R, G, B)
  - std (across full frame)
  - unique value count
  - fraction-clamped-to-255 (saturated-high)
  - fraction-clamped-to-0 (saturated-low)

This is the canonical "fast first-look" companion to validate_restir_gi.py:
- Use dump_pixelstats.py when you cannot rebuild (terminal blocked, GPU driver
  issue, etc.) and want to know which channel is broken without running the
  test again.
- Use validate_restir_gi.py when you have fresh dumps and want a yes/no pass
  verdict on the calibrated 3-check structural thresholds.

Usage: python3 dump_pixelstats.py [--data-dir <path>]
"""
```

## What this script does NOT do

- Does NOT replace the validator (different role; the validator returns PASS/FAIL on calibrated thresholds, this script returns raw statistics)
- Does NOT modify any source files (script-only)
- Does NOT create Kanban cards (per cron instruction)
- Does NOT commit, push, or rewrite history (per cron instruction)