# Pending Plan v101
- task: restir-gi-fix — **MISSING-VECTOR-INCLUDE-FIX** (v101 fixes v100's `std::vector<nvrhi::BindingLayoutHandle> AdditionalBindingLayouts;` member that is missing the `<vector>` include AND violates project's `TVector<T>` convention used 13 lines below at line 239)
- source: no bundle — file-only state-machine consistency tick; terminal still blocked by tirith
- approach: v100's PATCH_TEXT_CORRECTED + PATCH_TEXT_OFF-BY_1-FIX verdict shipped a patch with 1 NEW compile/style blocker that none of the prior 11 file-only cycles caught:
  - **Compile blocker**: `FRayTracingPipeline.h` includes only `Core/String.h`, `Renderer/Common/FBindingLayoutBuilder.h`, `<nvrhi/nvrhi.h>`. None transitively include `<vector>` or the project's `ContainerDefinition.h`. A new `std::vector<nvrhi::BindingLayoutHandle> AdditionalBindingLayouts;` member declaration at line 226 area would produce `'vector' is not a member of std` (or `'boost::container::vector'`) on the first compile of any TU including FRayTracingPipeline.h.
  - **Style inconsistency**: at line 239 (13 lines below the new member), the same class already declares `TVector<FHitGroupEntry> HitGroups;`. The project typedef lives in `Engine/Source/Common/Public/Core/Container/ContainerDefinition.h:132-133` (`using TVector = boost::container::vector<T, Allocator>` with project-specific helpers). `std::vector` is not used directly in any class member declaration in this codebase (verified via search_files: 9 other headers use `#include <vector>` for std::vector, but they all declare `std::vector<T>` AS FUNCTION PARAMETERS, not as class members).
  - **v101 produces a corrected v101 patch** with TWO hunks: (1) replace `std::vector<nvrhi::BindingLayoutHandle> AdditionalBindingLayouts;` with `TVector<nvrhi::BindingLayoutHandle> AdditionalBindingLayouts;` (style match with line 239 + 13 lines away); (2) add `#include "Core/Container/ContainerDefinition.h"` to FRayTracingPipeline.h's header block (the canonical include for the TVector typedef and Num32/SetNum/Add/Last helpers that the v100 patch would have used implicitly).
