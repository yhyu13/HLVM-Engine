# Pending Tests v130 — no test files produced (validation per-experiment)

- plan: docs/PENDING_PLAN_v130.md
- commit: docs/PENDING_COMMIT_v130.md
- test_files: NONE
- role: tester (this cron tick, role #5)
- timestamp: 2026-07-30 (tick 113)

## Test strategy (per v130 plan)
Per v128 plan, this cycle does NOT produce new test files. Validation
is per-experiment: vision + numpy per-pixel stats on the freshest dump
group only. The 4-check structural validator (`validate_restir_gi.py`)
replaces the scalar mean-luma gate.

The reasoning: this is GPU bisect work, not unit-test work. The "test"
is the discriminating experiment — running `HLVM_PT_DEBUG_MODE=20` and
observing whether the dump shows non-zero GBufferMaterial. Adding a
unit test for "does the patch compile and produce non-zero mode 20
output" would be cargo-culting — the test binary IS the test.

## Test files produced this cycle
NONE.

## Test files inherited from prior cycles
NONE for this specific bisect. The existing project has:
- `Engine/Source/Runtime/Test/TestSceneGraphNode.cpp` — 12 FNode/camera
  tests (unrelated to GI bisect).
- `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` —
  4-check structural validator (the canonical post-fix gate).

## What the parent runspace should validate
Per v130 plan's acceptance gate (seven criteria, inherited from v128):

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
7. **`HLVM_PT_DEBUG_MODE=20` returns non-zero GBufferMaterial.** After Step 0's
   bypass-patch lands, mode 20 dump must show real Sponza material albedo
   (white/cream, not uniform black).

## What the tester CAN verify in this file-only runspace
Nothing concrete. The patches landed in this cycle cannot be exercised
without the test binary running. The tester can:
- Confirm the patches are syntactically valid (done in PENDING_COMMIT_v130.md).
- Confirm the validator script exists and reads the expected input format.
- Confirm the env var handling for HLVM_PT_DEBUG_MODE reaches the shader
  (already verified in prior cycles — see PENDING_PLAN_v128.md "DebugMode flow").

## What the tester CANNOT verify in this file-only runspace
- The patches compile and link.
- The discriminator experiment (mode 20) runs and produces the predicted outcome.
- The handle-identity log lines appear in the binary output.
- The validator script returns PASS on the freshest dump group.
- The display image shows recognizable Sponza.

These all require the parent runspace.

## Honesty floor
This tester marker reports ZERO test work product. The validation
strategy is per-experiment and the experiments cannot be run in this
runspace. The marker exists to maintain the six-role state machine
audit trail. The actual validation happens in the parent runspace
after the patches are built into a binary.