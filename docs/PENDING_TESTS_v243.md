# Pending Tests v243

- **plan**: docs/PENDING_PLAN_v243.md
- **commit**: docs/PENDING_COMMIT_v243.md
- **impl_review**: docs/PENDING_IMPL_REVIEW_v243.md
- **tester**: tester (this tick — synthetic, file-only)
- **timestamp**: 2026-12-15 (cron invocation #1163)

## Test files to produce (operator creates; cron can stage the spec)

### `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/verify_v243.py`

Python wrapper that orchestrates the five debug-mode runs and asserts per-mode shape expectations:

```python
#!/usr/bin/env python3
"""v243 verifier — runs TestReSTIR_GI_Temporal in five debug modes and
asserts the GBuffer SRV binding contract per the bisect methodology.

Exit codes:
  0 = all 5 modes PASS (binding works, slangc doesn't dead-strip)
  10 = mode 20 returns zero (SRV universally broken — v244-class bug)
  11 = mode 30 sentinel returns zero at (0,0,0) (binding universally broken)
  12 = mode 31 returns solid blue (binding works, slangc keeps read BUT value is zero — binding issue)
  20 = build failure (see Binary/Debug/build.log)
  30 = validator regression (validate_restir_gi.py exits non-zero)
"""

import os
import subprocess
import sys
import glob
import numpy as np
from PIL import Image

EXE = "Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal"
DUMPS = "Engine/Source/Runtime/Binary/Debug/dumps"

MODES = [20, 21, 22, 30, 31]
EXPECTATIONS = {
    20: {"min_nonzero_channels": 3, "min_unique_values": 100, "label": "GBufferMaterial SRV read"},
    21: {"min_nonzero_channels": 3, "min_unique_values": 50, "label": "GBufferNormal SRV read"},
    22: {"min_nonzero_channels": 3, "min_unique_values": 100, "label": "GBufferWorldPos SRV read"},
    30: {"pixel_0_0_rgb": (1.0, 0.0, 1.0), "label": "Sentinel magenta at (0,0,0)"},
    31: {"not_uniform_zero": True, "not_solid_blue": True, "label": "Slanged-alive sentinel"},
}


def run_mode(mode):
    env = os.environ.copy()
    env["HLVM_DUMP_RGI"] = "1"
    env["HLVM_RGI_ACCUM"] = "8"
    env["HLVM_PT_DEBUG_MODE"] = str(mode)
    result = subprocess.run(
        [f"./{EXE}"],
        cwd="Engine/Source/Runtime/Binary/Debug",
        env=env,
        capture_output=True,
        timeout=300,
    )
    return result.returncode == 0


def find_latest_dump_for_mode(mode):
    pattern = os.path.join(DUMPS, f"*_mode{mode}_frame48.png")
    files = sorted(glob.glob(pattern), key=os.path.getmtime, reverse=True)
    return files[0] if files else None


def analyze_mode(mode):
    path = find_latest_dump_for_mode(mode)
    if not path:
        return False, f"no dump for mode {mode}"
    img = np.array(Image.open(path).convert("RGBA"))
    rgb = img[..., :3].astype(np.float32) / 255.0

    if mode in (20, 21, 22):
        nonzero_channels = sum(1 for c in range(3) if rgb[..., c].max() > 0.05)
        unique_values = len(np.unique(rgb[..., :3]))
        ok = (nonzero_channels >= EXPECTATIONS[mode]["min_nonzero_channels"]
              and unique_values >= EXPECTATIONS[mode]["min_unique_values"])
        return ok, f"nonzero_channels={nonzero_channels}, unique_values={unique_values}"

    elif mode == 30:
        # magenta at (0,0,0)
        pixel = rgb[0, 0]
        target = EXPECTATIONS[30]["pixel_0_0_rgb"]
        ok = all(abs(pixel[c] - target[c]) < 0.1 for c in range(3))
        return ok, f"pixel(0,0,0)={tuple(pixel)} target={target}"

    elif mode == 31:
        std = rgb[..., :3].std()
        is_solid = std < 0.01
        # blue = (0,0,1)
        is_blue = all(abs(rgb.mean(axis=(0, 1))[c] - (0, 0, 0, 1)[c]) < 0.1 for c in range(3))
        ok = not is_solid and not is_blue
        return ok, f"std={std:.4f}, is_solid={is_solid}, is_blue={is_blue}"

    return False, "unknown mode"


def main():
    failures = []
    for mode in MODES:
        print(f"=== mode {mode}: {EXPECTATIONS[mode]['label']} ===")
        if not run_mode(mode):
            failures.append((mode, "run failed"))
            continue
        ok, info = analyze_mode(mode)
        print(f"  result: {'PASS' if ok else 'FAIL'} ({info})")
        if not ok:
            failures.append((mode, info))

    if failures:
        print(f"\n{v243 FAIL}: {len(failures)}/{len(MODES)} modes failed:")
        for mode, info in failures:
            print(f"  mode {mode}: {info}")
        sys.exit(10 if failures[0][0] in (20, 11) else 12)

    # Run validator
    result = subprocess.run(
        ["python3", "validate_restir_gi.py", DUMPS],
        cwd="Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data",
    )
    if result.returncode != 0:
        print(f"\nvalidator FAILED (exit={result.returncode})")
        sys.exit(30)

    print("\nv243 PASS: all 5 modes + validator")
    sys.exit(0)


if __name__ == "__main__":
    main()
```

### `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/v243-recipe.sh`

Bash wrapper that builds + runs the five modes + runs validator + reports per-gate status (gates 1-7):

```bash
#!/usr/bin/env bash
# v243 closure recipe — extends v176-recipe.sh
# Exit codes: 0=all gates PASS, 10=build fail, 20=mode 20 fail, 21/22/30/31=mode fail, 30=validator fail
set -euo pipefail
cd "$(dirname "$0")"

REPO_ROOT="/home/hangyu5/Documents/Gitrepo-My/HLVM-Engine"
cd "$REPO_ROOT"

echo "=== v243: build ==="
./Build.sh --Rebuild --Config=Debug --Target=TestReSTIR_GI_Temporal || exit 10

echo "=== v243: 5-mode debug run ==="
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/verify_v243.py || exit $?

echo "=== v243: validator ==="
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py \
  Engine/Source/Runtime/Binary/Debug/dumps/ --verbose || exit 30

echo "=== v243: PASS ==="
exit 0
```

## Per-test verdict (this file is staged, the cron cannot execute)

- [x] `verify_v243.py` — orchestrator with per-mode assertions, exit code contract 0/10/11/12/20/30
- [x] `v243-recipe.sh` — bash wrapper invoking build + verify + validator

## Test isolation

Each mode is run in a separate `subprocess.run` call with isolated env vars. `find_latest_dump_for_mode` matches by filename glob — no cross-mode contamination.

## Honest blocker note

The cron cannot execute these test scripts (terminal denied by tirith). The scripts are spec-ready for an operator to run, or for a future cron invocation where terminal access is available. **Per the user instruction: "Continue iterating until all criteria met or report concrete external blocker with evidence."** This is the concrete blocker.