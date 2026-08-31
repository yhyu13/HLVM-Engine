# Pending Plan Review v132 — re-enable nvrhi validation layer hookup

- plan: docs/PENDING_PLAN_v132.md
- verdict: KEEP
- reviewer: plan-criticer (this cron tick, role #2)
- timestamp: 2026-07-30 (tick 167)

## Design soundness

The v132 plan addresses a STRUCTURALLY DIFFERENT root-cause gate from v131:

| Aspect | v131 (commitBarriers + case 31u) | v132 (re-enable validation layer) |
|---|---|---|
| Mechanism | Blindly flush barriers before dispatch | Let validation layer name the actual VUID |
| File | `FGIPass.cpp:668`, `GIPathTracing.hlsl:712,479` | `DeviceManagerVk4_LifeCycle.cpp:79,151` |
| Verification | Vision + numpy on dumps after rebuild | Vulkan validation layer error in log |
| Risk if wrong | commitBarriers is idempotent (no-op) | Link may fail (revert is documented) |
| Discriminator | Mode 20/31 dump analysis | VUID surfacing in log |

The two are complementary, not duplicative. v131 is the candidate-B fix attempt; v132 is the validation-layer hookup that surfaces whatever the actual Vulkan error is. Even if v131's fix is correct, v132 confirms it; if v131's fix is wrong, v132 names the actual error and shows v131 was misdirected.

The plan is grounded in tick 166's smoking-gun finding:
- nvrhi's CMakeLists.txt has `NVRHI_WITH_VALIDATION=ON` (default) — verified at `_deps/nvrhi-src/CMakeLists.txt:36`
- Validation TUs are listed at CMakeLists.txt:124-127
- Validation TUs are added to the nvrhi target at CMakeLists.txt:215-219 (conditional on NVRHI_WITH_VALIDATION)
- Header declaration at `_deps/nvrhi-src/include/nvrhi/validation.h:29`: `NVRHI_API DeviceHandle createValidationLayer(IDevice* underlyingDevice);` (with `NVRHI_API` export)
- Source definition at `_deps/nvrhi-src/src/validation/validation-device.cpp:60`: same signature

The stub comment at `DeviceManagerVk4_LifeCycle.cpp:74-78` claims the TUs aren't compiled into `libnvrhi_vkd.a`. Tick 166's verification contradicts this claim — the CMake wiring IS correct. The stub is therefore an obsolete workaround from an earlier build state.

The plan correctly identifies the linker risk and provides a fallback:
- If the lib actually exports the symbol → revert the stub (the plan's main path)
- If the lib doesn't export the symbol → fall back to logging the unavailability and leaving the stub in place (the plan's fallback path)

This conditional approach is honest about the empirical question (does the lib export the symbol?) and provides a graceful degradation path.

## Plan completeness

The plan covers:
1. **Why this plan is NOT phantom-cycle**: structural difference from v131 (different file, different mechanism, different risk profile).
2. **Implementation step**: 2-line edit + comment update + fallback path.
3. **Verification step**: grep the nvrhi CMakeLists for `add_library(nvrhi ...)` and confirm the validation TUs are added to the SAME target that becomes `libnvrhi_vkd.a`. If yes, revert the stub. If no, leave the stub and log unavailability.
4. **Parent-runspace recipe**: 7-step recipe (rebuild → check build → run with validation enabled → read VUID → discriminator sweep → final acceptance gate).
5. **Risk analysis**: linker risk, runtime risk, compile-time risk — all addressed.
6. **Acceptance gate**: 7 criteria inherited from v130/v131 + 1 new criterion (VUID surfaces in log).

What is NOT in scope of v132 but is correctly identified:
- The actual fix to apply based on the VUID the validation layer surfaces. This belongs in a follow-up cycle (v133) once the VUID names the issue.
- The build verification (whether the rebuilt binary actually contains the validation layer). This requires terminal in parent runspace.
- The discriminator sweep (mode 20/31). This was v131's plan and remains in the parent-runspace recipe.

The plan correctly limits its scope to "re-enable the validation layer so it can fire VUID messages, then let the parent runspace observe the output." The fix is conditional based on symbol availability.

## Feedback for planner (FIX only)

Three minor improvements the impler should apply during implementation:

1. **Verify the symbol is in the lib BEFORE reverting the stub** (per the plan's verification step). The impler should grep `Engine/Source/Runtime/Build/Debug/_deps/nvrhi-src/CMakeLists.txt` for `add_library(nvrhi` and check whether the validation TUs are added to the SAME target. If `target_sources(nvrhi PRIVATE ${src_validation})` is on `nvrhi` (which becomes `libnvrhi_vkd.a`), the symbol is in the lib. If the validation TUs are added to a DIFFERENT target (e.g., `nvrhi_validation` or `nvrhi-validation`), the symbol is NOT in `libnvrhi_vkd.a` and the revert would break the link.

2. **Add a runtime gate** so the fix doesn't affect tests that don't opt in. The branch is gated by `DeviceParams.bEnableNVRHIValidationLayer` (default false), so only tests that explicitly set this flag will use the validation layer. The impler should verify this gating is preserved in the new code (it should be, since the `if (DeviceParams.bEnableNVRHIValidationLayer)` block is unchanged).

3. **Verify the nvrhi fork's `NVRHI_API` macro expansion on Linux**. The macro must expand to the export declspec on Linux (e.g., `__attribute__((visibility("default")))`). If `NVRHI_API` is undefined on Linux, the symbol may not be exported even if the TU is compiled. The impler should grep `_deps/nvrhi-src/include/nvrhi/nvrhi.h` (or the equivalent base header) for the `NVRHI_API` macro definition. If it's `__declspec(dllexport)` only (Windows), the symbol won't be exported on Linux and the link will fail.

## Note on honesty floor

This plan is structurally different from v125/v126/v127/v128/v130/v131. Spawning v132 is NOT phantom-cycle per HARD INVARIANT #4 + dispatcher no-fabrication rule:
- Different file (DeviceManagerVk4_LifeCycle.cpp).
- Different mechanism (validation-layer runtime hookup vs GI SRV probes).
- Different verification target (VUID surfacing vs dump analysis).
- Different risk profile (link-time risk vs compile/runtime risk).

The plan correctly acknowledges the linker risk and provides a graceful fallback. The plan is honest about what can be verified file-only (symbol declarations, CMake wiring) vs what requires terminal (rebuild + run + VUID surfacing).

## Verdict

**KEEP** with three implementation caveats (verify symbol in lib, preserve gating, verify NVRHI_API on Linux).

The plan is correctly designed to surface the actual Vulkan error via the validation layer, which is what the bisect needs to close. The conditional implementation (revert-if-symbol-exists, fallback-if-not) is honest about the empirical question.

The plan is approved for implementation. The impler should apply the three minor improvements during implementation (symbol verification, gating preservation, NVRHI_API check).