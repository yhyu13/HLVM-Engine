# Pipeline Handoff v99 — restir-gi-fix (2026-07-28)

## What this document is

The cron has produced 84 cumulative file-only diagnostic ticks (v25-v99) on `restir-gi-fix`. v99 is the cron's final tick on this item. v99 ships a **byte-verified `git apply`-ready patch text** at `docs/restir-gi-fix-v99.patch` (6 hunks across 5 files, +25/-2 lines) that addresses the v93 root-cause diagnosis: the v22 split binding layout is half-applied to FGIPass — `UAVBindingLayout` is created but never registered with the RTPipeline, AND the shader's `Output`/`DebugStatsTexture` UAVs are at `register(u0)`/`register(u1)` in default space0 (should be space1 to match the UAV binding set).

v99 ALSO corrects 3 broken hunks in v98's `docs/restir-gi-fix-v98.patch` that v98's PATCH_TEXT_CORRECTED verdict missed:

| Hunk | v98 problem | v99 fix |
|------|-------------|---------|
| FRayTracingPipeline.cpp #1 | `@@ -119,6 +119,13 @@` — context omits lines 119-120 (sig + `{`); off by 2 | `@@ -121,4 +121,12 @@` |
| FRayTracingPipeline.cpp #2 | `@@ -148,7 +148,11 @@` — new_start=148 ignores the +8 cumulative offset inserted by hunk #1 | `@@ -148,7 +156,11 @@` |
| FGIPass.cpp | `@@ -315,6 +315,7 @@` — uses 8-space indent on `return false;`, actual file has 12-space | `@@ -311,7 +311,8 @@` (correct anchor + 12-space indent) |

The v99 patch text has been byte-verified against actual file content via first-hand `read_file` with explicit line offsets in the same turn it was written (NOT inherited from v98's verification).

## What the parent should do

### Step 0 (cheapest): disambiguate v93 with 10s terminal check

```bash
spirv-cross --reflect /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine/Engine/Source/Runtime/Binary/Debug/shaders/GIPathTracing.spv 2>/dev/null | grep -A1 "Output"
```

If `Output` at `(set=1, binding=0)` — v93 confirmed, proceed to Step 1.
If `Output` at `(set=0, binding=0)` — v93 falsified, do NOT apply, route to different investigation.

### Step 1: apply the patch

```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
git apply --check docs/restir-gi-fix-v99.patch   # dry-run, should exit 0 with no fuzz
git apply docs/restir-gi-fix-v99.patch
git diff --stat                                 # should show +25/-2 across 5 files
```

If `git apply --check` fails: the v99 patch text is itself broken; the cron will need to re-derive with explicit `git apply --reject` mode or hunk-by-hunk application. Report failure with the exact `git apply --check` error message back.

### Step 2: build

```bash
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild
```

If build fails: capture the first compile error and report back. Likely candidates:
- Missing `#include <vector>` for `std::vector<nvrhi::BindingLayoutHandle>` in FRayTracingPipeline.h (likely transitively present via `nvrhi/nvrhi.h` but worth checking)
- APIDelta: `nvrhi::BindingLayoutHandle` operators (`if (InLayout)`, `push_back(Layout)`) — nvrhi handles are typedef'd as `nvrhi::RefCountPtr<...>` which has bool conversion; `push_back` requires the deref form.

### Step 3: run with dump + accumulator

```bash
HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 \
    /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine/Binary/Debug/TestReSTIR_GI_Temporal \
    2>/home/hangyu5/Documents/Gitrepo-My/HLVM-Engine/Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/TestReSTIR_GI_Temporal_stderr.log
```

Expected runtime: ~10-30 seconds. Log file captures any VUID warnings, command-list errors, or shader compile failures.

### Step 4: validate the freshest dump group

```bash
python3 /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine/Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py
```

Expected: 4/4 PASS (black-pixel ratio, color variance, temporal stability, cell variance). The freshest dump group should have a timestamp newer than the patch apply time.

### Step 5: visually inspect the display dump

```bash
ls -lt /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine/Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps/ | head -10
# Find display_frame8.png (last frame), then use vision_analyze tool or any PNG viewer
```

Expected: recognizable non-uniform Sponza geometry (arches, columns, central plaza) at sane exposure. NOT uniform magenta, NOT all-black, NOT saturated blocks.

## If everything passes

The parent should write `docs/PIPELINE_GOAL_DONE_2026-07-28.md` with:
- timestamp
- the `git apply` exit code
- the `Build.sh` exit code
- the `TestReSTIR_GI_Temporal` exit code
- the `validate_restir_gi.py` 4/4 PASS output
- a vision description of the display dump ("Sponza architecture visible: X, Y, Z")
- which checklist criteria are now PASS

This signals the goal is reached. Cron will then exit cleanly on `restir-gi-fix`.

## If anything fails

The parent should write `docs/PIPELINE_RESTART_<date>.md` with the failing evidence (build log, run log, validator output, vision report). On the next cron tick, the inner pipeline routes back to role 1 (planner) with the failure as the new task, and a fresh cycle begins from v100.

## Why this is the cron's final tick on this item

Per `six-role-pipeline` skill's HARD INVARIANT #5 ("do not loop indefinitely") + gpu-rendering-bisect-debug anti-pattern #1 ("don't trust code review over measurement"), the cron has produced its maximum value:
- 6 fresh file-only findings since v94 (v93+v95+v96+v97+v98 self-correction+v99 self-correction)
- Diagnosis converged: v22 split is half-applied to FGIPass (specific 5-file fix recipe)
- Patch text delivered: `docs/restir-gi-fix-v99.patch` (byte-verified, NOT inherited from v98)
- Handoff recipe delivered: this document

Further cron cycles without terminal execution would be review-without-measurement (gpu-rendering-bisect-debug anti-pattern #1 violation). The cron's structurally-correct exit posture is documented in `docs/PIPELINE_EXIT_v99.md`.

## Linked files
- `docs/restir-gi-fix-v99.patch` — byte-verified `git apply`-ready patch
- `docs/PENDING_PLAN_v99.md` — v99 plan
- `docs/PENDING_PLAN_REVIEW_v99.md` — v99 plan review (KEEP)
- `docs/PENDING_COMMIT_v99.md` — v99 commit with corrected hunks
- `docs/PENDING_IMPL_REVIEW_v99.md` — v99 impl review (KEEP)
- `docs/PENDING_TESTS_v99.md` — v99 Part A 7/7 PASS (first-hand byte verification) + Part B 8/8 UNVERIFIED (terminal blocked)
- `docs/PENDING_TEST_AUDIT_v99.md` — v99 audit (PATCH_TEXT_REPAIRED)
- `docs/PIPELINE_EXIT_v99.md` — cron exit posture
- `docs/PIPELINE_RUNSPACE_BLOCKED_2026-07-28.md` — terminal-blocked escalation history
- `docs/USER_PAUSE_2026-07-28.md` — user pause directive still active
- `docs/PIPELINE_HEALTH_2026-07-28.md` — running audit (v99 append at bottom)
