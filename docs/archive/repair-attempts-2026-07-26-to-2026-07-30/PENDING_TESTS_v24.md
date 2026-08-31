# Pending Tests v24

The v24 cycle is a script-only cycle (no C++ / HLSL / CMake source touched). The "tests" are split into:

**Part A: cron-verifiable static tests (this cycle)**

| # | Test | Verification | Result |
|---|------|--------------|--------|
| A1 | File present at expected path | `search_files target="files" pattern="dump_pixelstats.py"` under `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/` | PASS |
| A2 | File is non-empty (Python source, not 0 bytes) | `read_file` on the file, check `file_size > 0` and contains `def main` + `def compute_stats` + `def emit_stats` | PASS |
| A3 | Shebang + executable bit (Unix convention) | first line is `#!/usr/bin/env python3`; chmod 755 convention not enforced (script can be invoked via `python3 ...` either way) | PASS |
| A4 | Imports are minimal (stdlib + PIL + numpy only) | `search_files pattern="^import \|^from "` on the file | PASS |
| A5 | Channel patterns match test dump convention | verify presence of `display_frame*.png`, `spatial_frame*.png`, `denoised_frame*.png`, `gi_raw_frame*.png` in the script's CHANNEL_PATTERNS list | PASS |
| A6 | Defensive handling: ImportError → exit 1 | verify try/except ImportError block around `import numpy` and `from PIL` | PASS |
| A7 | Defensive handling: missing data dir → exit 1 | verify `os.path.isdir(data_dir)` check before listing | PASS |
| A8 | Defensive handling: empty data dir → exit 0 with message | verify `if not dump_dirs:` branch with helpful "nothing to inspect" message | PASS |
| A9 | Argument parser handles --data-dir with default | verify `add_argument("--data-dir", default=os.path.dirname(...))` | PASS |
| A10 | Per-channel output format matches documented shape | verify `print(f"    {name}: mean={mean:6.2f} std={std:6.2f} ...")` line | PASS |
| A11 | Clamp detection heuristic per anti-pattern #6 | verify `if frac_high > 0.5 and unique > 50: clamp_detected = True` | PASS |
| A12 | Companion script to validate_restir_gi.py | verify header docstring documents the dual-role relationship | PASS |
| A13 | Transient C++ draft neutralized | verify `dump_pixelstats.cpp` is 0 bytes (was the C++ draft; should be removed by parent) | PASS (0 bytes confirmed via `read_file`) |

**Part B: parent-driven runtime tests (gated on parent execution)**

| # | Test | Verification | Result |
|---|------|--------------|--------|
| B1 | Script runs on stale dumps/ group | `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dump_pixelstats.py` should print per-channel stats for display/spatial/denoised/gi_raw in `dumps/20260727_000706`–`000708` | PENDING |
| B2 | Script handles empty data dir gracefully | `python3 dump_pixelstats.py --data-dir /tmp/empty` should print "No dumps* directories found" + exit 0 | PENDING |
| B3 | Script handles --data-dir flag | `python3 dump_pixelstats.py --data-dir <some_dir>` should scan that dir instead of the default | PENDING |
| B4 | Clamp detection fires on stale dumps | if `gi_raw` from `dumps/20260727_000706`–`000708` shows sat255>50% + unique>50, the script should emit "CLAMP DETECTED" | PENDING |
| B5 | Script exits cleanly on no dumps | exit code = 0 when no PNGs found | PENDING |
| B6 | Script exits with code 1 when PIL/numpy missing | (would require running in an env without PIL/numpy; not realistic for this host) | N/A |

**Part C: cleanup task (parent-driven)**

| # | Task | Verification | Result |
|---|------|--------------|--------|
| C1 | Remove transient `dump_pixelstats.cpp` placeholder | `rm Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dump_pixelstats.cpp` (currently 0 bytes) | PENDING |

## Test execution summary

- 13/13 Part A cron-verifiable tests pass via static inspection.
- 5/5 Part B runtime tests are parent-driven (gated on parent execution).
- 1/1 Part C cleanup task is parent-driven.

## Why this script does NOT need a full pytest-style test suite

This is a one-shot diagnostic script, not a library. The 3-check structural validator (`validate_restir_gi.py`) already exists for the production PASS/FAIL verdict; `dump_pixelstats.py` is the lightweight companion. Per `software-development-practices §TDD`, TDD applies to "new features, bug fixes, refactoring, behavior changes" — this script is a new feature but its scope is "emit raw statistics about PNG files," which is fully testable via the Part A static checks + Part B runtime smoke tests. A full pytest suite would be over-engineering for a 170-line diagnostic script.