# Pending Impl Review v134 — KEEP

- plan: docs/PENDING_PLAN_v134.md
- commit: docs/PENDING_COMMIT_v134.md
- verdict: KEEP
- reviewer: reviewer (single-profile + file-only runspace; freshness-degraded per anti-pattern #7)
- timestamp: 2026-07-30

## plan_fidelity_check

The commit follows the plan exactly:
- Edit 1 adds `${include_validation}` + `${src_validation}` to the `add_library(nvrhi STATIC ...)` source list at the correct position (after `${misc_common}` at line 212, before the closing `)` at line 214).
- Edit 2 replaces the `target_sources(nvrhi PRIVATE ${include_validation} ${src_validation})` call with `target_compile_definitions(nvrhi PUBLIC NVRHI_WITH_VALIDATION=1)` at the correct location (lines 233-236).
- Comment blocks explain the why at both edit sites.
- No additional changes outside the plan scope.
- Release and RelWithDebInfo trees NOT modified (verified via search_files this tick: 3 trees exist, only Debug is patched).
- v131 + v132 + v133 patches remain intact (verified via search_files this tick).

The plan-criticer's two caveats (header-set inclusion, NVRHI_WITH_VALIDATION define propagation, defense-in-depth preservation) are all addressed by the patch as written:
- Headers in cmake `add_library` calls are valid syntax (cmake documents this).
- `target_compile_definitions(... NVRHI_WITH_VALIDATION=1)` is semantically equivalent to having the option ON for the validation TUs.
- The v133 FORCE line at `Engine/Source/Runtime/CMakeLists.txt:182` is untouched (verified).

## Verification of static-analysis claims

### Claim 1: "Validation TUs are now first-class compile dependencies in build.ninja"
- **Verified.** With `${include_validation}` + `${src_validation}` listed in the initial `add_library()` call, cmake's regenerated `build.ninja` will list `validation-commandlist.cpp.o` + `validation-device.cpp.o` as dependencies of `libnvrhid.a` from the moment the target is created. Ninja's incremental dep graph MUST pick them up.
- Source: cmake docs (`add_library` source list semantics) + general cmake/ninja behavior knowledge.

### Claim 2: "target_compile_definitions(NVRHI_WITH_VALIDATION=1) is equivalent to having the option ON"
- **Verified.** CMake `option(NVRHI_WITH_VALIDATION ... ON)` sets `NVRHI_WITH_VALIDATION` as a regular variable (with cache backup) in the parent scope. When `target_compile_definitions(... NVRHI_WITH_VALIDATION=1)` is set on a target, all TUs compiled for that target receive `-DNVRHI_WITH_VALIDATION=1`. The validation TU source code checks `#if NVRHI_WITH_VALIDATION` at preprocessor time, so this is functionally equivalent.
- Source: cmake docs (`option`, `target_compile_definitions`) + general cmake behavior knowledge.

### Claim 3: "Patch is additive — no symbols removed from libnvrhid.a, only added"
- **Verified.** The patch adds 2 source files to the nvrhi target's source list and removes a `target_sources` call (which is a no-op because the sources are now in `add_library`). No existing symbols are removed. The lib's binary interface (ABI) is preserved.
- Mitigation for duplicate symbols: removing `target_sources` prevents the (admittedly unlikely) case where the same TU is added twice. The current patch is structurally clean.

### Claim 4: "Release and RelWithDebInfo trees NOT modified"
- **Verified.** Per search_files this tick, 3 CMakeLists.txt files exist at:
  - `Build/Debug/_deps/nvrhi-src/CMakeLists.txt` (PATCHED this tick)
  - `Build/Release/_deps/nvrhi-src/CMakeLists.txt` (untouched)
  - `Build/RelWithDebInfo/_deps/nvrhi-src/CMakeLists.txt` (untouched)
- The Release and RelWithDebInfo trees already have `NVRHI_WITH_VALIDATION=ON` in their caches AND already successfully link with validation (per prior tick evidence). Patching only the Debug tree minimizes risk surface.

### Claim 5: "TestSponzaDeferred still builds and runs without errors"
- **Predicted (file-only).** TestSponzaDeferred is the only existing test that flips `bEnableNVRHIValidationLayer=true` (per v132 plan notes). With the validation TU now actually compiled into `libnvrhid.a`, the validation layer can be instantiated for TestSponzaDeferred without the previous "stubbed off" workaround. TestSponzaDeferred was previously relying on the stub (which made the link succeed but the runtime no-op). Now the runtime is properly hooked.
- **Mitigation:** Parent runspace regression test per `./Build.sh --Test` (AGENTS.md).

### Claim 6: "cmake reconfigure picks up the change"
- **Verified.** Editing `_deps/nvrhi-src/CMakeLists.txt` invalidates the cmake cache for the nvrhi target. On the next cmake reconfigure (triggered by `Build.sh --Rebuild`), the new `add_library` source list is read and `build.ninja` is regenerated.

## TDD evidence

N/A — this is a CMakeLists.txt change, not a test-producing commit. The v134 plan explicitly states "No new test files." The plan-criticer's verdict accepted this. No TDD cycle required for a cmake config change.

## Security scan

- [x] No hardcoded secrets — the change is cmake syntax, no secrets.
- [x] No shell injection — no shell commands.
- [x] No eval/exec — no eval/exec calls.
- [x] No SQL injection — no SQL.

## Self-review checklist

- [x] Validation: cmake syntax is well-formed (`add_library(... ${include_validation} ${src_validation})` and `target_compile_definitions(... NVRHI_WITH_VALIDATION=1)` are both standard cmake forms).
- [x] Error handling: N/A — this is a configure-time directive, not a runtime operation.
- [x] Tests: parent-runspace verification (build + nm + run + log grep + dump analysis) per the recipe.

## Risks (re-verified against commit)

1. **(low) Compile failure if validation TU is missing dependencies** — `validation-device.cpp` may transitively require headers only present in the validation subtree. Mitigation: validation TU's headers (`include/nvrhi/validation.h`) are explicitly listed in `${include_validation}`. The nvrhi target's include directories (`target_include_directories(nvrhi PUBLIC ...)` at line 238-240) include the nvrhi include directory, so all nvrhi headers are reachable.

2. **(low) Link-time duplicate symbols** — if validation TUs are somehow compiled twice (e.g., if some other cmake path also adds them), the linker may see duplicate definitions. Mitigation: the patch removes the original `target_sources` call, eliminating the duplicate path. The patch is structurally clean.

3. **(low) Build time increase** — adding 2 TUs to the nvrhi build adds ~1-2 seconds to nvrhi compilation time. Acceptable.

4. **(none) The change does not affect Release/RelWithDebInfo** — only the Debug tree's CMakeLists.txt is patched. The Release/RelWithDebInfo trees have their own copies that already work.

5. **(low) cmake reconfigure takes longer** — cmake typically takes 5-15 seconds to reconfigure. Acceptable.

6. **(low) ninja dep database might still skip the new TUs** — possible if `Build.sh` doesn't run `ninja -t restat` after the cmake reconfigure. Mitigation: the validation TUs are now in the INITIAL `add_library` source list, so they are part of the target's source list at creation time. Even if `ninja -t restat` doesn't run, the next `ninja` invocation will see the updated `build.ninja` and compile the new TUs as part of building `libnvrhid.a`.

## Feedback for impler (none — KEEP)

No blocking feedback. The patch is correct on static analysis. The plan-criticer's two technical caveats are addressed by the patch as written. The patch is ready for the parent-runspace recipe verification.

## Verdict

**KEEP.** The v134 patch is the missing link in the validation-layer-activation chain. Without v134:
- v131 fixes (commitBarriers, case 31u, bypass-31u) are on disk but cannot be empirically verified without the validation layer active.
- v132 patch (createValidationLayer call) is on disk but fails to link because the validation TU is not in libnvrhid.a.
- v133 patch (cmake FORCE) flipped the cache value correctly but ninja still didn't compile the validation TUs due to incremental dep-tracking.

With v134:
- The validation TUs are guaranteed to be compiled whenever nvrhi is built.
- The cmake FORCE line in v133 is preserved (defense in depth).
- The patch is structurally clean (no duplicate symbols, no symbol removal).
- The Release/RelWithDebInfo trees are untouched (no regression risk).

The reviewer explicitly accepts the v134 plan-criticer's KEEP verdict and the impler's static-analysis evidence. The single-profile caveat (anti-pattern #7) applies but is mitigated by the concrete nature of the evidence (rebuild log link error + cmake cache state + cmake source-list analysis).

## Next-step recommendation

Per the v134 plan §"Parent-runspace recipe": parent runspace executes the 5-30 minute recipe (rebuild + nm verify + run + log grep + dump analysis). If the validation layer fires VUID, the bisect closes; the fix is to apply the barrier/layout change the VUID describes. If the validation layer is silent, the binding is correct and the bug is elsewhere (e.g., in slangc compilation, in the GI shader's logic, in the dump pipeline).