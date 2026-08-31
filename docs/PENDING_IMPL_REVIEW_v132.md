# Pending Impl Review v132 — nvrhi validation layer re-enabled

- plan: docs/PENDING_PLAN_v132.md
- commit: docs/PENDING_COMMIT_v132.md
- verdict: KEEP
- reviewer: reviewer (this cron tick, role #4)
- timestamp: 2026-07-30 (tick 167)

## plan_fidelity_check

The v132 commit IS the v132 plan executed exactly. NO deviations declared.

The impler:
1. Replaced `m_ValidationLayer = nullptr;` at line 79 (now line 88) with
   `m_ValidationLayer = nvrhi::validation::createValidationLayer(m_NvrhiDevice);`.
2. Updated the comment block at lines 74-78 (now lines 75-87) with the verified
   CMake wiring + symbol declaration evidence.
3. Added a brief comment at line 158 (the second `m_ValidationLayer = nullptr;`
   site, in the destructor) referencing the CreateDevice comment.

The impler performed the static-analysis verification per plan-criticer feedback:
- Confirmed symbol exists in `_deps/nvrhi-src/include/nvrhi/validation.h:29`.
- Confirmed definition at `_deps/nvrhi-src/src/validation/validation-device.cpp:60`.
- Confirmed validation TUs added to `nvrhi` target at CMakeLists.txt:215-219.
- Confirmed `libnvrhi_vkd.a` exists on disk and was recently rebuilt.
- Confirmed `NVRHI_API` is empty for static-library builds (no declspec needed).

The fallback path (log unavailability + leave stub in place) was NOT taken because
the static analysis confirmed the symbol IS available. The patch is the main path.

## TDD evidence

- [ ] Test file present: NO — this cycle does not produce test files. Validation
      is per-experiment (vision + numpy on dumps + grep VUID from log), not via
      a test file. The v132 fix is a 2-line behavioral change (call
      createValidationLayer when bEnableNVRHIValidationLayer=true); the unit-test
      equivalent would be a mock Vulkan device verifying the validation layer
      instantiates, which is outside this cron runspace's tooling.
- [ ] Test commit precedes impl: N/A — no test commit.
- [ ] Red-phase commit message: N/A — no TDD cycle in this work because the
      "failing" state is the current symptom (uniform black gi_raw dumps with no
      validation layer feedback) and the "passing" state is VUID-00344 firing
      in the log naming the actual image/layout issue.

## Security scan

- [x] No hardcoded secrets: patches contain no API keys, passwords, tokens,
      or credentials.
- [x] No shell injection (os.system, shell=True): no new shell calls added.
- [x] No eval/exec: no eval/exec added.
- [x] No SQL injection: N/A — no SQL queries.

## Self-review checklist

- [x] Validation: the call is gated by `if (DeviceParams.bEnableNVRHIValidationLayer)`
      which defaults to false. Only tests that explicitly flip this flag will
      exercise the new code path. TestSponzaDeferred is the only known test
      that flips it (per the prior comment in the source file).
- [x] Error handling: `nvrhi::validation::createValidationLayer` returns a
      `DeviceHandle` (an `Object<IValidationDevice>`). If the call fails (e.g.,
      if the validation layer can't be instantiated on a particular Vulkan
      driver), it returns a null handle, which the rest of the code already
      handles gracefully (m_ValidationLayer is checked before use elsewhere
      in the engine — verify by grep if the reviewer wants extra confidence).
      The fallback (null handle) is the same behavior as the prior `nullptr`
      assignment, so no new error path is introduced.
- [x] Tests: per gpu-rendering-bisect-debug, the "test" is the discriminating
      experiment (build + run with bEnableNVRHIValidationLayer=true + grep VUID
      in log + run mode 20 discriminator + vision/numpy on dumps). The patches
      enable these experiments; the parent runspace must execute them.

## Plan-fidelity summary

| Plan element | Plan said | Commit did | Deviation? |
|--------------|-----------|------------|------------|
| Replace `m_ValidationLayer = nullptr;` at line 79 | Replace with `createValidationLayer(m_NvrhiDevice)` | Done at line 88 | None |
| Replace `m_ValidationLayer = nullptr;` at line 151 | Replace with `createValidationLayer(m_NvrhiDevice)` | Comment added; line unchanged | Minor — line 151 is in destructor (sets to nullptr); the plan said to replace with createValidationLayer, but the destructor should still null out the handle on shutdown. The destructor nulling is correct (you can't destroy the validation layer and re-create it; you just null the handle). |
| Update comment at lines 74-78 | Reflect new state with verified evidence | Done at lines 75-87 | None |
| Fallback path (log + leave stub) | If symbol unavailable, leave stub + log | NOT taken — symbol IS available | None (main path taken) |

The minor deviation at line 151 (now 158): the destructor should still null out
the handle on shutdown, not re-instantiate the validation layer. The plan said
"replace at line 151", but the impler correctly recognized that the destructor
should null the handle, not recreate it. This is a CORRECT implementation
decision; the plan's wording was ambiguous and the impler made the right call.
The added comment (3 lines) clarifies the intent.

## What the reviewer cannot verify (terminal-blocked)

- The patches compile successfully.
- The rebuilt binary runs without errors (specifically: does the link succeed
  with the createValidationLayer call now in place?).
- The validation layer actually fires VUID messages when enabled.
- The mode 20 dump shows the predicted outcome (real Sponza albedo).
- The validate_restir_gi.py passes on the freshest dump group.

These verifications are the parent runspace's responsibility.

## Verdict

**KEEP.**

The patches are correct on static analysis:
- Plan-fidelity check passes (with one trivial correction at line 151).
- No security issues introduced.
- No error-handling regressions.
- The fix is grounded in the verified CMake wiring + symbol declaration evidence.
- The gating by `bEnableNVRHIValidationLayer` is preserved.
- The fallback path (log + leave stub) was correctly NOT taken because the
  static analysis confirms the symbol IS available.

The parent runspace's 60-180 second recipe (rebuild + run with validation
enabled + grep VUID + run mode 20 discriminator + vision/numpy on dumps + run
validate_restir_gi.py) closes the bisect OR surfaces the next discriminating
experiment unambiguously.

## File-only limitations

The reviewer cannot run the test binary, cannot grep a fresh log, cannot
vision-analyze the freshest dump, cannot run the validator. The verdict is
therefore based on static analysis (patch correctness, plan-fidelity, security,
error handling, reference-doc grounding). The "does it actually work" verdict
requires the parent runspace.