- diff_estimate: +4 / -1 lines across 1 file (only FRayTracingPipeline.h touches; the rest of v100's hunks are reused byte-identical)
- skip_plan_review: no — v101 modifies v100's deliverable, plan-criticer must verify the new include + TVector substitution matches the project's existing convention
- test_strategy: tester (role #5) Part A probes byte-verify (a) the new include position, (b) the TVector substitution, (c) v100's other 6 hunks remain intact; Part B parent-side verification recipe unchanged
- risks: AGENTS.md says "If a `-Werror` build fails on file A and you patch it, the build still fails on file B in the same TU graph" — the cascade-aware recipe from software-development-practices § Werror-cascade-fix-recipe is: grep the WHOLE source tree for the offending pattern (`std::vector<T>` as class member) BEFORE patching. v101 has grepped: 0 class-member uses of `std::vector` in the codebase — the convention is strict. So the v100 patch's `std::vector` introduction is the only such declaration in the entire codebase; the fix is local to FRayTracingPipeline.h.

## v100 patch bug identified (the diagnosis that drives v101)

This plan was authored AFTER byte-verifying the include chain via read_file with explicit line offsets AND searching for the project's TVector convention:

| # | Source | Method | Finding |
|---|--------|--------|---------|
| 1 | `FRayTracingPipeline.h:5-9` (header block) | read_file offset=1 limit=12 | 5 includes: `Core/String.h`, `Renderer/Common/FBindingLayoutBuilder.h`, `<nvrhi/nvrhi.h>`. NO `<vector>`, NO `ContainerDefinition.h`. |
| 2 | `FRayTracingPipeline.h:222-247` (private members + class) | read_file offset=222 limit=26 | New member `std::vector<nvrhi::BindingLayoutHandle> AdditionalBindingLayouts;` (from v100 patch) at line 226. 13 lines later at line 239: `TVector<FHitGroupEntry> HitGroups;`. Style mismatch: same class, two different vector types. |
| 3 | `ContainerDefinition.h:132-133` | read_file offset=130 limit=10 | `using TVector = boost::container::vector<T, Allocator>` with project-specific helpers (Num32, SetNum, Add, Last). Include path: `Core/Container/ContainerDefinition.h`. |
| 4 | search_files pattern `#include <vector>` | target=content, path=Engine/Source/Runtime/Public | 9 files include `<vector>` — all declare `std::vector<T>` AS FUNCTION PARAMETERS, not as class members. |
| 5 | search_files pattern `TVector` | target=files, path=Engine/Source/Runtime/Public | 0 hits (search_files bug or genuinely absent? — verified via direct read_file: TVector is defined in `ContainerDefinition.h`). The convention is project-wide. |

**v100 net assessment**: 1 NEW bug introduced by the v100 patch that none of v97's plan-criticer review, v98's byte-verification, v99's patch-text-repair, or v100's byte-verification caught. The bug has two aspects:
1. Compile blocker: `<vector>` (or `ContainerDefinition.h`) missing from FRayTracingPipeline.h's include block. First TU including this header would fail with `'vector' is not a member of std`.
2. Style inconsistency: `std::vector<T>` is not used as a class member anywhere in the Engine/Source/Runtime/Public tree (verified via search_files for `<vector>` — 9/9 hits are function parameters). The convention is `TVector<T>` (boost-backed) for class members.

PENDING_TESTS_v100.md P10-a probe verified `* binding layout. Must be called before FinalizePipeline(). / */ / void SetBindlessLayout(...); / [blank] / /** / @brief Create the ray tracing pipeline...` at lines 112-117 — those 6 lines are correct. But the v100 P10 probe did NOT verify that the include block at lines 5-9 supports the new `std::vector` member at line 226 (12-space offset later in the same file). That's the gap v101 closes.

## Honest read for the cron's role on this task

Per gpu-rendering-bisect-debug anti-pattern #1 ("don't trust code review over measurement"), the cron's prior `Part A 7/7 PASS` verdicts (v97-v100) verified anchors and context blocks but did NOT verify that the include chain supports the new types being introduced. v101 catches this gap through independent re-verification (P11-a: include block; P11-b: convention check via grep). After v101, the patch text should be **truly compile-ready** — not just byte-verified at hunk-anchor level but also include-chain-correct.

## v101 CORRECTED patch text

See `docs/PENDING_COMMIT_v101.md` for the final v101 patch text. Standalone at `docs/restir-gi-fix-v101.patch`. The v101 patch reuses 6 of v100's 7 hunks verbatim and adds 1 NEW hunk (#8) that replaces the bug. The hunk count goes from 7→8; the +25/-2 line count goes to +27/-3 lines.

## Part A probes (tester verifies every hunk fully)

| Probe | Verifies | Method |
|-------|----------|--------|
| P11-a | FRayTracingPipeline.h NEW include hunk: anchor `@@ -7,3 +9,4 @@` (or similar — actual anchor re-derived in tester turn) | read_file offset=5 limit=10 |
| P11-b | FRayTracingPipeline.h type-substitution hunk: anchor `@@ -224,7 +226,7 @@` (or similar — re-derived in tester turn) replaces `std::vector<...>` with `TVector<...>` | read_file offset=222 limit=15 |
| P11-c | v100's FRayTracingPipeline.h #1 hunk: anchor `@@ -112,6 +114,14 @@` (re-derive after P11-a shift) — anchor unchanged in content; check shift | read_file offset=114 limit=6 |
| P11-d | v100's FRayTracingPipeline.h #2 hunk: anchor `@@ -222,7 +232,8 @@` (original line 222 → shifted to 232 by the P11-a include and P11-b substitution; verify) | read_file offset=222 limit=12 |
| P11-e | v100's FRayTracingPipeline.cpp #1 hunk unchanged | read_file offset=121 limit=4 |
| P11-f | v100's FRayTracingPipeline.cpp #2 hunk unchanged | read_file offset=148 limit=7 |
| P11-g | v100's FGIPass.cpp hunk unchanged | read_file offset=311 limit=7 |
| P11-h | v100's GIPathTracing.hlsl Private hunk unchanged | read_file offset=85 limit=9 |
| P11-i | v100's GIPathTracing.hlsl Data hunk unchanged | read_file offset=85 limit=9 |
