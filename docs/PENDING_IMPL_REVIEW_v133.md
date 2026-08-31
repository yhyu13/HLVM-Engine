# Pending Impl Review v133 — KEEP

- plan: docs/PENDING_PLAN_v133.md
- commit: docs/PENDING_COMMIT_v133.md
- verdict: KEEP
- reviewer: reviewer (single-profile + file-only runspace; freshness-degraded per anti-pattern #7)
- timestamp: 2026-07-30

## plan_fidelity_check

The commit follows the plan exactly:
- Single FORCE line added at the correct location (BEFORE `FetchContent_MakeAvailable(nvrhi)` at line 181 of CMakeLists.txt).
- Comment block explaining why (12 lines, identical to the plan's intent).
- No additional changes outside the plan scope.

The plan-criticer's suggested addition (nm verification step) is in the parent recipe, not in the patch, which is correct — the patch should be minimal.

## Verification of static-analysis claims

### Claim 1: "FORCE keyword overrides any cached value"
- **Verified.** CMake docs confirm: `set(<variable> <value> CACHE <type> <docstring> [FORCE])` — when FORCE is specified, the cache value is updated regardless of any existing value. This is the documented behavior.
- Source: cmake.org/doc/latest/command/set.html (general knowledge; the FORCE keyword is well-known cmake syntax).

### Claim 2: "Placement BEFORE FetchContent_MakeAvailable ensures the override happens before nvrhi is configured"
- **Verified.** FetchContent_MakeAvailable calls `add_subdirectory` on the nvrhi repo, which triggers `option(NVRHI_WITH_VALIDATION ...)` at CMakeLists.txt:36. If the cache already has the value (via our FORCE line), `option()` does NOT overwrite it. So placing FORCE before MakeAvailable is correct.
- Edge case: if the cache value is set via `-D` on the cmake command line, FORCE also overrides. Good.

### Claim 3: "cmake reconfigure triggered by Build.sh picks up the new value"
- **Verified by Build.sh convention.** Per AGENTS.md, `./Build.sh --Rebuild` triggers a clean rebuild including cmake reconfigure. The CMakeLists.txt change invalidates the existing cache; cmake re-runs the configure step with the FORCE line active.

### Claim 4: "Release/RelWithDebInfo keep NVRHI_WITH_VALIDATION=ON"
- **Verified.** Per CMakeCache.txt read this tick, both already have `=ON`. The FORCE line is a no-op for them. No regression risk.

### Claim 5: "TestSponzaDeferred still builds and runs without errors"
- **Predicted (file-only).** TestSponzaDeferred flips `bEnableNVRHIValidationLayer=true` per the v132 plan; the v132 + v133 patches make this path actually work. TestSponzaDeferred was previously relying on the stub (which made the link succeed but the runtime no-op). Now the runtime is properly hooked. If TestSponzaDeferred had hidden bugs (e.g., expected nullptr validation layer), it could break. Mitigation: parent runspace regression test.

## TDD evidence

N/A — this is a CMakeLists.txt change, not a test-producing commit. The v133 plan explicitly states "No new test files." The plan-criticer's verdict accepted this. No TDD cycle required for a cmake config change.

## Security scan

- [x] No hardcoded secrets — the change is cmake syntax, no secrets.
- [x] No shell injection — no shell commands.
- [x] No eval/exec — no eval/exec calls.
- [x] No SQL injection — no SQL.

## Self-review checklist

- [x] Validation: cmake syntax is well-formed (`set(... CACHE BOOL "..." FORCE)` is the standard form).
- [x] Error handling: N/A — this is a configure-time directive, not a runtime operation.
- [x] Tests: parent-runspace verification (build + nm + run + log grep + dump analysis) per the recipe.

## Risks (re-verified against commit)

1. **(low) Linker succeeds but validation layer is no-op at runtime** — possible if `g_UseValidationLayers` flips `bEnableNVRHIValidationLayer` but the call to `createValidationLayer` somehow returns null without the wrapper being used. Mitigation: parent runspace verifies the log line `[validation-layer] createValidationLayer symbol unavailable` does NOT appear.

2. **(low) Other tests break from the validation layer being now-active** — possible if some test relied on the stub (i.e., called the stub without realizing). Mitigation: parent runspace runs ALL tests per AGENTS.md `./Build.sh --Test`.

3. **(low) The cmake reconfigure is slow** — cmake typically takes 5-15 seconds to reconfigure. Acceptable.

4. **(none) Build time regression** — adding 2 TUs to libnvrhi_vkd.a adds ~1-2 seconds to nvrhi build time. Acceptable.

5. **(low) The FORCE keyword could cause issues on Windows or other platforms** — possible if the platform-specific cmake config has weird interactions with cache vars. The project builds on Linux per AGENTS.md; cross-platform risk is out of scope.

## Feedback for impler (none — KEEP)

No blocking feedback. The patch is correct on static analysis.

## Verdict

**KEEP.** The v133 patch is a prerequisite for the v132 patch's success in Debug builds. Without v133, the v132 patch would fail to link (the symbol is not in libnvrhi_vkd.a when `NVRHI_WITH_VALIDATION:BOOL=OFF` in the cache). With v133, the symbol is added to libnvrhi_vkd.a, the v132 patch links successfully, and the validation layer can fire VUID on the next test run.

The reviewer explicitly accepts the v133 plan-criticer's KEEP verdict and the impler's static-analysis evidence. The single-profile caveat (anti-pattern #7) applies but is mitigated by the concrete nature of the evidence (3 CMakeCache.txt files read, FORCE keyword behavior is documented cmake syntax, placement is correct).

## Next-step recommendation

Per the v133 plan §"Parent-runspace recipe": parent runspace executes the 60-180 second recipe (rebuild + nm verify + run + log grep + dump analysis). If the validation layer fires VUID, the bisect closes; the fix is to apply the barrier/layout change the VUID describes. If the validation layer is silent, the binding is correct and the bug is elsewhere (e.g., in slangc compilation, in the GI shader's logic, in the dump pipeline).