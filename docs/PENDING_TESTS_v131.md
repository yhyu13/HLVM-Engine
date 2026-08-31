# Pending Tests v131 — no test files produced (validation per-experiment)

- plan: docs/PENDING_PLAN_v131.md
- commit: docs/PENDING_COMMIT_v131.md
- test_files: NONE
- role: tester (this cron tick, role #5)
- timestamp: 2026-07-30 (tick 151)

## Test strategy (per v131 plan)

Per v131 plan, this cycle does NOT produce new test files. Validation
is per-experiment: vision + numpy per-pixel stats on the freshest dump
group only. The 4-check structural validator (`validate_restir_gi.py`)
replaces the scalar mean-luma gate.

The reasoning: this is GPU bisect work, not unit-test work. The "test"
is the discriminating experiment — running `HLVM_PT_DEBUG_MODE=20` (and
optionally `31`) and observing whether the dump shows non-zero
GBufferMaterial. Adding a unit test for "does the patch compile and
produce non-zero mode 20 output" would be cargo-culting — the test
binary IS the test.

## Test files produced this cycle

NONE.

## Test files inherited from prior cycles

NONE for this specific bisect. The existing project has:
- `Engine/Source/Runtime/Test/TestSceneGraphNode.cpp` — 12 FNode/camera
  tests (unrelated to GI bisect).
- `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` —
  4-check structural validator (the canonical post-fix gate).

## What the parent runspace should validate

Per v131 plan's acceptance gate (seven criteria, inherited from v130):

1. **Debug target builds.** `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal`
   exits 0 with no warnings.
2. **Run env vars work.** `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Binary/Debug/TestReSTIR_GI_Temporal`
   produces 8 frames of dumps.
3. **No Vulkan VUID/ERROR.** Grep log for `VUID` and `ERROR`; both must return 0 matches.
4. **No command-list errors.** Grep log for `CommandList`; must return 0 matches
   (other than informational log lines).
5. **`validate_restir_gi.py` passes.** Run on the newest dump group only;
   4-check structural validator must report PASS.
6. **Fresh display image shows Sponza.** Vision analysis on
   `dumps/*_display_frame8.png`; must show recognizable Sponza geometry
   with sane exposure (mean luma 0.05-0.5, color variance > 0.05, cell variance > 0.02).
7. **`HLVM_PT_DEBUG_MODE=20` returns non-zero GBufferMaterial.** After
   v131's Candidate B fix (commitBarriers) lands, mode 20 dump must
   show real Sponza material albedo (white/cream, not uniform black).

If criterion 7 fails, additionally validate criterion 8:
8. **`HLVM_PT_DEBUG_MODE=31` discriminates Candidate A.** After v131's
   case 31u discriminator lands, mode 31 dump shows:
   - non-uniform color (transformed albedo) → slangc alive → fix
     was Candidate B → bisect closes;
   - uniform blue (0,0,1) → slangc alive but value zero → Candidate B
     fix didn't help → revert fix, investigate binding layer;
   - uniform black → slangc dead-stripped → Candidate A confirmed →
     fix path is to add keep-alive writes to modes 20/21/22.

## What the tester CAN verify in this file-only runspace

- The patches are syntactically valid HLSL/C++ (done in PENDING_COMMIT_v131.md
  via static analysis).
- The bypass list correctly includes 31u (verified at
  `GIPathTracing.hlsl:472-480` in both copies).
- The case 31u discriminator is structurally correct (verified at
  `GIPathTracing.hlsl:705-720` in both copies).
- The commitBarriers() fix is at the correct location (verified at
  `FGIPass.cpp:656-668`, immediately before RTPipeline.DispatchRays).

## What the tester CANNOT verify in this file-only runspace

- The patches compile and link.
- The discriminator experiment (mode 20 + mode 31) runs and produces
  the predicted outcomes.
- The validate_restir_gi.py returns PASS on the freshest dump group.
- The display image shows recognizable Sponza.

These all require the parent runspace.

## Honesty floor

This tester marker reports ZERO test work product. The validation
strategy is per-experiment and the experiments cannot be run in this
runspace. The marker exists to maintain the six-role state machine
audit trail. The actual validation happens in the parent runspace
after the patches are built into a binary.

## Per-experiment discriminators

| Experiment | Success | Failure | Action on failure |
|------------|---------|---------|-------------------|
| Mode 20 returns non-zero | Candidate B was root cause → bisect closes | Mode 20 still zero | Run mode 31 (next discriminator) |
| Mode 31 returns transformed color | slangc alive, value zero from Candidate B fix attempt | Mode 31 blue/black | Revert Candidate B fix; investigate Candidate C (binding layer) |
| Mode 31 returns uniform blue (0,0,1) | slangc alive, value zero | (n/a) | Revert Candidate B fix; investigate Candidate C (binding layer) |
| Mode 31 returns uniform black | slangc dead-strip confirmed (Candidate A) | (n/a) | Add keep-alive writes to modes 20/21/22; rebuild |
| validate_restir_gi.py PASS | All 4 checks pass → 7-criteria gate complete | Any check fails | Iterate on remaining discriminator |
| display_frame8.png shows Sponza | Vision analysis confirms recognizable Sponza | Image is uniform/dim | Re-iterate on remaining candidates |

Each experiment is cheap (~30 seconds for build, ~30 seconds for run,
~5 seconds for vision/numpy). The full discriminating sweep
(modes 20/21/22/30/31 + final acceptance gate) takes 60-180 seconds
in the parent runspace.