# Pending Tests v132 — no test files produced (validation per-experiment)

- plan: docs/PENDING_PLAN_v132.md
- commit: docs/PENDING_COMMIT_v132.md
- test_files: NONE
- role: tester (this cron tick, role #5)
- timestamp: 2026-07-30 (tick 167)

## Test strategy (per v132 plan)

Per v132 plan, this cycle does NOT produce new test files. Validation is per-experiment: rebuild + run with `bEnableNVRHIValidationLayer=true` + grep VUID in log + run mode 20 discriminator (inherited from v131) + vision/numpy on fresh dumps + run `validate_restir_gi.py` (inherited from v130). The 4-check structural validator replaces the scalar mean-luma gate.

The reasoning: this is GPU bisect work, not unit-test work. The "test" is the discriminating experiment — running the validation-layer-enabled test and observing whether VUID-00344 (or similar) fires naming the actual image/layout issue. Adding a unit test for "does the validation layer instantiate" would be cargo-culting — the test binary IS the test.

## Test files produced this cycle

NONE.

## Test files inherited from prior cycles

NONE for this specific bisect. The existing project has:
- `Engine/Source/Runtime/Test/TestSceneGraphNode.cpp` — 12 FNode/camera tests (unrelated to GI bisect).
- `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` — 4-check structural validator (the canonical post-fix gate).

## What the parent runspace should validate

Per v132 plan's acceptance gate (7 criteria inherited from v130 + 1 new criterion):

1. **Debug target builds.** `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal`
   exits 0 with no warnings. **NEW for v132**: this verifies the link succeeded with
   the `nvrhi::validation::createValidationLayer` call now in place. If the link
   fails with `undefined reference to 'nvrhi::validation::createValidationLayer'`,
   the patch needs to be reverted and the fallback path (log + leave stub) should
   be applied instead.

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
   v131's Candidate B fix (commitBarriers) AND v132's validation-layer
   re-enable land, mode 20 dump must show real Sponza material albedo
   (white/cream, not uniform black).

**Additional v132 criterion (criterion 8):**
8. **Validation layer fires VUID when enabled.** If the test is run with
   `bEnableNVRHIValidationLayer=true` AND the GI shader's GBuffer SRV
   reads hit the wrong image/layout, the validation layer must fire a VUID
   naming the actual issue (likely VUID-VkImageView-imageLayout-00344 for
   SHADER_READ_ONLY_OPTIMAL mismatch, or similar). The parent runspace can
   temporarily enable the validation layer by editing
   `Engine/Source/Runtime/Private/Renderer/DeviceManagerVk4_LifeCycle.cpp`
   to default `DeviceParams.bEnableNVRHIValidationLayer=true`, OR by adding
   a CVar that flips the flag.

   Note: criterion 8 is OPTIONAL. The bisect can close without it if
   criterion 7 passes (the v131 commitBarriers fix closes the root cause
   independently of v132's validation hookup). Criterion 8 is for cases
   where the v131 fix is insufficient and we need the validation layer to
   name the actual issue.

If criterion 7 fails, additionally validate criterion 9 (inherited from v131):
9. **`HLVM_PT_DEBUG_MODE=31` discriminates Candidate A.** After v131's
   case 31u discriminator lands, mode 31 dump shows:
   - non-uniform color (transformed albedo) → slangc alive → fix
     was Candidate B → bisect closes;
   - uniform blue (0,0,1) → slangc alive but value zero → Candidate B
     fix didn't help → revert fix, investigate binding layer;
   - uniform black → slangc dead-stripped → Candidate A confirmed →
     fix path is to add keep-alive writes to modes 20/21/22.

## What the tester CAN verify in this file-only runspace

- The patches are syntactically valid C++ (verified via patch tool's diff output).
- The reverted call at line 88 uses the correct API (`nvrhi::validation::createValidationLayer`).
- The gating by `if (DeviceParams.bEnableNVRHIValidationLayer)` is preserved.
- The destructor at line 158 still nulls the handle (correct destructor behavior).

## What the tester CANNOT verify in this file-only runspace

- The patches compile and link.
- The validation layer instantiates correctly.
- The discriminator experiments (mode 20, mode 31, VUID grep) run and produce
  the predicted outcomes.
- The validate_restir_gi.py returns PASS on the freshest dump group.
- The display image shows recognizable Sponza.

These all require the parent runspace.

## Honesty floor

This tester marker reports ZERO test work product. The validation strategy is
per-experiment and the experiments cannot be run in this runspace. The marker
exists to maintain the six-role state machine audit trail. The actual validation
happens in the parent runspace after the patches are built into a binary.

## Per-experiment discriminators

| Experiment | Success | Failure | Action on failure |
|------------|---------|---------|-------------------|
| Build succeeds with createValidationLayer | Symbol is in lib → patch is correct | Linker error on createValidationLayer | Revert patch; apply fallback path (log + leave stub); proceed with v131's commitBarriers-only fix |
| Validation layer fires VUID | nvrhi validation layer instantiated correctly | Linker error or runtime error in test | Revert patch; commitBarriers fix alone (v131) is sufficient |
| Mode 20 returns non-zero | Candidate B was root cause → bisect closes | Mode 20 still zero | Run mode 31 (next discriminator) |
| Mode 31 returns transformed color | slangc alive, value zero from Candidate B fix attempt | Mode 31 blue/black | Revert Candidate B fix; investigate Candidate C (binding layer) |
| Mode 31 returns uniform blue (0,0,1) | slangc alive, value zero | (n/a) | Revert Candidate B fix; investigate Candidate C (binding layer) |
| Mode 31 returns uniform black | slangc dead-strip confirmed (Candidate A) | (n/a) | Add keep-alive writes to modes 20/21/22; rebuild |
| validate_restir_gi.py PASS | All 4 checks pass → 7-criteria gate complete | Any check fails | Iterate on remaining discriminator |
| display_frame8.png shows Sponza | Vision analysis confirms recognizable Sponza | Image is uniform/dim | Re-iterate on remaining candidates |

Each experiment is cheap (~30 seconds for build, ~30 seconds for run, ~5 seconds
for vision/numpy). The full discriminating sweep (modes 20/21/22/30/31 + final
acceptance gate + VUID check) takes 60-180 seconds in the parent runspace.

## What this tester marker does NOT do

- Does not produce test files (per the v132 plan).
- Does not run any tests (terminal blocked).
- Does not invoke any external verification (terminal blocked + vision_analyze not in toolset).

The test cycle is INCOMPLETE in this runspace. The marker exists to maintain
the six-role state machine audit trail. KEEP verdict requires the parent runspace.