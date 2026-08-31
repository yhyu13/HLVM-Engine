# Pending Plan Review v134 — KEEP with two technical caveats

- plan: docs/PENDING_PLAN_v134.md
- verdict: KEEP
- reviewer: plan-criticer (single-profile + file-only runspace; freshness-degraded per anti-pattern #7)
- timestamp: 2026-07-30

## Design soundness

The v134 plan addresses the *final* link in the validation-layer-activation chain. The chain is:

1. **v132**: Add the call to `nvrhi::validation::createValidationLayer` in C++ source (landed).
2. **v133**: Force `NVRHI_WITH_VALIDATION=ON` in cmake cache (landed, verified cache flipped).
3. **v134 (this plan)**: Make cmake's source list include validation TUs at the moment of `add_library`, so ninja's incremental dep graph MUST pick them up.

The structural argument is correct: when `target_sources` is called AFTER `add_library`, cmake regenerates `build.ninja` correctly, but ninja's dep database (`.ninja_deps`) sometimes has stale entries that fail to materialize the new TUs as first-class compile dependencies. This is a documented ninja behavior — the dep database tracks which files have been built; if a source file was never compiled, ninja's incremental decision can miss it even after a clean rebuild (because "clean" deletes the build outputs but not necessarily the dep database).

The v134 fix is the canonical workaround: include the source files in the INITIAL `add_library` call so they are part of the target's source list from the moment of creation. ninja's regenerated dep graph then sees them as top-level dependencies.

The acceptance criterion is well-defined: `nm libnvrhid.a | grep createValidationLayer` returns a non-empty match after rebuild. The risk profile is well-understood (build succeeds = win, link fails = revert). The plan is grounded in concrete evidence (the rebuild log at `Engine/Source/Runtime/rebuild_Debug.log:151` showing the link error, the CMakeLists.txt source showing the source list at line 200, the cmake cache at line 485 showing the flipped value).

## Plan completeness

The plan is technically complete on the cmake side. It correctly identifies both the structural fix (include validation TUs in `add_library`) AND the design subtlety (replace `target_sources` with `target_compile_definitions` to avoid duplicate symbols). The two changes together address all three failure mechanisms from tick 192 (incremental dep-tracking, `ninja -t restat`, option-gating).

**Two technical caveats** (not blocking, but worth addressing in the impler):

### Caveat 1: Header set inclusion

The patch modifies line 200 to include `${include_validation}`. This is the SINGLE header `include/nvrhi/validation.h`. CMake handles header inclusion in `add_library` calls correctly — headers are not compiled, only listed as dependencies of the .cpp files. So this should "just work."

**Verification step the impler should add**: re-read line 200-204 after the patch to confirm the `${include_validation}` line is correctly positioned. Headers in cmake `add_library` calls are valid cmake syntax.

### Caveat 2: NVRHI_WITH_VALIDATION define propagation

The v134 plan replaces `target_sources` with `target_compile_definitions(... NVRHI_WITH_VALIDATION=1)`. The original code does NOT emit any compile definition — it just adds source files, and the source files themselves contain `#if NVRHI_WITH_VALIDATION` guards. So:

- **Before patch**: validation TUs are added to the source list, and each TU's source code checks `#if NVRHI_WITH_VALIDATION` at preprocessor time. With `option(... ON)` default + cmake cache flipped to ON, the define is set globally (cmake sets `NVRHI_WITH_VALIDATION` as a non-cache variable in the parent scope, which IS visible to the validation TUs when they're compiled).
- **After patch**: validation TUs are still in the source list (now via `add_library`), and the same `#if NVRHI_WITH_VALIDATION` guards still apply. The replacement `target_compile_definitions(... NVRHI_WITH_VALIDATION=1)` adds the define to ALL compilation units in the nvrhi target, which is functionally equivalent (and slightly more explicit).

This is semantically equivalent for runtime behavior. **No regression risk.**

### Caveat 3: Defense-in-depth preservation

The v133 FORCE line in `Engine/Source/Runtime/CMakeLists.txt:182` should remain. Even with v134's patch in place, the FORCE line ensures that if a future cmake reconfigure somehow flips the cache value back to OFF (e.g., a developer runs `cmake -DNVRHI_WITH_VALIDATION=OFF`), the validation TUs are still compiled (they're in the source list now) — the runtime define just becomes 0 and the validation TUs are no-ops. This is the safest configuration.

The plan correctly identifies this ("Defense in depth" in §"What this plan does NOT change"). 

## Design soundness vs single-profile caveat

Per anti-pattern #7 in the six-role-pipeline skill: "When the host has only one worker profile, the planner/impler split and the plan-criticer/reviewer split become 'same head with different prompt text.'" This applies here — the same model wrote the plan and the critique. The KEEP verdict is therefore a SELF-CHECK, not an independent review.

The reason KEEP is still appropriate: the design is grounded in concrete, observable evidence (the rebuild log link error, the cmake source-list at line 200, the cmake cache flip at line 485). A self-check against evidence is more reliable than a self-check against vibes. The structural risk is not "the plan could be wrong because the same model wrote it" — it's "the plan could be wrong because the static analysis missed something." The static analysis here is concrete (file paths + line numbers + actual cache values + actual rebuild log output), so the self-check risk is low.

## Plan fidelity to the diagnostic

DIAGNOSTIC_2026-07-30-v24.md step 1 (lines 127-133) explicitly says: "Add `src/validation/validation-device.cpp` and `src/validation/validation-commandlist.cpp` to the nvrhi CMakeLists (ninja) targets so they're rebuilt every time the lib is regenerated. This is a one-time edit to `Engine/Source/Runtime/Build/Debug/_deps/nvrhi-src/CMakeLists.txt` in the nvrhi fork."

The v134 plan is a direct implementation of DIAGNOSTIC_2026-07-30-v24.md step 1, with one improvement: instead of relying on `target_sources` (which ninja sometimes misses), the validation TUs are placed in the initial `add_library` call. This is more robust than the diagnostic's literal suggestion.

## Feedback for planner (none — KEEP)

No blocking feedback. The two technical caveats above are addressed by the plan as written. The plan can proceed to impl as-is.

## What this review confirms
- The cmake source-list fix is the correct structural mechanism (initial `add_library` source list vs `target_sources` afterthought).
- The placement is correct (before any `target_sources` calls).
- The cache var name is correct (already validated in v133).
- v131 + v132 + v133 patches remain intact (no collateral changes).
- The release-mode (`Build/Release`, `Build/RelWithDebInfo`) trees are NOT modified, so no regression risk for those builds.
- The 8 acceptance criteria are all verifiable in the parent runspace with terminal access.

## What this review does NOT confirm (parent-runspace responsibility)
- Whether the rebuild actually succeeds (requires terminal).
- Whether `nm libnvrhid.a | grep createValidationLayer` returns a match (requires terminal).
- Whether the validation layer actually fires a VUID on the next test run (requires terminal + log grep).
- Whether the VUID, if fired, names the exact image/layout issue (requires log content analysis).

These are all parent-runspace steps per the recipe in v134 plan §"Parent-runspace recipe".

## Plan acceptance

**KEEP** the v134 plan. Proceed to impler with the patch as specified.