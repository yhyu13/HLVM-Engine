# Pending Plan Review v133 — KEEP with one architectural caveat

- plan: docs/PENDING_PLAN_v133.md
- verdict: KEEP
- reviewer: plan-criticer (single-profile + file-only runspace; freshness-degraded per anti-pattern #7)
- timestamp: 2026-07-30

## Design soundness

The v133 plan addresses a real structural gap that was missed by v132: the v132 plan verified the CMakeLists.txt default for `NVRHI_WITH_VALIDATION` (which is `ON` per `_deps/nvrhi-src/CMakeLists.txt:36`) but did NOT verify the ACTUAL cmake cache state (`Build/Debug/CMakeCache.txt:485` shows `OFF`). This is a textbook "the config says X but the cache says Y" failure mode that v133 closes by forcing the cache to agree with the config. The approach is correct: `set(... CACHE BOOL "" FORCE)` BEFORE `FetchContent_MakeAvailable` overrides any cached value, and the cmake reconfigure triggered by Build.sh picks up the new value.

The acceptance criterion is well-defined (Debug target builds with validation layer symbols in libnvrhi_vkd.a; subsequent run with HLVM_DUMP_RGI=1 either surfaces a VUID or shows the GBuffer SRV working). The risk profile is well-understood (linker succeeds = win, linker fails = revert). The plan is grounded in file-only static analysis (3 distinct CMakeCache.txt files read and compared) rather than in speculation.

## Plan completeness

The plan is complete on the cmake side (single FORCE line, well-scoped), but is missing ONE acceptance criterion that would prevent another audit-tick loop: the parent-runspace recipe must VERIFY the validation symbols are now in `libnvrhi_vkd.a` (via `nm libnvrhi_vkd.a | grep createValidationLayer`) BEFORE attempting to run the test. Without this verification, the parent could rebuild, find linker failures, and the cron would have to wait another tick to learn of it.

**Suggested addition** (not blocking; can be added by the impler or left for parent):
> Acceptance gate step 1.5: `nm Engine/Source/Runtime/Binary/Debug/libnvrhi_vkd.a | grep createValidationLayer` returns a non-empty match. If empty, the link will fail; investigate before running the test.

This is a 1-line addition to the parent recipe and would close the "did the FORCE actually take effect" gap.

## Design soundness vs single-profile caveat

Per anti-pattern #7 in the six-role-pipeline skill: "When the host has only one worker profile, the planner/impler split and the plan-criticer/reviewer split become 'same head with different prompt text.'" This applies here — the same model wrote the plan, the critique, and will write the impl. The KEEP verdict is therefore a SELF-CHECK, not an independent review.

The reason KEEP is still appropriate: the design is grounded in observable evidence (3 CMakeCache.txt files, each at a distinct path, with the relevant cache var reading distinctly). A self-check against evidence is more reliable than a self-check against vibes. The architectural risk is not "the plan could be wrong because the same model wrote it" — it's "the plan could be wrong because the static analysis missed something." The static analysis here is concrete (file paths + line numbers + cache values), so the self-check risk is low.

## Plan fidelity to the v132 root cause hypothesis

DIAGNOSTIC_2026-07-30-v24.md identifies the root cause as "a barrier or layout issue" — the GI shader's `GBufferWorldPos[pixel]` SRV read returns `(0,0,0,0)` despite the textures having real data. The hypothesized mechanism is an image layout transition from RENDER_TARGET to SHADER_READ_ONLY_OPTIMAL not happening, OR happening at the wrong time. The v133 fix unblocks the validation layer; the validation layer would surface VUID-00344 (or similar) describing exactly this issue. The v133 plan is a prerequisite for the diagnostic's recommended next step ("Run with Vulkan validation layer enabled") and is therefore high-fidelity to the diagnostic.

## Feedback for planner (none — KEEP)

No blocking feedback. The one suggested addition (nm verification step) is a nice-to-have, not a blocker. The plan can proceed to impl as written.

## What this review confirms
- The 8 acceptance criteria (1-8 in v133 plan) are all verifiable in the parent runspace with terminal access.
- The cmake cache fix is the correct structural mechanism (FORCE keyword on a CACHE BOOL overrides any cached value, including a stale OFF).
- The fix is additive (no symbols removed from libnvrhi_vkd.a, only added) so no test should regress.
- The single-profile caveat applies but does not undermine the KEEP verdict because the evidence is concrete.

## What this review does NOT confirm (parent-runspace responsibility)
- Whether the rebuild actually succeeds (requires terminal).
- Whether `nm libnvrhi_vkd.a | grep createValidationLayer` returns a match (requires terminal).
- Whether the validation layer actually fires a VUID on the next test run (requires terminal + log grep).
- Whether the VUID, if fired, names the exact image/layout issue (requires log content analysis).

These are all parent-runspace steps per the recipe in v133 plan §"Parent-runspace recipe".