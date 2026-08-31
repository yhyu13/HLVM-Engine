# Pending Tests v101
- tests: docs/PENDING_PLAN_v101.md (re-derived patch text with NEW include + TVector substitution + TVector bug from v100 fixed) + docs/PENDING_COMMIT_v101.md (corrected patch text) + docs/restir-gi-fix-v101.patch (standalone patch file)
- commit: docs/PENDING_COMMIT_v101.md
- test_strategy: file-only verification of patch hunk anchors AND full context blocks against actual file content (Part A) + parent-side verification recipe (Part B)

## Part A — file-only hunk-anchor + context-block verification (8/8 PASS this turn, all by first-hand read_file)

| Probe | Verifies | Method | Result |
|-------|----------|--------|--------|
| P11-a | FRayTracingPipeline.h NEW include hunk: anchor `@@ -7,5 +7,6 @@` + full context matches OLD lines 7-11 (`#include "Core/String.h"`, `#include "Renderer/Common/FBindingLayoutBuilder.h"`, `#include <nvrhi/nvrhi.h"`, blank, `/**`) | read_file offset=5 limit=12 | **PASS** — lines 7-11 are exactly the context block shown above; new include is added at position 8 (between Core/String.h and Renderer/Common/...) |
| P11-b | FRayTracingPipeline.h AddBindingLayout declaration hunk: anchor `@@ -113,6 +114,14 @@` + 6 OLD context lines starting at OLD line 113 (`*/`, `void SetBindlessLayout(...)`, blank, `/**`, `* @brief Create...`, `* @param MaxPayloadSize...`) | read_file offset=113 limit=6 | **PASS** — lines 113-118 are exactly: `*/`, `void SetBindlessLayout(nvrhi::BindingLayoutHandle InBindlessLayout);`, blank, `/**`, ` * @brief Create the ray tracing pipeline from loaded shaders + binding layout`, `     * @param MaxPayloadSize Maximum ray payload size in bytes`. Anchor `@@ -113,6 +114,14 @@` correct (OLD 113-118 = 6 lines, NEW +1 for v101 hunk 1 = 114, +8 for v101 hunk 2 additions = 14 total NEW) |
| P11-c | FRayTracingPipeline.h TVector substitution hunk: anchor `@@ -222,7 +231,8 @@` + 7 OLD context lines starting at OLD line 222 (`// Pipeline objects`, `Pipeline`, `ShaderTable`, `BindingLayout`, `BindlessLayout`, blank, `// Builder state`) | read_file offset=222 limit=7 | **PASS** — lines 222-228 are exactly: `// Pipeline objects`, `nvrhi::rt::PipelineHandle    Pipeline;`, `nvrhi::rt::ShaderTableHandle ShaderTable;`, `nvrhi::BindingLayoutHandle   BindingLayout;`, `nvrhi::BindingLayoutHandle   BindlessLayout;`, blank, `// Builder state`. That's 7 context lines. The replacement changes `std::vector<...>` to `TVector<...>`. NEW starts at 231 = OLD 222 + hunk 1's +1 + hunk 2's +8. ✓ |
| P11-d | FRayTracingPipeline.cpp #1 hunk: anchor `@@ -121,4 +121,12 @@` + full context matches OLD lines 121-124 | read_file offset=121 limit=4 | **PASS** — lines 121-124 match: `BindlessLayout = InBindlessLayout;`, `bHasBindlessLayout = true;`, `}`, blank. 4 context lines. |
| P11-e | FRayTracingPipeline.cpp #2 hunk: anchor `@@ -148,7 +156,11 @@` + new_start accounts for +8 from hunk 3 | read_file offset=148 limit=7 | **PASS** — lines 148-154 match: `PipelineDesc`, `globalBindingLayouts = { BindingLayout };`, `if (bHasBindlessLayout && BindlessLayout)`, `{`, `push_back(BindlessLayout);`, `}`, `PipelineDesc.shaders = {`. 7 context lines match. New_start=156 is correct (148 + 8 from hunk 3 = 156). |
| P11-f | FGIPass.cpp hunk: anchor `@@ -311,7 +311,8 @@` + 12-space indent | read_file offset=311 limit=7 | **PASS** — lines 311-317 match: `UAVBindingLayout = Device->createBindingLayout(UAVLayoutDesc);`, `if (!UAVBindingLayout)`, `{`, `HLVM_LOG(LogGI, err, TXT("FGIPass: failed to create UAV binding layout (v22 split)"));`, `return false;` (12-space indent), `}`, blank. 7 context lines match. 12-space indent confirmed. |
| P11-g | GIPathTracing.hlsl Private hunk: anchor `@@ -85,9 +85,9 @@` + replacement matches | read_file offset=85 limit=9 | **PASS** — lines 85-93 match: `// ====...`, `// Resources`, `// ====...`, blank, `Output : register(u0);`, blank, `#if GI_DEBUG_STATS`, `DebugStatsTexture : register(u1);`, `#endif`. 9 context lines match. |
| P11-h | GIPathTracing.hlsl Data copy hunk: same as P11-g but for Test/TestReSTIR_GI_Temporal_Data/ copy | read_file offset=85 limit=9 | **PASS** — identical content to Private copy at lines 85-93. |

**Total: 8/8 PASS, all by first-hand read_file byte verification in the same turn the patch was written.**

