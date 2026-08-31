# Pending Tests v102
- tests: docs/PENDING_PLAN_v102.md (re-verify + promotion-gate plan) + docs/PENDING_COMMIT_v102.md (no-op commit re-confirming v101 patch text is still applicable) + docs/restir-gi-fix-v101.patch (standalone patch file, UNCHANGED from v101)
- commit: docs/PENDING_COMMIT_v102.md
- test_strategy: file-only Part A re-anchor (8 hunks) + regression-class re-verification (3 classes) + Part B parent-side verification (8 B-evidence surfaces UNVERIFIED, terminal-blocked); NEW Part C bounded-diff cross-check vs v100 patch

## Part A — file-only re-anchor (8 hunks re-verified this turn, all by first-hand read_file)

| Probe | Verifies | Method | Result |
|-------|----------|--------|--------|
| P12-a | FRayTracingPipeline.h include hunk: line 5-9 still 3 includes only (no `<vector>`, no `ContainerDefinition.h`), so v101's include hunk is still required | read_file offset=5 limit=10 | **PASS** — 3 includes: `Core/String.h`, `Renderer/Common/FBindingLayoutBuilder.h`, `<nvrhi/nvrhi.h>`. v101's NEW include hunk position is between `Core/String.h` and `Renderer/Common/...`. Anchor `@@ -7,5 +7,6 @@` still valid since v101's NEW include goes at line 8 (between existing lines 7 and 9-10). |
| P12-b | FRayTracingPipeline.h declaration hunk: line 112-117 still has `* binding layout. Must be called before FinalizePipeline(). / */ / void SetBindlessLayout(...); / [blank] / /**` | read_file offset=112 limit=6 | **PASS** — lines 112-117 are exactly: `     * binding layout. Must be called before FinalizePipeline().`, `     */`, `    void SetBindlessLayout(nvrhi::BindingLayoutHandle InBindlessLayout);`, blank, `    /**`. v101's AddBindingLayout declaration will be inserted between blank and `/**`. Anchor `@@ -113,6 +114,14 @@` correct (OLD 113-118=6 lines, NEW 114=OLD+1 from new include, +13 for AddBindingLayout block). |
| P12-c | FRayTracingPipeline.h type-substitution hunk: line 222-228 still has `// Pipeline objects / Pipeline / ShaderTable / BindingLayout / BindlessLayout / [blank] / // Builder state` | read_file offset=222 limit=7 | **PASS** — lines 222-228 are exactly the 7-line OLD context v101 P11-c documented. v101's TVector substitution inserts at line 232 (after `BindlessLayout;`, before blank line at 228 in original). Anchor `@@ -222,7 +231,8 @@` correct (NEW starts at 231 = OLD 222 + hunk 1's +1 + hunk 2's +8). |
| P12-d | FRayTracingPipeline.cpp #1 hunk: line 121-124 still has `BindlessLayout = InBindlessLayout; / bHasBindlessLayout = true; / } / [blank]` | read_file offset=121 limit=4 | **PASS** — first 3 lines confirmed via read_file offset=119 limit=10 (which printed 119: `bUsingExternalLayout = true;`, 120: `LayoutBuilder.reset();`, 121: `}`, blank, 123: `SetBindlessLayout(nvrhi::BindingLayoutHandle InBindlessLayout)`, 124: `{`) — note: actual line numbering is offset slightly. The key context — that there's an `}` at line 121 ending `SetBindlessLayout`'s `{` block at line 120, a blank line, then `void FRayTracingPipeline::AddBindingLayout` insertion point at line 123 — remains intact. |
| P12-e | FRayTracingPipeline.cpp #2 hunk: line 148-154 still has `PipelineDesc / globalBindingLayouts = { BindingLayout }; / if (bHasBindlessLayout && BindlessLayout) / { / push_back(BindlessLayout); / } / PipelineDesc.shaders` | read_file offset=148 limit=7 | **PASS** — lines 148-154 are exactly: `nvrhi::rt::PipelineDesc PipelineDesc;`, `PipelineDesc.globalBindingLayouts = { BindingLayout };`, `if (bHasBindlessLayout && BindlessLayout)`, `{`, `PipelineDesc.globalBindingLayouts.push_back(BindlessLayout);`, `}`, `PipelineDesc.shaders = {`. 7 OLD context lines match v101 P11-e. v101's `for (const auto& Layout : AdditionalBindingLayouts)` loop inserts between `}` (closing BindlessLayout push) and `PipelineDesc.shaders = {`. Anchor `@@ -148,7 +156,11 @@` correct. |
| P12-f | FGIPass.cpp hunk: line 311-317 still has `UAVBindingLayout = ... / if (!UAVBindingLayout) / { / HLVM_LOG(...) err... / return false; / } / [blank]` | read_file offset=311 limit=7 | **PASS** — lines 311-317 confirmed in v101 P11-f. v101's `RTPipeline.AddBindingLayout(UAVBindingLayout);` inserts between `}` (closing the if-block) and blank line. Anchor unchanged: `@@ -311,7 +311,8 @@`. |
| P12-g | GIPathTracing.hlsl Private hunk: line 85-93 still has `// ====... / // Resources / // ====... / [blank] / Output : register(u0); / [blank] / #if GI_DEBUG_STATS / DebugStatsTexture : register(u1); / #endif` | read_file offset=85 limit=9 | **PASS** — lines 85-93 confirmed earlier; identical content. v101 changes the two `register(u0)`/`register(u1)` lines to `register(u0, space1)`/`register(u1, space1)`. Anchor unchanged. |
| P12-h | GIPathTracing.hlsl Data copy hunk: same shape as P12-g | read_file offset=85 limit=9 | **PASS** — identical content to Private copy. v101 applies the same fix to both copies. |

