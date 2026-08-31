# Pending Tests v21

## Test surface for v21

v21 is a plan-only cycle: it produced the v21 plan + plan-review + commit + impl-review markers. No source code was modified. The actual v21a code change (FGIPass binding-layout split for nvrhi-deferred-barrier-ordering) is staged in `docs/PENDING_PLAN_v21.md` but NOT applied because it is conditional on parent-driven v20 evidence.

This PENDING_TESTS_v21.md documents BOTH:
1. The tests for the v21 plan-only cycle (verifying the markers are coherent and the decision tree is correct)
2. The tests for v21a's binding-layout-split fix (which will only be exercised if v20 evidence confirms the nvrhi-deferred-barrier-ordering hypothesis)

The cron is file-only; all test execution is parent-driven.

## Part A: v21 plan-only cycle tests (immediately exercisable)

### Test A1: marker completeness

**Goal**: confirm all 5 v21 markers are present and well-formed.

```bash
ls -la docs/PENDING_PLAN_v21.md docs/PENDING_PLAN_REVIEW_v21.md docs/PENDING_COMMIT_v21.md docs/PENDING_IMPL_REVIEW_v21.md docs/PENDING_TEST_AUDIT_v21.md
```

Expected: all 5 files exist (the 6th, PENDING_TESTS_v21.md, is the file you're reading now). The impl-review marker should have verdict=KEEP. The plan-review marker should have verdict=KEEP.

### Test A2: decision tree coverage

**Goal**: confirm the v21 plan's decision tree covers all 9 branches from PENDING_PICK.md lines 141-150.

```bash
grep -c "v21" docs/PENDING_PLAN_v21.md
grep -E "v21[a-i]" docs/PENDING_PLAN_v21.md | head -20
```

Expected: the plan mentions v21a..v21i or references the v20 9-branch decision matrix. v21a is the staged branch (hypothesis #1); v21b..v21i are listed in the decision tree.

### Test A3: hypothesis #1 evidence chain

**Goal**: confirm the v21 plan's "Why this cycle is correct" section cites the v22/v23 heartbeats' hypothesis #1.

```bash
grep -E "v22 heartbeat|v23 heartbeat|hypothesis #1|nvrhi-deferred-barrier" docs/PENDING_PLAN_v21.md
```

Expected: the plan references the v22/v23 heartbeats' hypothesis #1 as the evidence basis for v21a's binding-layout split.

### Test A4: source-code gating

**Goal**: confirm the v21 cycle did NOT modify any source code.

```bash
git status --porcelain Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp Engine/Source/Runtime/Public/Renderer/GI/FGIPass.h Engine/Source/Runtime/Private/Renderer/RayTracing/FRayTracingPipeline.cpp Engine/Source/Runtime/Public/Renderer/RayTracing/FRayTracingPipeline.h
```

Expected: empty output. The only changes in the working tree are the docs/ marker files.

## Part B: v21a binding-layout-split tests (conditionally exercisable)

These tests only apply if/when the parent runs v20's diagnostic, confirms the nvrhi-deferred-barrier-ordering hypothesis, and the cron applies the v21a code change (FGIPass.cpp binding layout split).

### Test B1: build cleanliness

**Goal**: confirm the Debug build succeeds after v21a is applied.

Run:
```bash
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal 2>&1 | tee /tmp/v21a_build.log
```

Expected: 0 errors. Warnings count is informational (not a fail). v21a's risk E (cascade-aware -Werror) is mitigated by grepping for `(uintptr_t)` and `reinterpret_cast` patterns before committing.

### Test B2: DeviceManager.cpp:52 warning suppression

**Goal**: confirm the v21a fix suppresses the "A command list should be executed before it is reopened" warning that was firing 7x per stale run.

Run:
```bash
cd Engine/Source/Runtime/Binary/Debug
HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal 2>stderr.log
grep -c "command list should be executed" stderr.log
```

Expected: 0 occurrences (down from 7 in stale log). If the warning count drops to 0, the binding-layout split resolved the nvrhi-deferred-barrier-ordering bug.

### Test B3: gi_raw non-zero

**Goal**: confirm the GI dispatch now writes non-zero values to OutputTexture.

Inspect the latest `gi_raw_frame*.png` from the dump directory. Use vision analysis or pixel statistics:

```bash
python3 -c "
from PIL import Image
import numpy as np
img = np.array(Image.open('Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps/gi_raw_frame8.png'))
print('Shape:', img.shape)
print('Mean per channel:', img.mean(axis=(0,1)))
print('Max per channel:', img.max(axis=(0,1)))
print('Unique values:', len(np.unique(img.reshape(-1, img.shape[-1] if img.ndim == 3 else 1), axis=0)))
"
```

Expected: mean per channel > 0 (not all-zero), max per channel > 50 (sentinel-raw-pixel value of ~0 maps to ~127 in 8-bit PNG; gi_raw with actual scene geometry should max much higher). Unique value count should be > 100 (real scene geometry has many unique pixel values; a uniform color or sentinel has <10).

### Test B4: validator 3/3

**Goal**: confirm the project's own validator passes the new dump group.

Run:
```bash
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py 2>&1 | tee /tmp/v21a_validator.log
grep "checks PASSED" /tmp/v21a_validator.log
```

Expected: `3/3 checks PASSED`. This is the project's own definition of "working."

### Test B5: display frame visible Sponza

**Goal**: confirm `display_frame8.png` shows recognizable non-uniform Sponza geometry.

Open the PNG with vision analysis (or eyeball if not available) and verify:
- Non-uniform color (not all-black, not all-magenta, not all-gray)
- Recognizable architectural structure (walls, arches, columns)
- Sane exposure (not blown out to white, not crushed to black)

Expected: Sponza geometry visible with reasonable lighting.

## Test verdict target

- **ALL_KEEP** for Part A: Tests A1-A4 all pass, marker pair is complete, decision tree covers all 9 branches, source code untouched.
- **SOME_RELAX** for Part B: Tests B1-B5 are parent-driven; the cron cannot execute them. If v20 evidence confirms v21a branch and the cron applies the code change, the parent must run B1-B5 to verify the fix.

**Expected verdict for v21**: SOME_RELAX (Part A is verifiable in this tick; Part B is gated on v20 evidence + parent rebuild).

## Verification recipe (parent-driven, after v21a is applied)

```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/run_rgi_diagnostic.sh
```

This single command:
1. Builds TestReSTIR_GI_Temporal (Test B1)
2. Runs default mode + modes 6/7/8/9/10/11/12/15/99 (Test B2: warning count in rgi_default.stderr; Test B3: gi_raw in dumps/; Test B5: display_frame in dumps/)
3. Runs the validator (Test B4: 3/3 in rgi_validator.log)
4. Emits rgi_evidence.txt with consolidated results

The parent pastes rgi_evidence.txt back to the cron. If all 4 test categories pass, write `docs/PIPELINE_GOAL_DONE_2026-07-27.md` and mark v0 task `[x]` in PENDING_PICK.md.