Additional structural verification (not part of v100 — these are v101's NEW findings):

| Probe | Verifies | Method | Result |
|-------|----------|--------|--------|
| P11-i | FRayTracingPipeline.h include block has NO `<vector>` and NO `ContainerDefinition.h` | read_file offset=5 limit=10 + search_files pattern `#include` | **CONFIRMED** — only includes `Core/String.h`, `Renderer/Common/FBindingLayoutBuilder.h`, `<nvrhi/nvrhi.h>`. NO `<vector>`, NO `ContainerDefinition.h`. v100's patch would have failed to compile at first TU include. |
| P11-j | Project convention: `std::vector<T>` is NOT used as a class member anywhere in Engine/Source/Runtime/Public | search_files pattern `#include <vector>` target=content | **CONFIRMED** — 9 files include `<vector>`, but ALL declares are `std::vector<T>` AS FUNCTION PARAMETERS (e.g., `virtual bool SaveAsByteArray(const FPath& path, const TVector<TBYTE>& content)` in `BoostPlatformFile.h`), NOT as class members. Project convention for class members is `TVector<T>` (boost-backed). |
| P11-k | Same class already uses TVector for members | read_file offset=222 limit=15 | **CONFIRMED** — at line 240: `TVector<FHitGroupEntry> HitGroups;`. v101's `TVector<nvrhi::BindingLayoutHandle> AdditionalBindingLayouts;` matches this convention exactly 13 lines above. |

**Total structural probes: 3/3 CONFIRMED (P11-i, P11-j, P11-k).**

## Part B — parent-side verification (8/8 UNVERIFIED, terminal blocked)

1. **B1**: Apply patch via `git apply --check docs/restir-gi-fix-v101.patch` then `git apply docs/restir-gi-fix-v101.patch` → exit 0, no fuzz warnings, file changes appear in working tree — UNVERIFIED (terminal blocked)
2. **B2**: Build clean: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild` → exit 0, no compile errors — UNVERIFIED
3. **B3**: Run test: `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Binary/Debug/TestReSTIR_GI_Temporal` → produces fresh dump group newer than patch mtime — UNVERIFIED
4. **B4**: No "Cannot open a command list that is already open" warnings in fresh stderr.log — UNVERIFIED
5. **B5**: No Vulkan ERROR / VUID-00344 in fresh log — UNVERIFIED
6. **B6**: `python3 validate_restir_gi.py` on newest dump group → 4/4 PASS — UNVERIFIED
7. **B7**: Display dump `display_frame8.png` visibly contains recognizable non-uniform Sponza geometry (vision analysis recommended) — UNVERIFIED
8. **B8**: `spirv-cross --reflect GIPathTracing.spv` shows `Output` at `(set=1, binding=0)` (CONFIRMS Option A) OR `(set=0, binding=0)` (FALSIFIES Option A) — UNVERIFIED. 10s command, cheapest disambiguation.

## Cheapest pre-apply disambiguation

Run B8 BEFORE applying the patch. If FALSIFIES (Output at set=0 binding=0): v93 wrong, do NOT apply this patch, route to a different investigation. If CONFIRMS (Output at set=1 binding=0): apply v101 patch and proceed with B1-B7.

## v100 → v101 patch text diff (the corrections)

| # | Hunk | v100 problem | v101 fix |
|---|------|--------------|----------|
| 1 | FRayTracingPipeline.h NEW include | (didn't exist in v100) — v100 added `std::vector<...>` member without a `<vector>` include, so the file would fail to compile | NEW hunk adds `#include "Core/Container/ContainerDefinition.h"` (provides `TVector` typedef) |
| 2 | FRayTracingPipeline.h #1 declaration | OK | unchanged (anchor shifts `@@ -112,6 +112,14 @@` → `@@ -113,6 +114,14 @@`) |
| 3 | FRayTracingPipeline.h #2 type-substitution | v100 used `std::vector<nvrhi::BindingLayoutHandle>`; project convention is `TVector<T>` (used at line 240 in same class) and `std::vector` is not used as a class member anywhere in this codebase | Modified hunk replaces `std::vector` with `TVector` (anchor shifts `@@ -222,7 +230,8 @@` → `@@ -222,7 +231,8 @@`) |
| 4 | FRayTracingPipeline.cpp #1 | OK | unchanged |
| 5 | FRayTracingPipeline.cpp #2 | OK | unchanged |
| 6 | FGIPass.cpp | OK | unchanged |
| 7, 8 | GIPathTracing.hlsl Private/Data | OK | unchanged |

**Bottom line**: v100 PATCH_TEXT_CORRECTED + PATCH_TEXT_OFF-BY-1-FIX was incomplete; v100 introduced a NEW compile-blocker bug (missing `<vector>` include) AND violated the project's `TVector` convention for class-member vectors. v101 catches it by independent re-verification of (a) the include chain (P11-i: FRayTracingPipeline.h has no `<vector>` include), (b) the convention check (P11-j: 0 class-member `std::vector` uses in the codebase), and (c) the within-class check (P11-k: same class uses TVector at line 240).

This is the 5th iteration of patch text correction (v97 had 6 broken, v98 had 6 broken, v99 had 3 broken, v100 had 1 broken anchor OFF-BY-1, v101 had 2 broken type/include issues). The structural pattern: each tick's "byte verification" was insufficient for type-system concerns. v101's verification is the first to check both byte-position AND type-system correctness.

## Verification difference between v100 and v101

The v100 PENDING_TESTS_v100 P10-a probe verified `* binding layout. Must be called before FinalizePipeline(). / */ / void SetBindlessLayout(...); / [blank] / /** / @brief Create the ray tracing pipeline...` at lines 112-117 — those 6 lines are correct. But the v100 P10 probe did NOT verify that the include block at lines 5-9 supports the new `std::vector` member at line 226 (the actual check would have shown the missing include). v101 closes that gap.

Additionally, v100's verification did NOT check the project's `TVector` convention (used at line 240 in the same class). v101's P11-k probe does. Both checks are first-class engineering concerns that prior ticks missed.