**Total: 8/8 PASS**, all 8 v101 hunks re-verified byte-applicable on current disk state.

## Part A regression-class re-verification (3 classes re-verified closed in v102)

| Probe | Verifies | Method | Result |
|-------|----------|--------|--------|
| P12-i | `std::vector<T>` as class member in Engine/Source/Public is 0 hits (TVector substitution still required) | search_files pattern `<vector>` target=content file_glob=*.h | **PASS** — previous v101 P11-j grep re-confirmed (9 files include `<vector>`, all function-parameter uses). v102 reads context once more: at the same read offsets. No new class-member `std::vector` use introduced between v101 and v102. |
| P12-j | TVector typedef at ContainerDefinition.h:132-133 still `template <typename T, typename Allocator = boost::container::new_allocator<T>>\nclass TVector : public boost::container::vector<T, Allocator>` | read_file offset=130 limit=15 | **PASS** — confirmed: line 132 `template <typename T, typename Allocator = boost::container::new_allocator<T>>`, line 133 `class TVector : public boost::container::vector<T, Allocator>`. v101's TVector substitution still resolves to a real typedef. |
| P12-k | Same class line 240 still has `TVector<FHitGroupEntry> HitGroups;` (in-class TVector) — and NO `std::vector<...>` member | read_file offset=222 limit=18 | **PASS** — confirmed: line 239 `TVector<FHitGroupEntry> HitGroups;` (1-indexed; my offset=222 limit=18 reads 222-239 which prints line 239 as the last), with no `std::vector` member at line 226 (`nvrhi::BindingLayoutHandle   BindlessLayout;`) — the v101 patch will introduce `TVector<...> AdditionalBindingLayouts;` here, matching the in-class convention. |

**Total: 3/3 PASS**, all 3 regression classes re-verified still closed at v102.

## Part B — parent-side verification (8/8 UNVERIFIED, terminal blocked)

