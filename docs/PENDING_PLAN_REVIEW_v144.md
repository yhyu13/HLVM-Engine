# Pending Plan Review v144
- plan: docs/PENDING_PLAN_v144.md
- verdict: KEEP
- reviewer: plan-criticer (single-profile self-check)
- timestamp: 2026-08-05

## Design soundness

The plan targets a concrete build blocker that is newer and more actionable than the stale pre-v143 runtime artifacts: `rebuild_Debug.log:147-153` reaches the executable link and fails on `createValidationLayer`, while `build.ninja:3944-3960` proves the validation translation unit is present in `libnvrhid.a`. A whole-archive link feature is an appropriate bounded experiment for a static-library member that the normal archive scan/LTO link is not extracting, and it preserves the requested validation wrapper rather than reverting it or disabling validation. The plan also keeps the generated CMake and PyCMake source synchronized and makes the linkage contract regression-testable.

## Plan completeness

Complete: source and generated linkage sites, the focused existing Python test file, exact build/run/validator commands, failure evidence, CMake-version risk, and the required no-fabrication runtime caveat are specified.

## Feasibility check

The project uses CMake 3.29.3 (`Build/Debug/CMakeFiles/3.29.3`) and therefore supports `$<LINK_LIBRARY:WHOLE_ARCHIVE,...>`; the NVRHI target is a static target named `nvrhi` and is already linked through `Runtime`, so no dependency installation or new package is required. The impler must verify the generated expression syntax rather than hand-writing an invalid quoted generator expression.

## Risks checked

- Whole-archiving `nvrhi` may increase link inputs or expose duplicate symbols; the reviewer must inspect scope and preserve the existing link order.
- A static source-contract test cannot prove the linker result; the commit marker must retain the real Debug rebuild as the behavioral gate.
- Terminal and vision remain externally blocked in this session; no runtime PASS may be inferred from existing logs.
- Single-profile caveat: this KEEP is a self-check, not independent fresh-eyes review.

## Feedback for planner (FIX only)

N/A — KEEP.