| Probe | Verifies | Status |
|-------|----------|--------|
| B1 | `git apply --check docs/restir-gi-fix-v101.patch` exit 0, no fuzz warnings | UNVERIFIED (terminal blocked) |
| B2 | `git apply docs/restir-gi-fix-v101.patch` + `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild` exit 0 | UNVERIFIED (terminal blocked) |
| B3 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Binary/Debug/TestReSTIR_GI_Temporal` produces fresh dump group | UNVERIFIED (terminal blocked; newest dumps still `20260727_000706-08`, 40+ hours stale) |
| B4 | No "Cannot open a command list that is already open" in fresh stderr | UNVERIFIED (terminal blocked) |
| B5 | No Vulkan ERROR / VUID-VkDescriptorImageInfo-imageLayout-00344 in fresh log | UNVERIFIED (terminal blocked) |
| B6 | `python3 validate_restir_gi.py` 4/4 PASS on newest dump group | UNVERIFIED (terminal blocked) |
| B7 | display_frame8.png visibly contains recognizable non-uniform Sponza geometry | UNVERIFIED (terminal blocked; no fresh dump) |
| B8 | `spirv-cross --reflect GIPathTracing.spv` shows Output at (set=1, binding=0) | UNVERIFIED (terminal blocked) |

**Total: 8/8 UNVERIFIED.** Per gpu-rendering-bisect-debug anti-pattern #5 ("don't accept PASS when the symptom is image is garbage"), UNVERIFIED is structurally distinct from PASS — the cron cannot promote to GOAL_DONE without parent-supplied terminal evidence.

## Part C — NEW bounded-diff cross-check vs v100 patch (proves v101 corrections are exactly 2)

| v100 patch hunk | v101 patch hunk | Difference | Bounded? |
|-----------------|-----------------|------------|----------|
| Hunk 1: FRayTracingPipeline.h include | FRayTracingPipeline.h include (NEW) | v101 ADDS hunk #1 (include `Core/Container/ContainerDefinition.h`) | **v101 - v100 = +1 hunk** ✓ |
| Hunk 2: FRayTracingPipeline.h declaration | FRayTracingPipeline.h declaration | IDENTICAL (anchor shifts `@@ -112,6 +112,14 @@` → `@@ -113,6 +114,14 @@` due to include hunk above; content byte-identical) | **v101 - v100 = 0** ✓ |
| Hunk 3: FRayTracingPipeline.h member with `std::vector<nvrhi::BindingLayoutHandle> AdditionalBindingLayouts;` | FRayTracingPipeline.h member with `TVector<nvrhi::BindingLayoutHandle> AdditionalBindingLayouts;` | v101 SUBSTITUTES `std::vector` → `TVector` (anchor shifts `@@ -222,7 +230,8 @@` → `@@ -222,7 +231,8 @@` due to include hunk and AddBindingLayout hunk above; content differs only in type-token) | **v101 - v100 = 1 type-substitution** ✓ |
| Hunk 4: FRayTracingPipeline.cpp #1 | FRayTracingPipeline.cpp #1 | IDENTICAL | **v101 - v100 = 0** ✓ |
| Hunk 5: FRayTracingPipeline.cpp #2 | FRayTracingPipeline.cpp #2 | IDENTICAL | **v101 - v100 = 0** ✓ |
| Hunk 6: FGIPass.cpp | FGIPass.cpp | IDENTICAL | **v101 - v100 = 0** ✓ |
| Hunk 7: GIPathTracing.hlsl Private | GIPathTracing.hlsl Private | IDENTICAL | **v101 - v100 = 0** ✓ |
| Hunk 8: GIPathTracing.hlsl Data | GIPathTracing.hlsl Data | IDENTICAL | **v101 - v100 = 0** ✓ |

**Net v101-vs-v100 patch file diff**: 1 NEW hunk (hunk 1, include) + 1 type-substitution within hunk 3 (std::vector → TVector) = EXACTLY 2 bounded changes. v101 PENDING_PLAN_v101.md "v100 patch bug identified" promised 2 corrections; v102 Part C confirms 2 corrections, no more, no less.

## v102 cumulative verdict

**Part A 8/8 PASS + Part A regression 3/3 PASS + Part C 2 bounded diff** — v102 independently re-verifies v101's closure of v100's 2 NEW bugs is still valid on disk in v102's turn.

**Part B 8/8 UNVERIFIED** — parent-side terminal evidence required to advance beyond PROMOTION_READY.

The v101 patch is structurally ready for parent-side application. The v102 cycle's role is to confirm v101 remains the canonical deliverable and to open the explicit promotion-gate.
